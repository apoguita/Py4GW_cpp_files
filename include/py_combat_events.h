/**
 * @file py_combat_events.h
 * @brief Combat Events System - C++ Layer
 *
 * This is the C++ component of the Py4GW Combat Events system. It hooks into
 * Guild Wars game packets via GWCA and captures combat-related events into a
 * thread-safe queue that Python polls each frame.
 *
 * Architecture
 * ------------
 * The combat events system follows a two-layer design:
 *
 * 1. C++ Layer (this file + py_combat_events.cpp):
 *    - Hooks into game packets using GWCA's StoC (Server-to-Client) callbacks
 *    - Captures relevant combat events (skills, damage, effects, etc.)
 *    - Stores events in a thread-safe queue
 *    - Does minimal processing - just captures and queues raw data
 *
 * 2. Python Layer (CombatEvents.py):
 *    - Polls the C++ queue each frame via GetAndClearEvents()
 *    - Maintains all state tracking (who's casting, attacking, etc.)
 *    - Dispatches callbacks to user-registered handlers
 *    - Handles all game logic and interpretation
 *
 * Hooked Packets
 * --------------
 * - SkillActivate: Sent when a skill starts casting (gives skill_id early)
 * - GenericValue: Most skill/attack state changes (start, finish, interrupt)
 * - GenericValueTarget: Skill/attack events that have a target
 * - GenericFloat: Cast time, knockdown duration, energy spent
 * - GenericModifier: Damage dealt (normal, critical, armor-ignoring)
 *
 * Thread Safety
 * -------------
 * The event queue is protected by a mutex because:
 * - Packet callbacks run in the game thread
 * - Python polls from a different thread
 * All queue access goes through PushEvent/GetAndClearEvents which lock the mutex.
 *
 * Usage from Python
 * -----------------
 * ```python
 * import PyCombatEvents
 *
 * queue = PyCombatEvents.GetCombatEventQueue()
 * queue.Initialize()  # Register packet hooks
 *
 * # Each frame:
 * events = queue.GetAndClearEvents()
 * for event in events:
 *     # Process event.event_type, event.agent_id, etc.
 *
 * queue.Terminate()  # Unregister hooks when done
 * ```
 *
 * @see CombatEvents.py for the Python layer that processes these events
 * @see py_combat_events.cpp for the packet handler implementations
 */

#pragma once

#include <Windows.h>
#include <vector>
#include <mutex>
#include <cstdint>
#include <deque>
#include <tuple>

#include <GWCA/Packets/StoC.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Utilities/Hook.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// ============================================================================
// Raw Combat Event - Minimal struct pushed to Python
// ============================================================================

/**
 * @brief A single combat event captured from game packets.
 *
 * This struct contains the minimal data needed to reconstruct combat events.
 * Python handles all interpretation and state tracking.
 *
 * Field meanings vary by event_type:
 * - Skill events: agent_id=caster, value=skill_id, target_id=target
 * - Damage events: agent_id=target, target_id=source, float_value=damage
 * - Effect events: agent_id=affected agent, value=effect_id
 * - Knockdown: agent_id=knocked agent, float_value=duration
 */

struct RawCombatEvent {
    uint32_t timestamp;     // GetTickCount() when event occurred
    uint32_t event_type;    // GenericValueID or custom event type
    uint32_t agent_id;      // Primary agent (caster/attacker/target depending on event)
    uint32_t value;         // Skill ID, effect ID, or other uint value
    uint32_t target_id;     // Secondary agent (target of skill/attack)
    float float_value;      // Duration, damage amount, energy, etc.

    RawCombatEvent() : timestamp(0), event_type(0), agent_id(0), value(0), target_id(0), float_value(0.0f) {}
    RawCombatEvent(uint32_t ts, uint32_t type, uint32_t agent, uint32_t val, uint32_t target, float fval)
        : timestamp(ts), event_type(type), agent_id(agent), value(val), target_id(target), float_value(fval) {}
};

// ============================================================================
// Event Type Constants (exposed to Python for parsing)
// ============================================================================

/**
 * @brief Event type constants for RawCombatEvent.event_type
 *
 * These values are used to identify what kind of combat event occurred.
 * Python uses these to dispatch to the appropriate callback handler.
 *
 * Note: The values are NOT arbitrary - they're chosen to avoid conflicts
 * and are grouped by category (skills 1-8, attacks 13-15, damage 30-32, etc.)
 */
