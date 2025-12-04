#pragma once
#include "Headers.h"

namespace py = pybind11;

template <typename T>
std::vector<T> ConvertArrayToVector(const GW::Array<T>& arr) {
    return std::vector<T>(arr.begin(), arr.end());
}


// Wrapper for function pointers
struct UIInteractionCallbackWrapper {
    uintptr_t callback_address;  // Store function pointer as an integer

    UIInteractionCallbackWrapper(GW::UI::UIInteractionCallback callback)
        : callback_address(reinterpret_cast<uintptr_t>(callback)) {
    }

    uintptr_t get_address() const { return callback_address; }
};

struct FramePositionWrapper {
    uint32_t top;
    uint32_t left;
    uint32_t bottom;
    uint32_t right;

    uint32_t content_top;
    uint32_t content_left;
    uint32_t content_bottom;
    uint32_t content_right;

    float unknown;
    float scale_factor;
    float viewport_width;
    float viewport_height;

    float screen_top;
    float screen_left;
    float screen_bottom;
    float screen_right;

    uint32_t top_on_screen;
    uint32_t left_on_screen;
    uint32_t bottom_on_screen;
    uint32_t right_on_screen;

	uint32_t width_on_screen;
	uint32_t height_on_screen;

    float viewport_scale_x;
    float viewport_scale_y;
};

struct FrameRelationWrapper {
	uint32_t parent_id;
	uint32_t field67_0x124;
	uint32_t field68_0x128;
	uint32_t frame_hash_id;
    std::vector<uint32_t> siblings;
};

class UIFrame {
public:
    bool is_created;
    bool is_visible;

	uint32_t frame_id;
	uint32_t parent_id;
	uint32_t frame_hash;
    uint32_t field1_0x0;
    uint32_t field2_0x4;
    uint32_t frame_layout;
    uint32_t field3_0xc;
    uint32_t field4_0x10;
    uint32_t field5_0x14;
    uint32_t visibility_flags;
    uint32_t field7_0x1c;
    uint32_t type;
    uint32_t template_type;
    uint32_t field10_0x28;
    uint32_t field11_0x2c;
    uint32_t field12_0x30;
    uint32_t field13_0x34;
    uint32_t field14_0x38;
    uint32_t field15_0x3c;
    uint32_t field16_0x40;
    uint32_t field17_0x44;
    uint32_t field18_0x48;
    uint32_t field19_0x4c;
    uint32_t field20_0x50;
    uint32_t field21_0x54;
    uint32_t field22_0x58;
    uint32_t field23_0x5c;
    uint32_t field24_0x60;
    uint32_t field24a_0x64;
    uint32_t field24b_0x68;
    uint32_t field25_0x6c;
    uint32_t field26_0x70;
    uint32_t field27_0x74;
    uint32_t field28_0x78;
    uint32_t field29_0x7c;
    uint32_t field30_0x80;
    std::vector<uintptr_t> field31_0x84;
    uint32_t field32_0x94;
    uint32_t field33_0x98;
    uint32_t field34_0x9c;
    uint32_t field35_0xa0;
    uint32_t field36_0xa4;
    std::vector<UIInteractionCallbackWrapper> frame_callbacks;
    uint32_t child_offset_id;
    uint32_t field40_0xc0;
    uint32_t field41_0xc4;
    uint32_t field42_0xc8;
    uint32_t field43_0xcc;
    uint32_t field44_0xd0;
    uint32_t field45_0xd4;
	FramePositionWrapper position;
    uint32_t field63_0x11c;
    uint32_t field64_0x120;
    uint32_t field65_0x124;
    FrameRelationWrapper relation;
    uint32_t field73_0x144;
    uint32_t field74_0x148;
    uint32_t field75_0x14c;
    uint32_t field76_0x150;
    uint32_t field77_0x154;
    uint32_t field78_0x158;
    uint32_t field79_0x15c;
    uint32_t field80_0x160;
    uint32_t field81_0x164;
    uint32_t field82_0x168;
    uint32_t field83_0x16c;
    uint32_t field84_0x170;
    uint32_t field85_0x174;
    uint32_t field86_0x178;
    uint32_t field87_0x17c;
    uint32_t field88_0x180;
    uint32_t field89_0x184;
    uint32_t field90_0x188;
    uint32_t frame_state;
    uint32_t field92_0x190;
    uint32_t field93_0x194;
    uint32_t field94_0x198;
    uint32_t field95_0x19c;
    uint32_t field96_0x1a0;
    uint32_t field97_0x1a4;
    uint32_t field98_0x1a8;
    //TooltipInfo* tooltip_info;
    uint32_t field100_0x1b0;
    uint32_t field101_0x1b4;
    uint32_t field102_0x1b8;
    uint32_t field103_0x1bc;
    uint32_t field104_0x1c0;
    uint32_t field105_0x1c4;

