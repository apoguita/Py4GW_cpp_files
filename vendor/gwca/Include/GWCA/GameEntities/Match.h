#pragma once

namespace GW {
    struct ObserverMatch {                  // total: 0x78/120
        /* +h0000 */ uint32_t match_id;     // Unique match identifier
        /* +h0004 */ uint32_t match_id_dup; // Duplicate of match_id (probably for validation)
        /* +h0008 */ uint32_t map_id;       // Map identifier
        /* +h000C */ uint32_t age;          // Match duration in minutes

        // Match configuration
        struct {
            /* +h0010 */ uint32_t type;     // Match type (1=HoH, 4=Guild Battles, 5=Hero's Ascent, 9=GvG Tournament)
            /* +h0014 */ uint32_t reserved; // Reserved/Unused (always 0)
            /* +h0018 */ uint32_t version;  // Format version (always 7)
            /* +h001C */ uint32_t state;    // Match state (1=HoH, 2=PvP/Tournament)
            /* +h0020 */ uint32_t level;    // Level (1=Normal, 2=Tournament)
            /* +h0024 */ uint32_t config1;  // Configuration 1 (0 for HoH, 1 for others)
            /* +h0028 */ uint32_t config2;  // Configuration 2 (0 for HoH, 1 for others)
            /* +h002C */ uint32_t score1;   // Score or statistic 1
            /* +h0030 */ uint32_t score2;   // Score or statistic 2
            /* +h0034 */ uint32_t score3;   // Score or statistic 3
            /* +h0038 */ uint32_t stat1;    // Statistic 1
            /* +h003C */ uint32_t stat2;    // Statistic 2
            /* +h0040 */ uint32_t data1;    // Additional data 1
            /* +h0044 */ uint32_t data2;    // Additional data 2
        } flags;

        /* +h0048 */ wchar_t* team_names;    // Pointer to team 1 name
        /* +h004C */ uint32_t unknown1[0xA]; // Unknown data
        /* +h0074 */ wchar_t* team_names2;   // Pointer to team 2 name
    };
    static_assert(sizeof(ObserverMatch) == 120, "struct ObserverMatch has incorrect size");
} // namespace GW