namespace CombatEventTypes {
    // ---- Skill Events (from GenericValue/GenericValueTarget packets) ----
    // These fire when agents use skills (spells, signets, etc.)

    constexpr uint32_t SKILL_ACTIVATED = 1;         // Non-attack skill started casting
                                                    // agent_id=caster, value=skill_id, target_id=target

    constexpr uint32_t ATTACK_SKILL_ACTIVATED = 2;  // Attack skill started (e.g., Jagged Strike)
                                                    // agent_id=caster, value=skill_id, target_id=target

    constexpr uint32_t SKILL_STOPPED = 3;           // Skill cast was cancelled (moved, etc.)
                                                    // agent_id=caster, value=skill_id

    constexpr uint32_t SKILL_FINISHED = 4;          // Skill completed successfully
                                                    // agent_id=caster, value=skill_id

    constexpr uint32_t ATTACK_SKILL_FINISHED = 5;   // Attack skill completed
                                                    // agent_id=caster, value=skill_id

    constexpr uint32_t INTERRUPTED = 6;             // Skill was interrupted
                                                    // agent_id=interrupted agent, value=skill_id

    constexpr uint32_t INSTANT_SKILL_ACTIVATED = 7; // Instant-cast skill used (no cast time)
                                                    // agent_id=caster, value=skill_id, target_id=target

    constexpr uint32_t ATTACK_SKILL_STOPPED = 8;    // Attack skill was cancelled
                                                    // agent_id=caster, value=skill_id

    // ---- Attack Events (auto-attacks, from GenericValueTarget) ----

    constexpr uint32_t ATTACK_STARTED = 13;         // Auto-attack started (not a skill)
                                                    // agent_id=attacker, target_id=target

    constexpr uint32_t ATTACK_STOPPED = 14;         // Auto-attack stopped/cancelled
                                                    // agent_id=attacker

    constexpr uint32_t MELEE_ATTACK_FINISHED = 15;  // Melee attack hit completed
                                                    // agent_id=attacker

    // ---- State Events ----

    constexpr uint32_t DISABLED = 16;               // Agent disabled state changed (cast-lock/aftercast)
                                                    // agent_id=agent, value=1 (disabled) or 0 (can act)
                                                    // This fires 4 times per skill: cast start, cast end,
                                                    // aftercast start, aftercast end

    constexpr uint32_t KNOCKED_DOWN = 17;           // Agent was knocked down
                                                    // agent_id=knocked agent, float_value=duration in seconds

    constexpr uint32_t CASTTIME = 18;               // Cast time modifier received
                                                    // agent_id=caster, float_value=cast duration in seconds

    // ---- Damage Events (from GenericModifier packets) ----
    // Note: For damage, the packet naming is counter-intuitive!
    // agent_id = target (who RECEIVES damage)
    // target_id = source (who DEALS damage)
    // float_value = damage as fraction of target's max HP

    constexpr uint32_t DAMAGE = 30;                 // Normal damage dealt
                                                    // agent_id=target, target_id=source, float_value=damage%

    constexpr uint32_t CRITICAL = 31;               // Critical hit damage
                                                    // agent_id=target, target_id=source, float_value=damage%

    constexpr uint32_t ARMOR_IGNORING = 32;         // Armor-ignoring damage (life steal, etc.)
                                                    // Can be negative for heals!
                                                    // agent_id=target, target_id=source, float_value=damage%

    // ---- Effect Events (from GenericValue/GenericValueTarget) ----

    constexpr uint32_t EFFECT_APPLIED = 40;         // Visual effect applied (internal effect_id, not skill_id!)
                                                    // agent_id=affected agent, value=effect_id

    constexpr uint32_t EFFECT_REMOVED = 41;         // Visual effect removed
                                                    // agent_id=affected agent, value=effect_id

    constexpr uint32_t EFFECT_ON_TARGET = 42;       // Skill effect hit a target (from effect_on_target packet)
                                                    // agent_id=caster, value=effect_id, target_id=target
                                                    // Python correlates this with recent casts to get skill_id

    // ---- Energy Events ----

    constexpr uint32_t ENERGY_GAINED = 50;          // Energy gained (from GenericValue energygain)
                                                    // agent_id=agent, float_value=energy amount (raw points)

    constexpr uint32_t ENERGY_SPENT = 51;           // Energy spent (from GenericFloat energy_spent)
                                                    // agent_id=agent, float_value=energy as fraction of max

    // ---- Skill-Damage Attribution ----

