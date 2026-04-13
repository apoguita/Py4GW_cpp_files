/**
 * @file py_combat_events.cpp
 * @brief Combat Events System - C++ Implementation
 *
 * This file implements the packet handlers for the Combat Events system.
 * Each handler extracts relevant data from GWCA packets and pushes events
 * to the thread-safe queue for Python to process.
 *
 * Packet Types Handled:
 * ---------------------
 * - SkillActivate: Early notification of skill cast (has skill_id before GenericValue)
 * - GenericValue: Most skill/attack state changes (no target info)
 * - GenericValueTarget: Skill/attack events with target information
 * - GenericFloat: Float values (cast time, knockdown duration, energy spent)
 * - GenericModifier: Damage events (normal, critical, armor-ignoring)
 *
 * Important Notes:
 * ----------------
 * 1. GWCA packet naming is sometimes counter-intuitive:
 *    - GenericValueTarget: packet->target is the CASTER, packet->caster is the TARGET
 *    - GenericModifier: target_id is correct (receiver), cause_id is source
 *
 * 2. The DISABLED event fires 4 times per skill:
 *    - disabled=1: Cast started (can't use other skills)
 *    - disabled=0: Cast finished (brief moment)
 *    - disabled=1: Aftercast started (can't use skills again)
 *    - disabled=0: Aftercast ended (can truly act)
 *
 * 3. Damage values are fractions of max HP, not absolute numbers.
 *    Python multiplies by Agent.GetMaxHealth() to get actual damage.
 *
 * @see py_combat_events.h for event type definitions
 * @see CombatEvents.py for Python-side processing
 */

#include "py_combat_events.h"

namespace py = pybind11;

// Backwards compatibility: create namespace with constexpr aliases to enum class values
// This allows existing code using CombatEventTypes::FOO to continue working
namespace CombatEventTypes {
    constexpr uint32_t SKILL_ACTIVATED = to_uint(CombatEventType::SKILL_ACTIVATED);
    constexpr uint32_t ATTACK_SKILL_ACTIVATED = to_uint(CombatEventType::ATTACK_SKILL_ACTIVATED);
    constexpr uint32_t SKILL_STOPPED = to_uint(CombatEventType::SKILL_STOPPED);
    constexpr uint32_t SKILL_FINISHED = to_uint(CombatEventType::SKILL_FINISHED);
    constexpr uint32_t ATTACK_SKILL_FINISHED = to_uint(CombatEventType::ATTACK_SKILL_FINISHED);
    constexpr uint32_t INTERRUPTED = to_uint(CombatEventType::INTERRUPTED);
    constexpr uint32_t INSTANT_SKILL_ACTIVATED = to_uint(CombatEventType::INSTANT_SKILL_ACTIVATED);
    constexpr uint32_t ATTACK_SKILL_STOPPED = to_uint(CombatEventType::ATTACK_SKILL_STOPPED);
    constexpr uint32_t ATTACK_STARTED = to_uint(CombatEventType::ATTACK_STARTED);
    constexpr uint32_t ATTACK_STOPPED = to_uint(CombatEventType::ATTACK_STOPPED);
    constexpr uint32_t MELEE_ATTACK_FINISHED = to_uint(CombatEventType::MELEE_ATTACK_FINISHED);
    constexpr uint32_t DISABLED = to_uint(CombatEventType::DISABLED);
    constexpr uint32_t KNOCKED_DOWN = to_uint(CombatEventType::KNOCKED_DOWN);
    constexpr uint32_t CASTTIME = to_uint(CombatEventType::CASTTIME);
    constexpr uint32_t DAMAGE = to_uint(CombatEventType::DAMAGE);
    constexpr uint32_t CRITICAL = to_uint(CombatEventType::CRITICAL);
    constexpr uint32_t ARMOR_IGNORING = to_uint(CombatEventType::ARMOR_IGNORING);
    constexpr uint32_t EFFECT_APPLIED = to_uint(CombatEventType::EFFECT_APPLIED);
    constexpr uint32_t EFFECT_REMOVED = to_uint(CombatEventType::EFFECT_REMOVED);
    constexpr uint32_t EFFECT_ON_TARGET = to_uint(CombatEventType::EFFECT_ON_TARGET);
    constexpr uint32_t ENERGY_GAINED = to_uint(CombatEventType::ENERGY_GAINED);
    constexpr uint32_t ENERGY_SPENT = to_uint(CombatEventType::ENERGY_SPENT);
    constexpr uint32_t SKILL_DAMAGE = to_uint(CombatEventType::SKILL_DAMAGE);
    constexpr uint32_t SKILL_ACTIVATE_PACKET = to_uint(CombatEventType::SKILL_ACTIVATE_PACKET);
    constexpr uint32_t SKILL_RECHARGE = to_uint(CombatEventType::SKILL_RECHARGE);
    constexpr uint32_t SKILL_RECHARGED = to_uint(CombatEventType::SKILL_RECHARGED);
}