    UIFrame(int pframe_id) {
		frame_id = pframe_id;
        GetContext();
    }
	void GetContext();
};

class UIManager {
public:


    static uint32_t GetFrameIDByLabel(const std::string& label) {
        std::wstring wlabel(label.begin(), label.end()); // Convert to wide string
        return GW::UI::GetFrameIDByLabel(wlabel.c_str());
    }

    static uint32_t GetFrameIDByHash(uint32_t hash) {
        return GW::UI::GetFrameIDByHash(hash);
    }

    static uint32_t GetHashByLabel(const std::string& label) {
        return GW::UI::GetHashByLabel(label);
    }

    static std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>> GetFrameHierarchy() {
        return GW::UI::GetFrameHierarchy();
    }

    static std::vector<std::pair<uint32_t, uint32_t>> GetFrameCoordsByHash(uint32_t frame_hash) {
        return GW::UI::GetFrameCoordsByHash(frame_hash);
    }

	static uint32_t GetChildFrameID(uint32_t parent_hash, std::vector<uint32_t> child_offsets) {
		return GW::UI::GetChildFrameID(parent_hash, child_offsets);
	}

	static void ButtonClick(uint32_t frame_id) {
        GW::GameThread::Enqueue([frame_id]() {
            GW::UI::Frame* frame = GW::UI::GetFrameById(frame_id);
            GW::UI::ButtonClick(frame);
            });
        
	}

    static void TestMouseAction(uint32_t frame_id, uint32_t current_state, uint32_t wparam_value = 0, uint32_t lparam=0) {
        GW::GameThread::Enqueue([frame_id, current_state, wparam_value, lparam]() {
			GW::UI::TestMouseAction(frame_id, current_state, wparam_value, lparam);
            });

    }

    static void TestMouseClickAction(uint32_t frame_id, uint32_t current_state, uint32_t wparam_value = 0, uint32_t lparam = 0) {
        GW::GameThread::Enqueue([frame_id, current_state, wparam_value, lparam]() {
            GW::UI::TestMouseClickAction(frame_id, current_state, wparam_value, lparam);
            });

    }

	static uint32_t GetRootFrameID() {
		GW::UI::Frame* frame = GW::UI::GetRootFrame();
		if (!frame) {
			return 0;
		}
		return frame->frame_id;
	}

	static bool IsWorldMapShowing() {
		return GW::UI::GetIsWorldMapShowing();
	}

    static std::vector<uint32_t> GetPreferenceOptions(uint32_t pref) {
        GW::UI::EnumPreference pref_enum = static_cast<GW::UI::EnumPreference>(pref);

        uint32_t* options_ptr = nullptr;
        uint32_t count = GW::UI::GetPreferenceOptions(pref_enum, &options_ptr);

        std::vector<uint32_t> result;
        if (options_ptr && count > 0) {
            result.assign(options_ptr, options_ptr + count);
        }
        return result;
    }



	static uint32_t GetEnumPreference(uint32_t pref) {
		GW::UI::EnumPreference pref_enum = static_cast<GW::UI::EnumPreference>(pref);
		return GW::UI::GetPreference(pref_enum);
	}

