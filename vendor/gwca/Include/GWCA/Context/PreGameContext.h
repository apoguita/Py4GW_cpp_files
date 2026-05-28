#pragma once

#include <GWCA/GameContainers/Array.h>
#include <GWCA/Utilities/Export.h>

namespace GW {
    struct PreGameContext;
    GWCA_API PreGameContext* GetPreGameContext();

    struct LoginCharacter {
        uint32_t unk0;
        uint32_t pvp_or_campaign;
        uint32_t UnkPvPData01;
		uint32_t UnkPvPData02;
		uint32_t UnkPvPData03;
		uint32_t UnkPvPData04;
        uint32_t Unk01[4];
        uint32_t Level;
		uint32_t current_map_id;
		uint32_t Unk02[7];
        wchar_t character_name[20];
    };
    struct PreGameContext {
        uint32_t frame_id;
        uint32_t Unk01[20];
        float h0054;
        float h0058;
        uint32_t Unk02[2];
        float h0060;
        uint32_t Unk03[2];
        float h0068;
        uint32_t Unk04;
        float h0070;
        uint32_t Unk05;
        float h0078;
        uint32_t Unk06[8];
        float h00a0;
        float h00a4;
        float h00a8;
        uint32_t Unk07[9];
        uint32_t Unk08;                    // 0xD4  game patch: this field became -1 sentinel
        uint32_t chosen_character_index;   // 0xD8  live selection (was 0xD4 pre-patch, shifted +4)
        uint32_t pad_0xDC;                 // 0xDC  patch inserted field, pushed chars to 0xE0
        GW::Array<LoginCharacter> chars;   // 0xE0
    };
}