    constexpr uint32_t SKILL_DAMAGE = 60;           // Pre-notification: this skill will cause damage
                                                    // agent_id=target, value=skill_id
                                                    // Sent to TARGET before DAMAGE packet arrives

    // ---- Pre-Notification ----

    constexpr uint32_t SKILL_ACTIVATE_PACKET = 70;  // Early skill activation notification
                                                    // agent_id=caster, value=skill_id
                                                    // From SkillActivate packet (arrives before GenericValue)
}

// ============================================================================
// CombatEventQueue - Minimal C++ class, just captures and queues events
// ============================================================================

/**
 * @brief Thread-safe queue for combat events captured from game packets.
 *
 * This class hooks into GWCA packet callbacks to capture combat events and
 * stores them in a queue for Python to poll each frame. It follows the
 * principle of minimal C++ processing - all game logic is in Python.
 *
 * Lifecycle:
 * 1. Call Initialize() once at startup to register packet hooks
 * 2. Call GetAndClearEvents() each frame from Python to retrieve events
 * 3. Call Terminate() when shutting down to unregister hooks
 *
 * Thread Safety:
 * The event queue is protected by a mutex. Packet callbacks (game thread)
 * push events, and Python (different thread) retrieves them.
 *
 * Queue Management:
 * - Default max size: 1000 events
 * - If queue exceeds max, oldest events are dropped (ring buffer behavior)
 * - Use SetMaxEvents() to adjust if needed
 */
class CombatEventQueue {
public:
    CombatEventQueue() : is_initialized(false), max_events(1000) {}
    ~CombatEventQueue() { Terminate(); }

    // ---- Lifecycle ----

    /**
     * @brief Register packet callbacks with GWCA.
     * Call once at startup. Safe to call multiple times (no-op if already initialized).
     */
    void Initialize();

    /**
     * @brief Unregister packet callbacks.
     * Call when shutting down. Safe to call multiple times.
     */
    void Terminate();

    // ---- Queue Access ----

    /**
     * @brief Get all queued events and clear the queue.
     * @return Vector of events that occurred since last call.
     *
     * Call this every frame in your main loop. Thread-safe.
     */
    std::vector<RawCombatEvent> GetAndClearEvents();

    /**
     * @brief Get events without clearing (for debugging).
     * @return Copy of current queue contents.
     */
    std::vector<RawCombatEvent> PeekEvents() const;

    // ---- Configuration ----

    /** @brief Set maximum events before oldest are dropped. Default: 1000 */
    void SetMaxEvents(size_t count) { max_events = count; }

    /** @brief Get current max events setting. */
    size_t GetMaxEvents() const { return max_events; }

    /** @brief Check if packet hooks are registered. */
    bool IsInitialized() const { return is_initialized; }

    /** @brief Get current number of queued events. */
    size_t GetQueueSize() const;

private:
    bool is_initialized;
    size_t max_events;

    // GWCA hook entries - these store the callback registration state
    GW::HookEntry generic_value_entry;        // GenericValue packets (skill states)
    GW::HookEntry generic_value_target_entry; // GenericValueTarget packets (skill with target)
    GW::HookEntry generic_float_entry;        // GenericFloat packets (durations, energy)
    GW::HookEntry generic_modifier_entry;     // GenericModifier packets (damage)
    GW::HookEntry skill_activate_entry;       // SkillActivate packets (early skill notification)

    // Thread-safe event queue
    mutable std::mutex queue_mutex;
    std::deque<RawCombatEvent> event_queue;

    // ---- Packet Handlers ----
    // These are called by GWCA when packets arrive.
    // They do minimal processing - just extract relevant data and push to queue.

    void OnSkillActivate(GW::Packet::StoC::SkillActivate* packet);
    void OnGenericValue(GW::Packet::StoC::GenericValue* packet);
    void OnGenericValueTarget(GW::Packet::StoC::GenericValueTarget* packet);
    void OnGenericFloat(GW::Packet::StoC::GenericFloat* packet);
    void OnGenericModifier(GW::Packet::StoC::GenericModifier* packet);

    /**
     * @brief Add event to queue (thread-safe).
     * If queue exceeds max_events, oldest events are dropped.
     */
    void PushEvent(const RawCombatEvent& event);
};

// ============================================================================
// Global Singleton
// ============================================================================

inline CombatEventQueue& GetCombatEventQueue() {
    static CombatEventQueue instance;
    return instance;
}