// ============================================================================
// Lifecycle Implementation
// ============================================================================

void CombatEventQueue::Initialize() {
    if (is_initialized) return;

    // SkillActivate packet - gives us skill_id before GenericValue arrives
    // Useful for correlating skill info when GenericValue doesn't have it
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::SkillActivate>(
        &skill_activate_entry,
        [this](GW::HookStatus*, GW::Packet::StoC::SkillActivate* packet) {
            OnSkillActivate(packet);
        }
    );

    // GenericValue packet - most skill/attack state changes
    // Contains: agent_id, value_id (event type), value (usually skill_id)
    // No target information in this packet type
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(
        &generic_value_entry,
        [this](GW::HookStatus*, GW::Packet::StoC::GenericValue* packet) {
            OnGenericValue(packet);
        }
    );

    // GenericValueTarget packet - skill/attack events WITH target info
    // WARNING: Naming is swapped! packet->target = caster, packet->caster = target
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValueTarget>(
        &generic_value_target_entry,
        [this](GW::HookStatus*, GW::Packet::StoC::GenericValueTarget* packet) {
            OnGenericValueTarget(packet);
        }
    );

    // GenericFloat packet - events with float values
    // Used for: cast time, knockdown duration, energy spent
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericFloat>(
        &generic_float_entry,
        [this](GW::HookStatus*, GW::Packet::StoC::GenericFloat* packet) {
            OnGenericFloat(packet);
        }
    );

    // GenericModifier packet - damage and healing events
    // Contains: target_id (receiver), cause_id (dealer), value (damage as HP fraction)
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericModifier>(
        &generic_modifier_entry,
        [this](GW::HookStatus*, GW::Packet::StoC::GenericModifier* packet) {
            OnGenericModifier(packet);
        }
    );

    // SkillRecharge packet - skill went on cooldown
    // Contains: agent_id, skill_id, skill_instance, recharge (in milliseconds)
    // Works for ANY agent - player, heroes, enemies, NPCs!
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::SkillRecharge>(
        &skill_recharge_entry,
        [this](GW::HookStatus*, GW::Packet::StoC::SkillRecharge* packet) {
            OnSkillRecharge(packet);
        }
    );

    // SkillRecharged packet - skill came off cooldown
    // Contains: agent_id, skill_id, skill_instance
    // Works for ANY agent - player, heroes, enemies, NPCs!
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::SkillRecharged>(
        &skill_recharged_entry,
        [this](GW::HookStatus*, GW::Packet::StoC::SkillRecharged* packet) {
            OnSkillRecharged(packet);
        }
    );

    is_initialized = true;
}