	static uint32_t GetIntPreference(uint32_t pref) {
		GW::UI::NumberPreference pref_enum = static_cast<GW::UI::NumberPreference>(pref);
		return GW::UI::GetPreference(pref_enum);
	}

	static std::string GetStringPreference(uint32_t pref) {
		GW::UI::StringPreference pref_enum = static_cast<GW::UI::StringPreference>(pref);
		wchar_t* str = GW::UI::GetPreference(pref_enum);
		if (!str) {
			return "";
		}
		std::wstring wstr(str);
		std::string str_utf8(wstr.begin(), wstr.end());
		return str_utf8;

	}

	static bool GetBoolPreference(uint32_t pref) {
		GW::UI::FlagPreference pref_enum = static_cast<GW::UI::FlagPreference>(pref);
		return GW::UI::GetPreference(pref_enum);
	}

    static void SetEnumPreference(uint32_t pref, uint32_t value) {
        GW::GameThread::Enqueue([pref, value]() {
            GW::UI::EnumPreference pref_enum = static_cast<GW::UI::EnumPreference>(pref);
            GW::UI::SetPreference(pref_enum, value);
            });	
	}

	static void SetIntPreference(uint32_t pref, uint32_t value) {
		GW::GameThread::Enqueue([pref, value]() {
			GW::UI::NumberPreference pref_enum = static_cast<GW::UI::NumberPreference>(pref);
			GW::UI::SetPreference(pref_enum, value);
			});
	}

	static void SetStringPreference(uint32_t pref, const std::string& value) {
		GW::GameThread::Enqueue([pref, value]() {
			GW::UI::StringPreference pref_enum = static_cast<GW::UI::StringPreference>(pref);
			std::wstring wstr(value.begin(), value.end());
			wchar_t* wstr_ptr = const_cast<wchar_t*>(wstr.c_str());
			GW::UI::SetPreference(pref_enum, wstr_ptr);
			});
	}

	static void SetBoolPreference(uint32_t pref, bool value) {
		GW::GameThread::Enqueue([pref, value]() {
			GW::UI::FlagPreference pref_enum = static_cast<GW::UI::FlagPreference>(pref);
			GW::UI::SetPreference(pref_enum, value);
			});
	}

	static uint32_t GetFrameLimit() {
		return GW::UI::GetFrameLimit();
	}

    static void SetFrameLimit(uint32_t value) {
        GW::GameThread::Enqueue([value]() {
            GW::UI::SetFrameLimit(value);
            });

	}

	static std::vector<uint32_t> GetKeyMappings() {
        // NB: This address is fond twice, we only care about the first.
        uint32_t* key_mappings_array = nullptr;
        uint32_t key_mappings_array_length = 0x75;
        uintptr_t address = GW::Scanner::FindAssertion("FrKey.cpp", "count == arrsize(s_remapTable)", 0, 0x13);
        Logger::AssertAddress("key_mappings", address);
        if (address && GW::Scanner::IsValidPtr(*(uintptr_t*)address)) {
            key_mappings_array = *(uint32_t**)address;
        }
		std::vector<uint32_t> result;
		if (key_mappings_array) {
			result.assign(key_mappings_array, key_mappings_array + key_mappings_array_length);
		}
		return result;
	}

	static void SetKeyMappings(const std::vector<uint32_t>& mappings) {
		GW::GameThread::Enqueue([mappings]() {
			// NB: This address is fond twice, we only care about the first.
			uint32_t* key_mappings_array = nullptr;
			uint32_t key_mappings_array_length = 0x75;
			uintptr_t address = GW::Scanner::FindAssertion("FrKey.cpp", "count == arrsize(s_remapTable)", 0, 0x13);
			Logger::AssertAddress("key_mappings", address);
			if (address && GW::Scanner::IsValidPtr(*(uintptr_t*)address)) {
				key_mappings_array = *(uint32_t**)address;
			}
			if (key_mappings_array) {
				size_t count = std::min(static_cast<size_t>(key_mappings_array_length), mappings.size());
				std::copy(mappings.begin(), mappings.begin() + count, key_mappings_array);
			}
			});
	}



