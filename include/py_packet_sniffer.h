/**
 * @file py_packet_sniffer.h
 * @brief Unified packet sniffer for both StoC and CToS traffic.
 */

#pragma once

#include "Headers.h"

#include <cstdint>
#include <mutex>
#include <vector>

#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Packets/StoC.h>
#include <GWCA/Utilities/Hook.h>

constexpr uint32_t STOC_HEADER_COUNT = 0x1e7;

enum class PacketDirection : uint8_t {
    StoC = 0,
    CToS = 1,
};

struct PacketLogEntry {
    uint64_t tick = 0;
    PacketDirection direction = PacketDirection::StoC;
    uint32_t header = 0;
    uint32_t size = 0;
    std::vector<uint8_t> data;
};

class PacketSniffer {
public:
    static PacketSniffer& Instance();

    bool Initialize();
    bool InitializeStoC();
    bool InitializeCToS();

    void Terminate();
    void TerminateStoC();
    void TerminateCToS();

    void LogPacket(PacketLogEntry entry);
    std::vector<PacketLogEntry> GetLogs();
    std::vector<PacketLogEntry> GetLogsByDirection(PacketDirection direction);
    void ClearLogs();
    void ClearLogsByDirection(PacketDirection direction);

private:
    PacketSniffer() : stoc_hook_entries_(STOC_HEADER_COUNT) {}
    PacketSniffer(const PacketSniffer&) = delete;
    PacketSniffer& operator=(const PacketSniffer&) = delete;

public:
    ~PacketSniffer() { Terminate(); }

    static constexpr size_t kMaxStoCPacketBuffer = 512;
    static constexpr uint32_t kMaxReasonableCToSPacketSize = 4096;
    static constexpr size_t kMaxLogs = 10000;

    std::mutex mutex_;
    std::vector<PacketLogEntry> logs_;

    std::vector<uint32_t> stoc_packet_sizes_;
    std::vector<GW::HookEntry> stoc_hook_entries_;

    bool stoc_initialized_ = false;
    bool ctos_initialized_ = false;
};