void CombatEventQueue::Terminate() {
    if (!is_initialized) return;

    GW::StoC::RemoveCallback(GW::Packet::StoC::SkillActivate::STATIC_HEADER, &skill_activate_entry);
    GW::StoC::RemoveCallback(GW::Packet::StoC::GenericValue::STATIC_HEADER, &generic_value_entry);
    GW::StoC::RemoveCallback(GW::Packet::StoC::GenericValueTarget::STATIC_HEADER, &generic_value_target_entry);
    GW::StoC::RemoveCallback(GW::Packet::StoC::GenericFloat::STATIC_HEADER, &generic_float_entry);
    GW::StoC::RemoveCallback(GW::Packet::StoC::GenericModifier::STATIC_HEADER, &generic_modifier_entry);
    GW::StoC::RemoveCallback(GW::Packet::StoC::SkillRecharge::STATIC_HEADER, &skill_recharge_entry);
    GW::StoC::RemoveCallback(GW::Packet::StoC::SkillRecharged::STATIC_HEADER, &skill_recharged_entry);

    is_initialized = false;
}

// ============================================================================
// Queue Access
// ============================================================================

std::vector<RawCombatEvent> CombatEventQueue::GetAndClearEvents() {
    std::lock_guard<std::mutex> lock(queue_mutex);
    std::vector<RawCombatEvent> result(event_queue.begin(), event_queue.end());
    event_queue.clear();
    return result;
}

std::vector<RawCombatEvent> CombatEventQueue::PeekEvents() const {
    std::lock_guard<std::mutex> lock(queue_mutex);
    return std::vector<RawCombatEvent>(event_queue.begin(), event_queue.end());
}

size_t CombatEventQueue::GetQueueSize() const {
    std::lock_guard<std::mutex> lock(queue_mutex);
    return event_queue.size();
}

void CombatEventQueue::PushEvent(const RawCombatEvent& event) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    event_queue.push_back(event);
    while (event_queue.size() > max_events) {
        event_queue.pop_front();
    }
}

bool CombatEventQueue::IsMapReady() const {
    auto instance_type = GW::Map::GetInstanceType();
    return GW::Map::GetIsMapLoaded() &&
           instance_type != GW::Constants::InstanceType::Loading;
}

// ============================================================================
// Packet Handlers
// ============================================================================
// Each handler extracts relevant data from the packet and pushes a RawCombatEvent.
// All logic and state tracking is done in Python - we just capture raw data here.

/**
 * @brief Handle SkillActivate packet.
 *
 * This packet arrives BEFORE GenericValue for skill activations.
 * It gives us the skill_id early, which is useful for correlating
 * with other packets that may not have the skill_id.
 */
void CombatEventQueue::OnSkillActivate(GW::Packet::StoC::SkillActivate* packet) {
    if (!IsMapReady()) return;
    uint32_t now = static_cast<uint32_t>(GetTickCount64());
    PushEvent(RawCombatEvent(now, CombatEventTypes::SKILL_ACTIVATE_PACKET,
        packet->agent_id, packet->skill_id, 0, 0.0f));
}

/**
 * @brief Handle GenericValue packet.
 *
 * This packet handles most skill/attack state changes that don't have target info:
 * - skill_activated, attack_skill_activated: Skill cast started
 * - skill_finished, attack_skill_finished: Skill completed
 * - skill_stopped, attack_skill_stopped: Skill cancelled (moved, etc.)
 * - interrupted: Skill was interrupted
 * - instant_skill_activated: Instant-cast skill (no cast time)
 * - attack_stopped, melee_attack_finished: Auto-attack states
 * - disabled: Agent can/can't act (cast-lock and aftercast)
 * - add_effect, remove_effect: Visual effects (internal IDs)
 * - skill_damage: Pre-notification that a skill will deal damage
 * - energygain: Energy gained
 */