	static std::vector <uint32_t> GetFrameArray() {
		return GW::UI::GetFrameArray();
	}

    // Press and hold a key (down only)
    static void KeyDown(uint32_t key, uint32_t frame_id) {
        GW::GameThread::Enqueue([key, frame_id]() {
            // Convert the integer into a ControlAction enum value
            GW::UI::ControlAction key_action = static_cast<GW::UI::ControlAction>(key);

            GW::UI::Frame* frame = nullptr;
            if (frame_id != 0) {
                frame = GW::UI::GetFrameById(frame_id);
            }

            // Call the actual UI function
            GW::UI::Keydown(key_action, frame);
            });
    }

    // Release a key (up only)
    static void KeyUp(uint32_t key, uint32_t frame_id) {
        GW::GameThread::Enqueue([key, frame_id]() {
            GW::UI::ControlAction key_action = static_cast<GW::UI::ControlAction>(key);

            GW::UI::Frame* frame = nullptr;
            if (frame_id != 0) {
                frame = GW::UI::GetFrameById(frame_id);
            }

            GW::UI::Keyup(key_action, frame);
            });
    }

    // Simulate a full keypress (down + up)
    static void KeyPress(uint32_t key, uint32_t frame_id) {
        GW::GameThread::Enqueue([key, frame_id]() {
            GW::UI::ControlAction key_action = static_cast<GW::UI::ControlAction>(key);

            GW::UI::Frame* frame = nullptr;
            if (frame_id != 0) {
                frame = GW::UI::GetFrameById(frame_id);
            }

            GW::UI::Keypress(key_action, frame);
            });
    }

    static std::vector<uintptr_t> GetWindowPosition(uint32_t window_id) {
        std::vector<uintptr_t> result;
        GW::UI::WindowPosition* position =
            GW::UI::GetWindowPosition(static_cast<GW::UI::WindowID>(window_id));
        if (position) {
            result.push_back(static_cast<uintptr_t>(position->left()));
            result.push_back(static_cast<uintptr_t>(position->top()));
            result.push_back(static_cast<uintptr_t>(position->right()));
            result.push_back(static_cast<uintptr_t>(position->bottom()));
        }
        return result;
    }

	static bool IsWindowVisible(uint32_t window_id) {
        GW::UI::WindowPosition* position = GW::UI::GetWindowPosition(static_cast<GW::UI::WindowID>(window_id));
		if (!position) {
			return false;
		}
        return (position->state & 0x1) != 0;
	}

	static void SetWindowVisible(uint32_t window_id, bool is_visible) {
		GW::GameThread::Enqueue([window_id, is_visible]() {
			GW::UI::SetWindowVisible(static_cast<GW::UI::WindowID>(window_id), is_visible);
			});
	}

    static void SetWindowPosition(uint32_t window_id, const std::vector<uintptr_t>& position) {
        GW::GameThread::Enqueue([window_id, position]() {
            if (position.size() < 4) return; // Ensure we have enough data
            GW::UI::WindowPosition* win_pos =
                GW::UI::GetWindowPosition(static_cast<GW::UI::WindowID>(window_id));
            if (!win_pos) return;

            // write back into p1/p2 from the values we accepted (left, top, right, bottom)
            win_pos->p1.x = static_cast<float>(position[0]);
            win_pos->p1.y = static_cast<float>(position[1]);
            win_pos->p2.x = static_cast<float>(position[2]);
            win_pos->p2.y = static_cast<float>(position[3]);

            GW::UI::SetWindowPosition(static_cast<GW::UI::WindowID>(window_id), win_pos);
            });
    }

	static bool IsShiftScreenShot() {
		return GW::UI::GetIsShiftScreenShot();
	}

};

