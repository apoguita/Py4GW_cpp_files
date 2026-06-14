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
#include "FrameClock.h"

#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/AgentMgr.h>

namespace py = pybind11;

// Snapshot a live agent's max_hp/max_energy inside a packet handler. Safe
// because StoC dispatch runs on the game thread with the agent table held
// stable. Returns zero-initialized snapshot if agent_id=0, agent not found,
// or not living.
static AgentSnapshot SnapshotAgentLiving(uint32_t agent_id) {
    AgentSnapshot snap;
    if (!agent_id) return snap;
    GW::Agent* agent = GW::Agents::GetAgentByID(agent_id);
    if (!agent) return snap;
    if (!agent->GetIsLivingType()) return snap;
    GW::AgentLiving* living = agent->GetAsAgentLiving();
    if (!living) return snap;
    snap.max_hp = static_cast<uint32_t>(living->max_hp);
    snap.max_energy = static_cast<uint32_t>(living->max_energy);
    return snap;
}

// Build a RawCombatEvent with agent_id and target_id snapshots populated.
// Centralises the snapshot logic so every PushEvent call site picks it up.
static RawCombatEvent MakeEvent(uint32_t ts, uint32_t type, uint32_t agent,
                                 uint32_t val, uint32_t target, float fval) {
    RawCombatEvent e(ts, type, agent, val, target, fval);
    AgentSnapshot a_snap = SnapshotAgentLiving(agent);
    e.agent_max_hp = a_snap.max_hp;
    e.agent_max_energy = a_snap.max_energy;
    if (target) {
        AgentSnapshot t_snap = SnapshotAgentLiving(target);
        e.target_max_hp = t_snap.max_hp;
        e.target_max_energy = t_snap.max_energy;
    }
    return e;
}

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
    constexpr uint32_t HEALING = to_uint(CombatEventType::HEALING);
    constexpr uint32_t CURRENT_HEALTH = to_uint(CombatEventType::CURRENT_HEALTH);
    constexpr uint32_t CURRENT_ENERGY = to_uint(CombatEventType::CURRENT_ENERGY);
    constexpr uint32_t HEALTH_REGEN_CHANGE = to_uint(CombatEventType::HEALTH_REGEN_CHANGE);
    constexpr uint32_t ENERGY_REGEN_CHANGE = to_uint(CombatEventType::ENERGY_REGEN_CHANGE);
    constexpr uint32_t REACHED_MAXHP = to_uint(CombatEventType::REACHED_MAXHP);
    constexpr uint32_t EFFECT_APPLIED = to_uint(CombatEventType::EFFECT_APPLIED);
    constexpr uint32_t EFFECT_REMOVED = to_uint(CombatEventType::EFFECT_REMOVED);
    constexpr uint32_t EFFECT_ON_TARGET = to_uint(CombatEventType::EFFECT_ON_TARGET);
    constexpr uint32_t EFFECT_RENEWED = to_uint(CombatEventType::EFFECT_RENEWED);
    constexpr uint32_t ENERGY_GAINED = to_uint(CombatEventType::ENERGY_GAINED);
    constexpr uint32_t ENERGY_SPENT = to_uint(CombatEventType::ENERGY_SPENT);
    constexpr uint32_t SKILL_DAMAGE = to_uint(CombatEventType::SKILL_DAMAGE);
    constexpr uint32_t SKILL_ACTIVATE_PACKET = to_uint(CombatEventType::SKILL_ACTIVATE_PACKET);
    constexpr uint32_t SKILL_RECHARGE = to_uint(CombatEventType::SKILL_RECHARGE);
    constexpr uint32_t SKILL_RECHARGED = to_uint(CombatEventType::SKILL_RECHARGED);
    constexpr uint32_t AGENT_ADDED = to_uint(CombatEventType::AGENT_ADDED);
    constexpr uint32_t AGENT_REMOVED = to_uint(CombatEventType::AGENT_REMOVED);
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
    // Used for: cast time, knockdown duration, energy spent,
    // HP/energy "snapshots" (opaque - see note), and regen-rate changes
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

    // AgentAdd packet - agent entered the client's tracking range
    // Contains: agent_id, agent_type bitmask, position, speed, allegiance_bits, etc.
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentAdd>(
        &agent_add_entry,
        [this](GW::HookStatus*, GW::Packet::StoC::AgentAdd* packet) {
            OnAgentAdd(packet);
        }
    );

    // AgentRemove packet - agent left tracking range (out of sight, dead, or DC'd)
    // Contains: agent_id
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentRemove>(
        &agent_remove_entry,
        [this](GW::HookStatus*, GW::Packet::StoC::AgentRemove* packet) {
            OnAgentRemove(packet);
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
    GW::StoC::RemoveCallback(GW::Packet::StoC::AgentAdd::STATIC_HEADER, &agent_add_entry);
    GW::StoC::RemoveCallback(GW::Packet::StoC::AgentRemove::STATIC_HEADER, &agent_remove_entry);

    active_effects.clear();
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
    bool is_map_ready = (GW::Map::GetIsMapLoaded()) && (!GW::Map::GetIsObserving()) && (instance_type != GW::Constants::InstanceType::Loading);

    return is_map_ready;
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
    if (!IsMapReady()) {
        active_effects.clear();
        return;
    }
    uint32_t now = static_cast<uint32_t>(frame_clock::GetFrameTimestamp());
    PushEvent(MakeEvent(now, CombatEventTypes::SKILL_ACTIVATE_PACKET,
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
 * - max_hp_reached: Agent HP returned to full (REACHED_MAXHP signal event)
 */
void CombatEventQueue::OnGenericValue(GW::Packet::StoC::GenericValue* packet) {
    if (!IsMapReady()) {
        active_effects.clear();
        return;
    }
    uint32_t now = static_cast<uint32_t>(frame_clock::GetFrameTimestamp());

    using namespace GW::Packet::StoC::GenericValueID;

    switch (packet->value_id) {
        case skill_activated:
            PushEvent(MakeEvent(now, CombatEventTypes::SKILL_ACTIVATED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case attack_skill_activated:
            PushEvent(MakeEvent(now, CombatEventTypes::ATTACK_SKILL_ACTIVATED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case skill_stopped:
            PushEvent(MakeEvent(now, CombatEventTypes::SKILL_STOPPED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case skill_finished:
            PushEvent(MakeEvent(now, CombatEventTypes::SKILL_FINISHED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case attack_skill_finished:
            PushEvent(MakeEvent(now, CombatEventTypes::ATTACK_SKILL_FINISHED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case interrupted:
            PushEvent(MakeEvent(now, CombatEventTypes::INTERRUPTED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case instant_skill_activated:
            PushEvent(MakeEvent(now, CombatEventTypes::INSTANT_SKILL_ACTIVATED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case attack_skill_stopped:
            PushEvent(MakeEvent(now, CombatEventTypes::ATTACK_SKILL_STOPPED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case attack_stopped:
            PushEvent(MakeEvent(now, CombatEventTypes::ATTACK_STOPPED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case melee_attack_finished:
            PushEvent(MakeEvent(now, CombatEventTypes::MELEE_ATTACK_FINISHED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case disabled:
            // Aftercast: value=1 means disabled (in aftercast), value=0 means can act
            PushEvent(MakeEvent(now, CombatEventTypes::DISABLED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case add_effect:
        {
            auto& agent_effects = active_effects[packet->agent_id];
            const bool already_active = agent_effects.find(packet->value) != agent_effects.end();
            agent_effects.insert(packet->value);
            PushEvent(MakeEvent(
                now,
                already_active ? CombatEventTypes::EFFECT_RENEWED : CombatEventTypes::EFFECT_APPLIED,
                packet->agent_id,
                packet->value,
                0,
                0.0f));
            break;
        }

        case remove_effect:
            if (auto it = active_effects.find(packet->agent_id); it != active_effects.end()) {
                it->second.erase(packet->value);
                if (it->second.empty()) {
                    active_effects.erase(it);
                }
            }
            PushEvent(MakeEvent(now, CombatEventTypes::EFFECT_REMOVED,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case skill_damage:
            // Track which skill caused upcoming damage
            PushEvent(MakeEvent(now, CombatEventTypes::SKILL_DAMAGE,
                packet->agent_id, packet->value, 0, 0.0f));
            break;

        case energygain:
            PushEvent(MakeEvent(now, CombatEventTypes::ENERGY_GAINED,
                packet->agent_id, 0, 0, static_cast<float>(packet->value)));
            break;

        case max_hp_reached:
            // Observed 2026-05-21: fires when an agent's HP returns to full,
            // not when max HP itself changes (matches the past-tense GWCA name
            // "max_hp_reached"). value/target are typically 0 - it's a signal event.
            PushEvent(MakeEvent(now, CombatEventTypes::REACHED_MAXHP,
                packet->agent_id, packet->value, 0, 0.0f));
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
    if (!IsMapReady()) {
        active_effects.clear();
        return;
    }
    uint32_t now = static_cast<uint32_t>(frame_clock::GetFrameTimestamp());

    using namespace GW::Packet::StoC::GenericValueID;

    // Fix the swapped naming (see docstring above)
    uint32_t actual_caster = packet->target;  // Yes, this is intentionally swapped
    uint32_t actual_target = packet->caster;  // Yes, this is intentionally swapped

    switch (packet->Value_id) {
        case skill_activated:
            // Skill cast with target - agent_id=caster, target_id=target
            PushEvent(MakeEvent(now, CombatEventTypes::SKILL_ACTIVATED,
                actual_caster, packet->value, actual_target, 0.0f));
            break;

        case attack_skill_activated:
            // Attack skill with target - agent_id=attacker, target_id=target
            PushEvent(MakeEvent(now, CombatEventTypes::ATTACK_SKILL_ACTIVATED,
                actual_caster, packet->value, actual_target, 0.0f));
            break;

        case attack_started:
            // Auto-attack started - agent_id=attacker, target_id=target
            PushEvent(MakeEvent(now, CombatEventTypes::ATTACK_STARTED,
                actual_caster, 0, actual_target, 0.0f));
            break;

        case effect_on_target:
            // Effect applied to target - uses NORMAL naming (not swapped!)
            // agent_id=caster, value=effect_id, target_id=target
            // Note: effect_id is NOT the skill_id - Python correlates to find skill_id
            PushEvent(MakeEvent(now, CombatEventTypes::EFFECT_ON_TARGET,
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
 * - health (34), change_health_regen (44): HP snapshot + regen rate (GWCA-labeled)
 * - energy (33), change_energy_regen (43): energy equivalents (no GWCA label)
 */
void CombatEventQueue::OnGenericFloat(GW::Packet::StoC::GenericFloat* packet) {
    if (!IsMapReady()) {
        active_effects.clear();
        return;
    }
    uint32_t now = static_cast<uint32_t>(frame_clock::GetFrameTimestamp());

    using namespace GW::Packet::StoC::GenericValueID;

    // GWCA-unlabeled value_ids confirmed via WASM RE 2026-05-21
    constexpr uint32_t energy              = 33;  // AvCharNotifyStatInit(agent, ENERGY)
    constexpr uint32_t change_energy_regen = 43;  // AvCharNotifyStatRate(agent, ENERGY)

    switch (packet->type) {
        case knocked_down:
            // Knockdown - float_value is duration in seconds
            PushEvent(MakeEvent(now, CombatEventTypes::KNOCKED_DOWN,
                packet->agent_id, 0, 0, packet->value));
            break;

        case casttime:
            // Cast time - float_value is the cast duration in seconds
            PushEvent(MakeEvent(now, CombatEventTypes::CASTTIME,
                packet->agent_id, 0, 0, packet->value));
            break;

        case energy_spent:
            // Energy spent - float_value is fraction of max energy (0.0-1.0)
            PushEvent(MakeEvent(now, CombatEventTypes::ENERGY_SPENT,
                packet->agent_id, 0, 0, packet->value));
            break;

        case energy:
            // Current energy as fraction of max (0.0-1.0). Fires on visibility
            // / engagement resync. Authoritative for engaged agents.
            PushEvent(MakeEvent(now, CombatEventTypes::CURRENT_ENERGY,
                packet->agent_id, 0, 0, packet->value));
            break;

        case health:
            // Current HP as fraction of max (0.0-1.0). Fires on visibility
            // / engagement resync. Authoritative for engaged agents.
            PushEvent(MakeEvent(now, CombatEventTypes::CURRENT_HEALTH,
                packet->agent_id, 0, 0, packet->value));
            break;

        case change_energy_regen:
            // Energy regen change - float_value is pips/sec (signed)
            PushEvent(MakeEvent(now, CombatEventTypes::ENERGY_REGEN_CHANGE,
                packet->agent_id, 0, 0, packet->value));
            break;

        case change_health_regen:
            // HP regen change - float_value is pips/sec (signed)
            PushEvent(MakeEvent(now, CombatEventTypes::HEALTH_REGEN_CHANGE,
                packet->agent_id, 0, 0, packet->value));
            break;
    }
}

/**
 * @brief Handle GenericModifier packet.
 *
 * FLOAT_TARGET channel - events with target + cause + signed HP delta:
 * - damage: Normal physical/elemental damage (positive float)
 * - critical: Critical hit damage (positive float)
 * - armorignoring (value_id=55): GWCA-misnamed; actually SpellAdjust(HEALTH).
 *   Sign distinguishes heal-spell (positive -> HEALING event) from
 *   damage-spell (negative -> ARMOR_IGNORING event). Confirmed via WASM
 *   RE 2026-05-21 - case 0x37 -> AvCharNotifySpellAdjust(target, src, HP, fval).
 *
 * The float is damage as a FRACTION of target's max HP, not absolute.
 * Python: actual = value * Agent.GetMaxHealth(target_id).
 * Example: float_value = 0.15 on a target with 480 HP = 72 damage.
 */
void CombatEventQueue::OnGenericModifier(GW::Packet::StoC::GenericModifier* packet) {
    if (!IsMapReady()) {
        active_effects.clear();
        return;
    }
    uint32_t now = static_cast<uint32_t>(frame_clock::GetFrameTimestamp());

    using namespace GW::Packet::StoC::GenericValueID;

    // Extract fields - naming is intuitive here (unlike GenericValueTarget)
    uint32_t target_id = packet->target_id;  // Who receives damage
    uint32_t source_id = packet->cause_id;   // Who deals damage
    float value = packet->value;              // Damage as fraction of max HP

    switch (packet->type) {
        case damage:
            // Normal damage - agent_id=target, target_id=source, float=damage%
            PushEvent(MakeEvent(now, CombatEventTypes::DAMAGE,
                target_id, 0, source_id, value));
            break;

        case critical:
            // Critical hit - same structure as damage
            PushEvent(MakeEvent(now, CombatEventTypes::CRITICAL,
                target_id, 0, source_id, value));
            break;

        case armorignoring:
            // Armor-ignoring HP change (verified in-game): spell damage,
            // life-steal, degen, holy, chaos, sacrifice. Sign: +gain -> HEALING,
            // -damage -> ARMOR_IGNORING. See OnGenericModifier docstring.
            PushEvent(MakeEvent(
                now,
                value > 0.0f ? CombatEventTypes::HEALING : CombatEventTypes::ARMOR_IGNORING,
                target_id,
                0,
                source_id,
                value));
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
    if (!IsMapReady()) {
        active_effects.clear();
        return;
    }
    uint32_t now = static_cast<uint32_t>(frame_clock::GetFrameTimestamp());
    // agent_id=who, value=skill_id, float_value=recharge_ms
    PushEvent(MakeEvent(now, CombatEventTypes::SKILL_RECHARGE,
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
    if (!IsMapReady()) {
        active_effects.clear();
        return;
    }
    uint32_t now = static_cast<uint32_t>(frame_clock::GetFrameTimestamp());
    // agent_id=who, value=skill_id
    PushEvent(MakeEvent(now, CombatEventTypes::SKILL_RECHARGED,
        packet->agent_id, packet->skill_id, 0, 0.0f));
}

/**
 * @brief Handle AgentAdd packet.
 *
 * Fires when an agent enters the client's tracking range (visibility resync,
 * map entry near other agents, spawn). Useful for initializing per-agent
 * tracking state on first sight.
 *
 * value = agent_type bitmask:
 *   0x30000000 = Player | PlayerNumber
 *   0x20000000 = NPC    | PlayerNumber
 *   0x00000000 = Signpost
 * target_id = allegiance_bits (raw); Python decodes via existing allegiance helpers
 * float_value = base speed (default 288.0)
 */
void CombatEventQueue::OnAgentAdd(GW::Packet::StoC::AgentAdd* packet) {
    if (!IsMapReady()) {
        active_effects.clear();
        return;
    }
    uint32_t now = static_cast<uint32_t>(frame_clock::GetFrameTimestamp());
    PushEvent(MakeEvent(now, CombatEventTypes::AGENT_ADDED,
        packet->agent_id, packet->agent_type, packet->allegiance_bits, packet->speed));
}

/**
 * @brief Handle AgentRemove packet.
 *
 * Fires when an agent leaves the client's tracking range. The packet itself
 * doesn't distinguish "out of range" from "dead" from "disconnected" - all
 * three look the same here. Cross-reference with AgentState.dead bit (when
 * AgentState gets wired) for definitive death detection.
 */
void CombatEventQueue::OnAgentRemove(GW::Packet::StoC::AgentRemove* packet) {
    if (!IsMapReady()) {
        active_effects.clear();
        return;
    }
    uint32_t now = static_cast<uint32_t>(frame_clock::GetFrameTimestamp());
    PushEvent(MakeEvent(now, CombatEventTypes::AGENT_REMOVED,
        packet->agent_id, 0, 0, 0.0f));
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
    types.attr("HEALING") = CombatEventTypes::HEALING;
    types.attr("CURRENT_HEALTH") = CombatEventTypes::CURRENT_HEALTH;
    types.attr("CURRENT_ENERGY") = CombatEventTypes::CURRENT_ENERGY;
    types.attr("HEALTH_REGEN_CHANGE") = CombatEventTypes::HEALTH_REGEN_CHANGE;
    types.attr("ENERGY_REGEN_CHANGE") = CombatEventTypes::ENERGY_REGEN_CHANGE;
    types.attr("REACHED_MAXHP") = CombatEventTypes::REACHED_MAXHP;
    types.attr("EFFECT_APPLIED") = CombatEventTypes::EFFECT_APPLIED;
    types.attr("EFFECT_REMOVED") = CombatEventTypes::EFFECT_REMOVED;
    types.attr("EFFECT_ON_TARGET") = CombatEventTypes::EFFECT_ON_TARGET;
    types.attr("EFFECT_RENEWED") = CombatEventTypes::EFFECT_RENEWED;
    types.attr("ENERGY_GAINED") = CombatEventTypes::ENERGY_GAINED;
    types.attr("ENERGY_SPENT") = CombatEventTypes::ENERGY_SPENT;
    types.attr("SKILL_DAMAGE") = CombatEventTypes::SKILL_DAMAGE;
    types.attr("SKILL_ACTIVATE_PACKET") = CombatEventTypes::SKILL_ACTIVATE_PACKET;
    types.attr("SKILL_RECHARGE") = CombatEventTypes::SKILL_RECHARGE;
    types.attr("SKILL_RECHARGED") = CombatEventTypes::SKILL_RECHARGED;
    types.attr("AGENT_ADDED") = CombatEventTypes::AGENT_ADDED;
    types.attr("AGENT_REMOVED") = CombatEventTypes::AGENT_REMOVED;

    // RawCombatEvent struct
    py::class_<RawCombatEvent>(m, "PyRawCombatEvent")
        .def(py::init<>())
        .def_readonly("timestamp", &RawCombatEvent::timestamp)
        .def_readonly("event_type", &RawCombatEvent::event_type)
        .def_readonly("agent_id", &RawCombatEvent::agent_id)
        .def_readonly("value", &RawCombatEvent::value)
        .def_readonly("target_id", &RawCombatEvent::target_id)
        .def_readonly("float_value", &RawCombatEvent::float_value)
        // Enqueue-time max-HP/max-energy snapshots - use these instead of
        // Agent.GetMaxHealth(id) / Agent.GetMaxEnergy(id) to avoid access
        // violations when an agent burrows/despawns mid-frame.
        .def_readonly("agent_max_hp", &RawCombatEvent::agent_max_hp)
        .def_readonly("agent_max_energy", &RawCombatEvent::agent_max_energy)
        .def_readonly("target_max_hp", &RawCombatEvent::target_max_hp)
        .def_readonly("target_max_energy", &RawCombatEvent::target_max_energy)
        .def("__repr__", [](const RawCombatEvent& e) {
            return "<RawCombatEvent type=" + std::to_string(e.event_type) +
                   " agent=" + std::to_string(e.agent_id) +
                   " value=" + std::to_string(e.value) +
                   " target=" + std::to_string(e.target_id) +
                   " float=" + std::to_string(e.float_value) +
                   " a_max_hp=" + std::to_string(e.agent_max_hp) +
                   " t_max_hp=" + std::to_string(e.target_max_hp) + ">";
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