void CombatEventQueue::OnGenericValue(GW::Packet::StoC::GenericValue* packet) {
    if (!IsMapReady()) return;
    uint32_t now = static_cast<uint32_t>(GetTickCount64());

    using namespace GW::Packet::StoC::GenericValueID;

    switch (packet->value_id) {
        case skill_activated:
            PushEvent(RawCombatEvent(now, CombatEventTypes::SKILL_ACTIVATED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case attack_skill_activated:
            PushEvent(RawCombatEvent(now, CombatEventTypes::ATTACK_SKILL_ACTIVATED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case skill_stopped:
            PushEvent(RawCombatEvent(now, CombatEventTypes::SKILL_STOPPED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case skill_finished:
            PushEvent(RawCombatEvent(now, CombatEventTypes::SKILL_FINISHED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case attack_skill_finished:
            PushEvent(RawCombatEvent(now, CombatEventTypes::ATTACK_SKILL_FINISHED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case interrupted:
            PushEvent(RawCombatEvent(now, CombatEventTypes::INTERRUPTED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case instant_skill_activated:
            PushEvent(RawCombatEvent(now, CombatEventTypes::INSTANT_SKILL_ACTIVATED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case attack_skill_stopped:
            PushEvent(RawCombatEvent(now, CombatEventTypes::ATTACK_SKILL_STOPPED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case attack_stopped:
            PushEvent(RawCombatEvent(now, CombatEventTypes::ATTACK_STOPPED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case melee_attack_finished:
            PushEvent(RawCombatEvent(now, CombatEventTypes::MELEE_ATTACK_FINISHED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case disabled:
            // Aftercast: value=1 means disabled (in aftercast), value=0 means can act
            PushEvent(RawCombatEvent(now, CombatEventTypes::DISABLED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case add_effect:
            PushEvent(RawCombatEvent(now, CombatEventTypes::EFFECT_APPLIED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case remove_effect:
            PushEvent(RawCombatEvent(now, CombatEventTypes::EFFECT_REMOVED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case skill_damage:
            // Track which skill caused upcoming damage
            PushEvent(RawCombatEvent(now, CombatEventTypes::SKILL_DAMAGE,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case energygain:
            PushEvent(RawCombatEvent(now, CombatEventTypes::ENERGY_GAINED,
                packet->agent_id, 0, 0, static_cast<float>(packet->value)));
            break;
    }
}

/**
 * @brief Handle GenericValueTarget packet.
 *
 * This packet is for skill/attack events that include target information.
 *
 * IMPORTANT: GWCA naming is SWAPPED for most events in this packet!
 * - packet->target = the actual CASTER (who does the action)
 * - packet->caster = the actual TARGET (who receives the action)
 *
 * EXCEPTION: effect_on_target uses normal naming:
 * - packet->caster = who applies the effect
 * - packet->target = who receives the effect
 *
 * We normalize this so Python always sees:
 * - agent_id = caster/source
 * - target_id = target/victim
 */
void CombatEventQueue::OnGenericValueTarget(GW::Packet::StoC::GenericValueTarget* packet) {
    if (!IsMapReady()) return;
    uint32_t now = static_cast<uint32_t>(GetTickCount64());

    using namespace GW::Packet::StoC::GenericValueID;

    // Fix the swapped naming (see docstring above)
    uint32_t actual_caster = packet->target;  // Yes, this is intentionally swapped
    uint32_t actual_target = packet->caster;  // Yes, this is intentionally swapped

    switch (packet->Value_id) {
        case skill_activated:
            // Skill cast with target - agent_id=caster, target_id=target
            PushEvent(RawCombatEvent(now, CombatEventTypes::SKILL_ACTIVATED,
                actual_caster, packet->value, actual_target, 0.0f));
            break;

        case attack_skill_activated:
            // Attack skill with target - agent_id=attacker, target_id=target
            PushEvent(RawCombatEvent(now, CombatEventTypes::ATTACK_SKILL_ACTIVATED,
                actual_caster, packet->value, actual_target, 0.0f));
            break;

        case attack_started:
            // Auto-attack started - agent_id=attacker, target_id=target
            PushEvent(RawCombatEvent(now, CombatEventTypes::ATTACK_STARTED,
                actual_caster, 0, actual_target, 0.0f));
            break;

        case effect_on_target:
            // Effect applied to target - uses NORMAL naming (not swapped!)
            // agent_id=caster, value=effect_id, target_id=target
            // Note: effect_id is NOT the skill_id - Python correlates to find skill_id
            PushEvent(RawCombatEvent(now, CombatEventTypes::EFFECT_ON_TARGET,
                packet->caster, packet->value, packet->target, 0.0f));
            break;
    }
}

/**
 * @brief Handle GenericFloat packet.
 *
 * This packet is for events with float values:
 * - knocked_down: Agent knocked down, float_value = duration in seconds
 * - casttime: Cast time modifier received, float_value = duration in seconds
 * - energy_spent: Energy consumed, float_value = energy as fraction of max
 */
void CombatEventQueue::OnGenericFloat(GW::Packet::StoC::GenericFloat* packet) {
    if (!IsMapReady()) return;
    uint32_t now = static_cast<uint32_t>(GetTickCount64());

    using namespace GW::Packet::StoC::GenericValueID;

    switch (packet->type) {
        case knocked_down:
            // Knockdown - float_value is duration in seconds
            PushEvent(RawCombatEvent(now, CombatEventTypes::KNOCKED_DOWN,
                packet->agent_id, 0, 0, packet->value));
            break;

        case casttime:
            // Cast time - float_value is the cast duration in seconds
            // This tells us how long the current cast will take
            PushEvent(RawCombatEvent(now, CombatEventTypes::CASTTIME,
                packet->agent_id, 0, 0, packet->value));
            break;

        case energy_spent:
            // Energy spent - float_value is fraction of max energy (0.0-1.0)
            PushEvent(RawCombatEvent(now, CombatEventTypes::ENERGY_SPENT,
                packet->agent_id, 0, 0, packet->value));
            break;
    }
}

/**
 * @brief Handle GenericModifier packet.
 *
 * This packet is for damage and healing events:
 * - damage: Normal physical/elemental damage
 * - critical: Critical hit damage (same as damage, just flagged)
 * - armorignoring: Armor-ignoring damage (life steal, holy, etc.)
 *                  Can be NEGATIVE for heals!
 *
 * The float value is damage as a FRACTION of target's max HP, not absolute.
 * Python multiplies by Agent.GetMaxHealth(target_id) to get actual damage.
 *
 * Example: float_value = 0.15 on a target with 480 HP = 72 damage
 */
void CombatEventQueue::OnGenericModifier(GW::Packet::StoC::GenericModifier* packet) {
    if (!IsMapReady()) return;
    uint32_t now = static_cast<uint32_t>(GetTickCount64());

    using namespace GW::Packet::StoC::GenericValueID;

    // Extract fields - naming is intuitive here (unlike GenericValueTarget)
    uint32_t target_id = packet->target_id;  // Who receives damage
    uint32_t source_id = packet->cause_id;   // Who deals damage
    float value = packet->value;              // Damage as fraction of max HP

    switch (packet->type) {
        case damage:
            // Normal damage - agent_id=target, target_id=source, float=damage%
            PushEvent(RawCombatEvent(now, CombatEventTypes::DAMAGE,
                target_id, 0, source_id, value));
            break;

        case critical:
            // Critical hit - same structure as damage
            PushEvent(RawCombatEvent(now, CombatEventTypes::CRITICAL,
                target_id, 0, source_id, value));
            break;

        case armorignoring:
            // Armor-ignoring damage (or heal if negative)
            PushEvent(RawCombatEvent(now, CombatEventTypes::ARMOR_IGNORING,
                target_id, 0, source_id, value));
            break;
    }
}

/**
 * @brief Handle SkillRecharge packet.
 *
 * This packet fires when a skill goes on cooldown.
 * Works for ANY agent - player, heroes, enemies, NPCs!
 *
 * Great for:
 * - Building enemy skillbars over time
 * - Tracking enemy/ally skill cooldowns
 * - Knowing when enemies can use dangerous skills again
 *
 * Fields:
 * - agent_id: The agent whose skill is recharging
 * - skill_id: The skill that went on cooldown
 * - recharge: Cooldown duration in milliseconds
 */
void CombatEventQueue::OnSkillRecharge(GW::Packet::StoC::SkillRecharge* packet) {
    if (!IsMapReady()) return;
    uint32_t now = static_cast<uint32_t>(GetTickCount64());
    // agent_id=who, value=skill_id, float_value=recharge_ms
    PushEvent(RawCombatEvent(now, CombatEventTypes::SKILL_RECHARGE,
        packet->agent_id, packet->skill_id, 0, static_cast<float>(packet->recharge)));
}

/**
 * @brief Handle SkillRecharged packet.
 *
 * This packet fires when a skill comes off cooldown.
 * Works for ANY agent - player, heroes, enemies, NPCs!
 *
 * Great for:
 * - Knowing when enemies can use skills again
 * - Triggering actions when your own skills are ready
 * - Updating tracked enemy skillbar states
 *
 * Fields:
 * - agent_id: The agent whose skill finished recharging
 * - skill_id: The skill that is now ready to use
 */
void CombatEventQueue::OnSkillRecharged(GW::Packet::StoC::SkillRecharged* packet) {
    if (!IsMapReady()) return;
    uint32_t now = static_cast<uint32_t>(GetTickCount64());
    // agent_id=who, value=skill_id
    PushEvent(RawCombatEvent(now, CombatEventTypes::SKILL_RECHARGED,
        packet->agent_id, packet->skill_id, 0, 0.0f));
}

// ============================================================================
// Python Bindings
// ============================================================================
// These bindings expose the CombatEventQueue class and related types to Python.
// The module is named "PyCombatEvents" and can be imported directly in Python.

PYBIND11_EMBEDDED_MODULE(PyCombatEvents, m) {
    m.doc() = R"doc(
Combat Events C++ Module for Py4GW

This module provides low-level access to Guild Wars combat packets.
It captures events into a thread-safe queue that Python polls each frame.

Quick Start:
    import PyCombatEvents

    # Get the global queue (singleton)
    queue = PyCombatEvents.GetCombatEventQueue()

    # Initialize packet hooks (call once at startup)
    queue.Initialize()

    # Each frame, get and process events
    events = queue.GetAndClearEvents()
    for event in events:
        print(f"Event type {event.event_type} from agent {event.agent_id}")

    # Cleanup (call at shutdown)
    queue.Terminate()

Event Types:
    Access via PyCombatEvents.EventType.SKILL_ACTIVATED, etc.
    See py_combat_events.h for full documentation of each event type.

Note:
    For high-level usage, prefer CombatEvents.py which wraps this module
    with state tracking, callbacks, and helper functions.
)doc";

    // Event type constants
    py::module_ types = m.def_submodule("PyEventType", "Combat event type constants");
    types.attr("SKILL_ACTIVATED") = CombatEventTypes::SKILL_ACTIVATED;
    types.attr("ATTACK_SKILL_ACTIVATED") = CombatEventTypes::ATTACK_SKILL_ACTIVATED;
    types.attr("SKILL_STOPPED") = CombatEventTypes::SKILL_STOPPED;
    types.attr("SKILL_FINISHED") = CombatEventTypes::SKILL_FINISHED;
    types.attr("ATTACK_SKILL_FINISHED") = CombatEventTypes::ATTACK_SKILL_FINISHED;
    types.attr("INTERRUPTED") = CombatEventTypes::INTERRUPTED;
    types.attr("INSTANT_SKILL_ACTIVATED") = CombatEventTypes::INSTANT_SKILL_ACTIVATED;
    types.attr("ATTACK_SKILL_STOPPED") = CombatEventTypes::ATTACK_SKILL_STOPPED;
    types.attr("ATTACK_STARTED") = CombatEventTypes::ATTACK_STARTED;
    types.attr("ATTACK_STOPPED") = CombatEventTypes::ATTACK_STOPPED;
    types.attr("MELEE_ATTACK_FINISHED") = CombatEventTypes::MELEE_ATTACK_FINISHED;
    types.attr("DISABLED") = CombatEventTypes::DISABLED;
    types.attr("KNOCKED_DOWN") = CombatEventTypes::KNOCKED_DOWN;
    types.attr("CASTTIME") = CombatEventTypes::CASTTIME;
    types.attr("DAMAGE") = CombatEventTypes::DAMAGE;
    types.attr("CRITICAL") = CombatEventTypes::CRITICAL;
    types.attr("ARMOR_IGNORING") = CombatEventTypes::ARMOR_IGNORING;
    types.attr("EFFECT_APPLIED") = CombatEventTypes::EFFECT_APPLIED;
    types.attr("EFFECT_REMOVED") = CombatEventTypes::EFFECT_REMOVED;
    types.attr("EFFECT_ON_TARGET") = CombatEventTypes::EFFECT_ON_TARGET;
    types.attr("ENERGY_GAINED") = CombatEventTypes::ENERGY_GAINED;
    types.attr("ENERGY_SPENT") = CombatEventTypes::ENERGY_SPENT;
    types.attr("SKILL_DAMAGE") = CombatEventTypes::SKILL_DAMAGE;
    types.attr("SKILL_ACTIVATE_PACKET") = CombatEventTypes::SKILL_ACTIVATE_PACKET;
    types.attr("SKILL_RECHARGE") = CombatEventTypes::SKILL_RECHARGE;
    types.attr("SKILL_RECHARGED") = CombatEventTypes::SKILL_RECHARGED;

    // RawCombatEvent struct
    py::class_<RawCombatEvent>(m, "PyRawCombatEvent")
        .def(py::init<>())
        .def_readonly("timestamp", &RawCombatEvent::timestamp)
        .def_readonly("event_type", &RawCombatEvent::event_type)
        .def_readonly("agent_id", &RawCombatEvent::agent_id)
        .def_readonly("value", &RawCombatEvent::value)
        .def_readonly("target_id", &RawCombatEvent::target_id)
        .def_readonly("float_value", &RawCombatEvent::float_value)
        .def("__repr__", [](const RawCombatEvent& e) {
            return "<RawCombatEvent type=" + std::to_string(e.event_type) +
                   " agent=" + std::to_string(e.agent_id) +
                   " value=" + std::to_string(e.value) +
                   " target=" + std::to_string(e.target_id) +
                   " float=" + std::to_string(e.float_value) + ">";
        })
        .def("as_tuple", [](const RawCombatEvent& e) {
            return py::make_tuple(e.timestamp, e.event_type, e.agent_id,
                                  e.value, e.target_id, e.float_value);
        });

    // CombatEventQueue class
    py::class_<CombatEventQueue>(m, "PyCombatEventQueue")
        .def(py::init<>())
        .def("Initialize", &CombatEventQueue::Initialize,
            "Initialize packet callbacks (call once at startup)")
        .def("Terminate", &CombatEventQueue::Terminate,
            "Remove packet callbacks")
        .def("GetAndClearEvents", &CombatEventQueue::GetAndClearEvents,
            "Get all queued events and clear the queue (call each frame)")
        .def("PeekEvents", &CombatEventQueue::PeekEvents,
            "Get events without clearing (for debugging)")
        .def("SetMaxEvents", &CombatEventQueue::SetMaxEvents, py::arg("count"),
            "Set maximum events to queue before dropping old ones")
        .def("GetMaxEvents", &CombatEventQueue::GetMaxEvents,
            "Get maximum event queue size")
        .def("IsInitialized", &CombatEventQueue::IsInitialized,
            "Check if packet callbacks are registered")
        .def("GetQueueSize", &CombatEventQueue::GetQueueSize,
            "Get current number of queued events");

    // Global singleton getter
    m.def("GetCombatEventQueue", &GetCombatEventQueue, py::return_value_policy::reference,
        "Get the global CombatEventQueue singleton instance");
}
