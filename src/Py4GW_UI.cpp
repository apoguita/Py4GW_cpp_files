#include "Py4GW_UI.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>

namespace py = pybind11;

class Py4GW_UI {
private:
    std::vector<Command> commands_;
    std::unordered_map<std::string, py::object> vars_;
    bool in_window = false;
    bool window_visible = true;
    int blocked_scope_depth = 0;
    std::vector<bool> table_scope_stack;
    std::vector<int> child_scope_stack;
    std::vector<int> tab_bar_scope_stack;
    std::vector<int> tab_item_scope_stack;
    std::vector<int> popup_scope_stack;
    std::vector<int> popup_modal_scope_stack;
    std::vector<int> combo_scope_stack;
    static constexpr uint32_t kInvalidSlot = 0xFFFFFFFFu;

    enum class SlotType : uint8_t {
        Unknown,
        Bool,
        Int,
        Float,
        String,
        Color4,
    };

    struct SlotState {
        SlotType type = SlotType::Unknown;
        bool b = false;
        int i = 0;
        float f = 0.0f;
        std::string s;
        std::array<float, 4> c4 = { 1.f, 1.f, 1.f, 1.f };
        std::vector<char> text_buf;
        bool dirty = false;
    };

    std::vector<SlotState> slots_;
    std::vector<std::string> slot_names_;
    std::unordered_map<std::string, uint32_t> slot_by_name_;
    bool commands_compiled_ = false;
    bool finalized_ = false;
    uint64_t layout_version_ = 0;
    uint64_t recording_ignored_count_ = 0;
    double perf_render_total_ms_ = 0.0;
    double perf_finalize_ms_ = 0.0;
    double perf_python_callable_ms_ = 0.0;
    double perf_slot_sync_ms_ = 0.0;

    SlotType infer_slot_type(const py::object& value) const {
        if (value.is_none()) return SlotType::Unknown;
        if (py::isinstance<py::bool_>(value)) return SlotType::Bool;
        if (py::isinstance<py::int_>(value)) return SlotType::Int;
        if (py::isinstance<py::float_>(value)) return SlotType::Float;
        if (py::isinstance<py::str>(value)) return SlotType::String;
        if (py::isinstance<py::sequence>(value)) {
            try {
                py::sequence seq = py::cast<py::sequence>(value);
                if (py::len(seq) == 4) return SlotType::Color4;
            } catch (...) {}
        }
        return SlotType::Unknown;
    }

    py::object slot_to_py(const SlotState& slot) const {
        switch (slot.type) {
        case SlotType::Bool: return py::bool_(slot.b);
        case SlotType::Int: return py::int_(slot.i);
        case SlotType::Float: return py::float_(slot.f);
        case SlotType::String: return py::str(slot.s);
        case SlotType::Color4: return py::make_tuple(slot.c4[0], slot.c4[1], slot.c4[2], slot.c4[3]);
        default: return py::none();
        }
    }

    void set_slot_from_py(uint32_t slot_id, const py::object& value) {
        if (slot_id == kInvalidSlot || slot_id >= slots_.size()) return;
        SlotState& slot = slots_[slot_id];
        SlotType inferred = infer_slot_type(value);
        if (slot.type == SlotType::Unknown) slot.type = inferred;

        try {
            switch (slot.type) {
            case SlotType::Bool: slot.b = py::cast<bool>(value); break;
            case SlotType::Int: slot.i = py::cast<int>(value); break;
            case SlotType::Float: slot.f = py::cast<float>(value); break;
            case SlotType::String: slot.s = py::cast<std::string>(value); break;
            case SlotType::Color4:
                if (py::isinstance<py::sequence>(value)) {
                    py::sequence seq = py::cast<py::sequence>(value);
                    const size_t n = std::min<size_t>(4, static_cast<size_t>(py::len(seq)));
                    for (size_t i = 0; i < n; ++i) slot.c4[i] = py::cast<float>(seq[i]);
                }
                break;
            default:
                break;
            }
        } catch (...) {
        }
    }

    uint32_t ensure_slot(const std::string& name, SlotType preferred = SlotType::Unknown) {
        if (name.empty()) return kInvalidSlot;
        const auto it = slot_by_name_.find(name);
        if (it != slot_by_name_.end()) {
            SlotState& slot = slots_[it->second];
            if (slot.type == SlotType::Unknown && preferred != SlotType::Unknown) slot.type = preferred;
            return it->second;
        }

        const uint32_t id = static_cast<uint32_t>(slots_.size());
        slot_by_name_[name] = id;
        slot_names_.push_back(name);
        slots_.emplace_back();
        slots_.back().type = preferred;

        const auto py_it = vars_.find(name);
        if (py_it != vars_.end() && !py_it->second.is_none()) {
            set_slot_from_py(id, py_it->second);
        }
        return id;
    }

    std::string slot_name_for(uint32_t slot) const {
        if (slot < slot_names_.size()) return slot_names_[slot];
        return std::string();
    }

    bool read_slot_bool(uint32_t slot, const std::string& name, bool fallback) const {
        if (slot != kInvalidSlot && slot < slots_.size()) {
            const SlotState& s = slots_[slot];
            if (s.type == SlotType::Bool) return s.b;
            if (s.type == SlotType::Int) return s.i != 0;
        }
        return read_bool(name, fallback);
    }

    int read_slot_int(uint32_t slot, const std::string& name, int fallback) const {
        if (slot != kInvalidSlot && slot < slots_.size()) {
            const SlotState& s = slots_[slot];
            if (s.type == SlotType::Int) return s.i;
            if (s.type == SlotType::Bool) return s.b ? 1 : 0;
        }
        return read_int(name, fallback);
    }

    float read_slot_float(uint32_t slot, const std::string& name, float fallback) const {
        if (slot != kInvalidSlot && slot < slots_.size()) {
            const SlotState& s = slots_[slot];
            if (s.type == SlotType::Float) return s.f;
            if (s.type == SlotType::Int) return static_cast<float>(s.i);
        }
        return read_float(name, fallback);
    }

    std::string read_slot_string(uint32_t slot, const std::string& name, const std::string& fallback) const {
        if (slot != kInvalidSlot && slot < slots_.size()) {
            const SlotState& s = slots_[slot];
            if (s.type == SlotType::String) return s.s;
        }
        return read_string(name, fallback);
    }

    void write_slot_bool(uint32_t slot, const std::string& name, bool value) {
        if (slot != kInvalidSlot && slot < slots_.size()) {
            SlotState& s = slots_[slot];
            if (s.type == SlotType::Unknown) s.type = SlotType::Bool;
            if (s.type == SlotType::Bool && s.b != value) {
                s.b = value;
                s.dirty = true;
            } else if (s.type == SlotType::Bool) {
                s.b = value;
            }
            return;
        }
        vars_[name] = py::bool_(value);
    }

    void write_slot_int(uint32_t slot, const std::string& name, int value) {
        if (slot != kInvalidSlot && slot < slots_.size()) {
            SlotState& s = slots_[slot];
            if (s.type == SlotType::Unknown) s.type = SlotType::Int;
            if (s.type == SlotType::Int && s.i != value) {
                s.i = value;
                s.dirty = true;
            } else if (s.type == SlotType::Int) {
                s.i = value;
            }
            return;
        }
        vars_[name] = py::int_(value);
    }

    void write_slot_float(uint32_t slot, const std::string& name, float value) {
        if (slot != kInvalidSlot && slot < slots_.size()) {
            SlotState& s = slots_[slot];
            if (s.type == SlotType::Unknown) s.type = SlotType::Float;
            if (s.type == SlotType::Float && s.f != value) {
                s.f = value;
                s.dirty = true;
            } else if (s.type == SlotType::Float) {
                s.f = value;
            }
            return;
        }
        vars_[name] = py::float_(value);
    }

    void write_slot_string(uint32_t slot, const std::string& name, const std::string& value) {
        if (slot != kInvalidSlot && slot < slots_.size()) {
            SlotState& s = slots_[slot];
            if (s.type == SlotType::Unknown) s.type = SlotType::String;
            if (s.type == SlotType::String && s.s != value) {
                s.s = value;
                s.dirty = true;
            } else if (s.type == SlotType::String) {
                s.s = value;
            }
            return;
        }
        vars_[name] = py::str(value);
    }

    std::array<float, 4> read_slot_color4(uint32_t slot, const std::string& name, const std::array<float, 4>& fallback) const {
        if (slot != kInvalidSlot && slot < slots_.size()) {
            const SlotState& s = slots_[slot];
            if (s.type == SlotType::Color4) return s.c4;
        }
        const auto it = vars_.find(name);
        if (it == vars_.end() || it->second.is_none()) return fallback;

        std::array<float, 4> out = fallback;
        try {
            if (py::isinstance<py::sequence>(it->second)) {
                py::sequence seq = py::cast<py::sequence>(it->second);
                const size_t n = std::min<size_t>(4, static_cast<size_t>(py::len(seq)));
                for (size_t i = 0; i < n; ++i) out[i] = py::cast<float>(seq[i]);
            }
        }
        catch (const py::cast_error&) {
            return fallback;
        }
        return out;
    }

    void write_slot_color4(uint32_t slot, const std::string& name, const std::array<float, 4>& value) {
        if (slot != kInvalidSlot && slot < slots_.size()) {
            SlotState& s = slots_[slot];
            if (s.type == SlotType::Unknown) s.type = SlotType::Color4;
            if (s.type == SlotType::Color4 && s.c4 != value) {
                s.c4 = value;
                s.dirty = true;
            }
            else if (s.type == SlotType::Color4) {
                s.c4 = value;
            }
            return;
        }
        vars_[name] = py::make_tuple(value[0], value[1], value[2], value[3]);
    }

    void flush_dirty_slots_to_vars() {
        for (uint32_t i = 0; i < slots_.size(); ++i) {
            SlotState& s = slots_[i];
            if (!s.dirty) continue;
            vars_[slot_names_[i]] = slot_to_py(s);
            s.dirty = false;
        }
    }

    void push_command(Command&& cmd) {
        if (finalized_) {
            recording_ignored_count_++;
            return;
        }
        commands_compiled_ = false;
        commands_.push_back(std::move(cmd));
    }

    void finalize_impl() {
        for (auto& cmd : commands_) {
            switch (cmd.type) {
            case OpType::Begin:
            case OpType::Checkbox:
            case OpType::InputText:
            case OpType::InputInt:
            case OpType::InputFloat:
            case OpType::SliderInt:
            case OpType::SliderFloat:
            case OpType::Combo:
            case OpType::RadioButton:
            case OpType::CollapsingHeader:
            case OpType::IsItemHovered:
            case OpType::IsWindowCollapsed:
            case OpType::GetClipboardText:
            case OpType::ColorEdit4:
                if (auto* p = std::get_if<SigStrVarInt>(&cmd.payload)) {
                    if (p->slot == kInvalidSlot && !p->str_var.empty()) {
                        SlotType t = SlotType::Unknown;
                        if (cmd.type == OpType::Checkbox || cmd.type == OpType::IsItemHovered || cmd.type == OpType::IsWindowCollapsed) t = SlotType::Bool;
                        else if (cmd.type == OpType::InputText || cmd.type == OpType::GetClipboardText) t = SlotType::String;
                        else if (cmd.type == OpType::InputFloat) t = SlotType::Float;
                        else if (cmd.type == OpType::ColorEdit4) t = SlotType::Color4;
                        else t = SlotType::Int;
                        p->slot = ensure_slot(p->str_var, t);
                    }
                }
                break;
            default:
                break;
            }

            if (auto* p = std::get_if<SigButton>(&cmd.payload)) {
                if (p->slot == kInvalidSlot && !p->str_var.empty()) {
                    p->slot = ensure_slot(p->str_var, SlotType::Bool);
                }
            }
            if (auto* p = std::get_if<SigSelectable>(&cmd.payload)) {
                if (p->slot == kInvalidSlot && !p->str_var.empty()) {
                    p->slot = ensure_slot(p->str_var, SlotType::Bool);
                }
            }
            if (auto* p = std::get_if<SigStrVarIntRange>(&cmd.payload)) {
                if (p->slot == kInvalidSlot && !p->str_var.empty()) {
                    p->slot = ensure_slot(p->str_var, SlotType::Int);
                }
            }
            if (auto* p = std::get_if<SigStrVarFloatRange>(&cmd.payload)) {
                if (p->slot == kInvalidSlot && !p->str_var.empty()) {
                    p->slot = ensure_slot(p->str_var, SlotType::Float);
                }
            }
            if (auto* p = std::get_if<SigStrVarItems>(&cmd.payload)) {
                if (p->slot == kInvalidSlot && !p->str_var.empty()) {
                    p->slot = ensure_slot(p->str_var, SlotType::Int);
                }
            }
            if (auto* p = std::get_if<SigStrVar2>(&cmd.payload)) {
                if (p->slot == kInvalidSlot && !p->str_var.empty()) p->slot = ensure_slot(p->str_var, SlotType::Float);
                if (p->slot2 == kInvalidSlot && !p->str_var2.empty()) p->slot2 = ensure_slot(p->str_var2, SlotType::Float);
            }
        }
        commands_compiled_ = true;
    }

    bool read_bool(const std::string& name, bool fallback) const {
        const auto it = vars_.find(name);
        if (it == vars_.end() || it->second.is_none()) {
            return fallback;
        }
        try {
            return py::cast<bool>(it->second);
        }
        catch (const py::cast_error&) {
            return fallback;
        }
    }

    int read_int(const std::string& name, int fallback) const {
        const auto it = vars_.find(name);
        if (it == vars_.end() || it->second.is_none()) {
            return fallback;
        }
        try {
            return py::cast<int>(it->second);
        }
        catch (const py::cast_error&) {
            return fallback;
        }
    }

    float read_float(const std::string& name, float fallback) const {
        const auto it = vars_.find(name);
        if (it == vars_.end() || it->second.is_none()) {
            return fallback;
        }
        try {
            return py::cast<float>(it->second);
        }
        catch (const py::cast_error&) {
            return fallback;
        }
    }

    std::string read_string(const std::string& name, const std::string& fallback) const {
        const auto it = vars_.find(name);
        if (it == vars_.end() || it->second.is_none()) {
            return fallback;
        }
        try {
            return py::cast<std::string>(it->second);
        }
        catch (const py::cast_error&) {
            return fallback;
        }
    }

public:
    Py4GW_UI() = default;

    void finalize() {
        if (finalized_ && commands_compiled_) {
            return;
        }
        const auto t0 = std::chrono::high_resolution_clock::now();
        finalize_impl();
        finalized_ = true;
        layout_version_++;
        const auto t1 = std::chrono::high_resolution_clock::now();
        perf_finalize_ms_ = std::chrono::duration<double, std::milli>(t1 - t0).count();
    }

    bool is_finalized() const { return finalized_; }
    uint64_t layout_version() const { return layout_version_; }
    uint64_t recording_ignored_count() const { return recording_ignored_count_; }
    py::dict perf_stats() const {
        py::dict d;
        d[py::str("render_total_ms")] = py::float_(perf_render_total_ms_);
        d[py::str("finalize_ms")] = py::float_(perf_finalize_ms_);
        d[py::str("python_callable_ms")] = py::float_(perf_python_callable_ms_);
        d[py::str("slot_sync_ms")] = py::float_(perf_slot_sync_ms_);
        d[py::str("layout_version")] = py::int_(layout_version_);
        d[py::str("is_finalized")] = py::bool_(finalized_);
        d[py::str("recording_ignored_count")] = py::int_(recording_ignored_count_);
        return d;
    }

    void clear_ui() { commands_.clear(); commands_compiled_ = false; finalized_ = false; }
    void clear_vars() { vars_.clear(); slots_.clear(); slot_names_.clear(); slot_by_name_.clear(); }

    void set_var(const std::string& name, const py::object& value) {
        vars_[name] = value;
        const uint32_t slot = ensure_slot(name, infer_slot_type(value));
        set_slot_from_py(slot, value);
    }

    uint32_t register_var(const std::string& name, const std::string& type, const py::object& initial = py::none()) {
        SlotType preferred = SlotType::Unknown;
        if (type == "bool") preferred = SlotType::Bool;
        else if (type == "int") preferred = SlotType::Int;
        else if (type == "float") preferred = SlotType::Float;
        else if (type == "string") preferred = SlotType::String;
        else if (type == "color4") preferred = SlotType::Color4;

        const uint32_t slot = ensure_slot(name, preferred);
        if (!initial.is_none()) {
            set_slot_from_py(slot, initial);
            vars_[name] = slot_to_py(slots_[slot]);
        }
        return slot;
    }

    py::object vars(const std::string& name) const {
        const auto sit = slot_by_name_.find(name);
        if (sit != slot_by_name_.end() && sit->second < slots_.size()) {
            return slot_to_py(slots_[sit->second]);
        }
        const auto it = vars_.find(name);
        if (it == vars_.end()) {
            return py::none();
        }
        return it->second;
    }

    py::dict all_vars() const {
        py::dict out;
        for (const auto& [key, value] : vars_) {
            out[py::str(key)] = value;
        }
        for (size_t i = 0; i < slots_.size() && i < slot_names_.size(); ++i) {
            out[py::str(slot_names_[i])] = slot_to_py(slots_[i]);
        }
        return out;
    }

    py::dict build_vars_dict() const {
        return all_vars();
    }

    void apply_vars_dict(const py::dict& d) {
        for (auto item : d) {
            const std::string key = py::cast<std::string>(item.first);
            const py::object value = py::reinterpret_borrow<py::object>(item.second);
            vars_[key] = value;
            const uint32_t slot = ensure_slot(key, infer_slot_type(value));
            set_slot_from_py(slot, value);
        }
    }

	bool can_continue() const {
		if ((in_window && window_visible && blocked_scope_depth == 0)) {
			return true;
		}
        return false;
	}

    void render() {
        const auto render_t0 = std::chrono::high_resolution_clock::now();
        perf_python_callable_ms_ = 0.0;
        perf_slot_sync_ms_ = 0.0;
        if (!commands_compiled_ && !commands_.empty()) finalize();
        // Reset per-frame gate state before replaying cached commands.
        in_window = false;
        window_visible = true;
        blocked_scope_depth = 0;
        table_scope_stack.clear();
        child_scope_stack.clear();
        tab_bar_scope_stack.clear();
        tab_item_scope_stack.clear();
        popup_scope_stack.clear();
        popup_modal_scope_stack.clear();
        combo_scope_stack.clear();

        for (auto& cmd : commands_) {
            switch (cmd.type) {
            case OpType::Begin: {process_begin(cmd);break;}
            case OpType::End: {process_end();break;}
            case OpType::Text: {process_text(cmd);break;}
            case OpType::TextColored: {process_text_colored(cmd);break;}
            case OpType::BulletText: {process_bullet_text(cmd);break;}
            case OpType::Separator: {process_separator();break;}
            case OpType::Spacing: {process_spacing();break;}
            case OpType::SameLine: {process_same_line(cmd);break;}
            case OpType::Button: {process_button(cmd);break;}
            case OpType::PushStyleColor: {process_push_style_color(cmd);break;}
            case OpType::PopStyleColor: {process_pop_style_color(cmd);break;}
            case OpType::TextWrapped: {process_text_wrapped(cmd);break;}
            case OpType::RadioButton: {process_radio_button(cmd);break;}
            case OpType::CollapsingHeader: {process_collapsing_header(cmd);break;}
            case OpType::PushItemWidth: {process_push_item_width(cmd);break;}
            case OpType::PopItemWidth: {process_pop_item_width();break;}
            case OpType::SetNextWindowSize: {process_set_next_window_size(cmd);break;}
            case OpType::SetNextWindowPos: {process_set_next_window_pos(cmd);break;}
            case OpType::BeginTooltip: {process_begin_tooltip();break;}
            case OpType::EndTooltip: {process_end_tooltip();break;}
            case OpType::TextDisabled: {process_text_disabled(cmd);break;}
            case OpType::BeginDisabled: {process_begin_disabled(cmd);break;}
            case OpType::EndDisabled: {process_end_disabled();break;}
            case OpType::BeginGroup: {process_begin_group();break;}
            case OpType::EndGroup: {process_end_group();break;}
            case OpType::Indent: {process_indent(cmd);break;}
            case OpType::Unindent: {process_unindent(cmd);break;}
            case OpType::SmallButton: {process_small_button(cmd);break;}
            case OpType::MenuItem: {process_menu_item(cmd);break;}
            case OpType::OpenPopup: {process_open_popup(cmd);break;}
            case OpType::CloseCurrentPopup: {process_close_current_popup();break;}
            case OpType::BeginTable: {process_begin_table(cmd);break;}
            case OpType::EndTable: {process_end_table();break;}
            case OpType::TableSetupColumn: {process_table_setup_column(cmd);break;}
            case OpType::TableNextColumn: {process_table_next_column();break;}
            case OpType::TableNextRow: {process_table_next_row(cmd);break;}
            case OpType::TableSetColumnIndex: {process_table_set_column_index(cmd);break;}
            case OpType::TableHeadersRow: {process_table_headers_row();break;}
            case OpType::BeginChild: {process_begin_child(cmd);break;}
            case OpType::EndChild: {process_end_child();break;}
            case OpType::BeginTabBar: {process_begin_tab_bar(cmd);break;}
            case OpType::EndTabBar: {process_end_tab_bar();break;}
            case OpType::BeginTabItem: {process_begin_tab_item(cmd);break;}
            case OpType::EndTabItem: {process_end_tab_item();break;}
            case OpType::BeginPopup: {process_begin_popup(cmd);break;}
            case OpType::EndPopup: {process_end_popup();break;}
            case OpType::BeginPopupModal: {process_begin_popup_modal(cmd);break;}
            case OpType::EndPopupModal: {process_end_popup_modal();break;}
            case OpType::BeginCombo: {process_begin_combo(cmd);break;}
            case OpType::EndCombo: {process_end_combo();break;}
            case OpType::Selectable: {process_selectable(cmd);break;}
            case OpType::NewLine: {process_new_line();break;}
            case OpType::InvisibleButton: {process_invisible_button(cmd);break;}
            case OpType::ProgressBar: {process_progress_bar(cmd);break;}
            case OpType::Dummy: {process_dummy(cmd);break;}
            case OpType::GetCursorScreenPos: {process_get_cursor_screen_pos(cmd);break;}
            case OpType::SetCursorScreenPos: {process_set_cursor_screen_pos(cmd);break;}
            case OpType::DrawListAddLine: {process_draw_list_add_line(cmd);break;}
            case OpType::DrawListAddRect: {process_draw_list_add_rect(cmd);break;}
            case OpType::DrawListAddRectFilled: {process_draw_list_add_rect_filled(cmd);break;}
            case OpType::DrawListAddText: {process_draw_list_add_text(cmd);break;}
            case OpType::CalcTextSize: {process_calc_text_size(cmd);break;}
            case OpType::GetContentRegionAvail: {process_get_content_region_avail(cmd);break;}
            case OpType::IsItemHovered: {process_is_item_hovered(cmd);break;}
            case OpType::SetClipboardText: {process_set_clipboard_text(cmd);break;}
            case OpType::GetClipboardText: {process_get_clipboard_text(cmd);break;}
            case OpType::ColorEdit4: {process_color_edit4(cmd);break;}
            case OpType::SetTooltip: {process_set_tooltip(cmd);break;}
            case OpType::ShowTooltip: {process_show_tooltip(cmd);break;}
            case OpType::IsWindowCollapsed: {process_is_window_collapsed(cmd);break;}
            case OpType::GetWindowPos: {process_get_window_pos(cmd);break;}
            case OpType::GetWindowSize: {process_get_window_size(cmd);break;}
            case OpType::Checkbox: {process_checkbox(cmd);break;}
			case OpType::InputText: {process_input_text(cmd);break;}
			case OpType::InputInt: {process_input_int(cmd);break;}
			case OpType::InputFloat: {process_input_float(cmd); break;}
            case OpType::SliderInt: {process_slider_int(cmd);break;}
			case OpType::SliderFloat: {process_slider_float(cmd); break;}
            case OpType::Combo: {process_combo(cmd); break;}
            case OpType::PythonCallable: {process_python_callable(cmd);break;}

            default:
                break;
            }
        }

        if (in_window) {
            ImGui::End();
        }
        in_window = false;
        window_visible = true;
        blocked_scope_depth = 0;
        table_scope_stack.clear();
        child_scope_stack.clear();
        tab_bar_scope_stack.clear();
        tab_item_scope_stack.clear();
        popup_scope_stack.clear();
        popup_modal_scope_stack.clear();
        combo_scope_stack.clear();
        const auto sync_t0 = std::chrono::high_resolution_clock::now();
        flush_dirty_slots_to_vars();
        const auto sync_t1 = std::chrono::high_resolution_clock::now();
        perf_slot_sync_ms_ = std::chrono::duration<double, std::milli>(sync_t1 - sync_t0).count();
        const auto render_t1 = std::chrono::high_resolution_clock::now();
        perf_render_total_ms_ = std::chrono::duration<double, std::milli>(render_t1 - render_t0).count();
    }

    Py4GW_UI& python_callable(const py::object& fn) {
        if (!PyCallable_Check(fn.ptr())) {
            throw std::runtime_error("python_callable expects a callable");
        }
        Command cmd;
        cmd.type = OpType::PythonCallable;
        cmd.payload = SigCallable{ fn };
        push_command(std::move(cmd));
        return *this;
    }

    void process_python_callable(Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigCallable>(&cmd.payload)) {
            const auto t0 = std::chrono::high_resolution_clock::now();
            try {
                py::dict vars_dict = build_vars_dict();
                py::object result = p->callable(vars_dict);
                apply_vars_dict(vars_dict);
                if (!result.is_none() && py::isinstance<py::dict>(result)) {
                    apply_vars_dict(py::cast<py::dict>(result));
                }
            }
            catch (const py::error_already_set& e) {
                ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "python_callable error");
                ImGui::TextWrapped("%s", e.what());
            }
            const auto t1 = std::chrono::high_resolution_clock::now();
            perf_python_callable_ms_ += std::chrono::duration<double, std::milli>(t1 - t0).count();
        }
    }

    // Real ImGui signature shape: bool Begin(const char* name, bool* p_open, ImGuiWindowFlags flags)
    Py4GW_UI& begin(const std::string& name, const std::string& p_open_var = "", int flags = 0) {
        Command cmd;
        cmd.type = OpType::Begin;
        cmd.payload = SigStrVarInt{ name, p_open_var, flags, false };
        push_command(std::move(cmd));
        return *this;
    }

    bool process_begin(Command& cmd) {
        if (auto* p = std::get_if<SigStrVarInt>(&cmd.payload)) {
            if (p->str_var.empty()) {
                p->return_value = ImGui::Begin(p->text.c_str(), nullptr, static_cast<ImGuiWindowFlags>(p->value));
            }
            else {
                bool open_value = read_slot_bool(p->slot, p->str_var, true);
                p->return_value = ImGui::Begin(p->text.c_str(), &open_value, static_cast<ImGuiWindowFlags>(p->value));
                write_slot_bool(p->slot, p->str_var, open_value);
            }

            in_window = true;
            window_visible = p->return_value;
            return p->return_value;
        }

        in_window = false;
        window_visible = false;
        return false;
    }


    Py4GW_UI& end() {
        Command cmd;
        cmd.type = OpType::End;
        cmd.payload = std::monostate{};
        push_command(std::move(cmd));
        return *this;
    }

	void process_end() {
        if (in_window) {
            ImGui::End();
        }
        in_window = false;
        window_visible = true;
        blocked_scope_depth = 0;
        table_scope_stack.clear();
        child_scope_stack.clear();
        tab_bar_scope_stack.clear();
        tab_item_scope_stack.clear();
        popup_scope_stack.clear();
        popup_modal_scope_stack.clear();
        combo_scope_stack.clear();
	}


    Py4GW_UI& text(const std::string& value) {
        Command cmd;
        cmd.type = OpType::Text;
        cmd.payload = SigStr{ value };
        push_command(std::move(cmd));
        return *this;
    }

    Py4GW_UI& text_colored(const std::string& value, const std::array<float, 4>& color) {
        Command cmd;
        cmd.type = OpType::TextColored;
        cmd.payload = SigStrColor{ value, color };
        push_command(std::move(cmd));
        return *this;
    }

    Py4GW_UI& text_wrapped(const std::string& value) {
        Command cmd;
        cmd.type = OpType::TextWrapped;
        cmd.payload = SigStr{ value };
        push_command(std::move(cmd));
        return *this;
    }

    Py4GW_UI& bullet_text(const std::string& value) {
        Command cmd;
        cmd.type = OpType::BulletText;
        cmd.payload = SigStr{ value };
        push_command(std::move(cmd));
        return *this;
    }

    Py4GW_UI& separator() {
        Command cmd;
        cmd.type = OpType::Separator;
        cmd.payload = std::monostate{};
        push_command(std::move(cmd));
        return *this;
    }

    Py4GW_UI& spacing() {
        Command cmd;
        cmd.type = OpType::Spacing;
        cmd.payload = std::monostate{};
        push_command(std::move(cmd));
        return *this;
    }

    Py4GW_UI& same_line(float offset_from_start_x = 0.0f, float spacing = -1.0f) {
        Command cmd;
        cmd.type = OpType::SameLine;
        cmd.payload = SigSameLine{ offset_from_start_x, spacing };
        push_command(std::move(cmd));
        return *this;
    }

    Py4GW_UI& button(const std::string& label, const std::string& clicked_var = "", float width = 0.0f, float height = 0.0f) {
        Command cmd;
        cmd.type = OpType::Button;
        cmd.payload = SigButton{ label, clicked_var, width, height };
        push_command(std::move(cmd));
        return *this;
    }

    Py4GW_UI& push_style_color(int idx, const std::array<float, 4>& color) {
        Command cmd;
        cmd.type = OpType::PushStyleColor;
        cmd.payload = SigStyleColor{ idx, color };
        push_command(std::move(cmd));
        return *this;
    }

    Py4GW_UI& pop_style_color(int count = 1) {
        Command cmd;
        cmd.type = OpType::PopStyleColor;
        cmd.payload = SigInt{ count };
        push_command(std::move(cmd));
        return *this;
    }

	void process_text(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStr>(&cmd.payload)) {
            ImGui::TextUnformatted(p->text.c_str());
        }
	}

    void process_text_colored(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStrColor>(&cmd.payload)) {
            ImGui::TextColored(ImVec4(p->color[0], p->color[1], p->color[2], p->color[3]), "%s", p->text.c_str());
        }
    }

    void process_text_wrapped(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStr>(&cmd.payload)) {
            ImGui::TextWrapped("%s", p->text.c_str());
        }
    }

    void process_bullet_text(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStr>(&cmd.payload)) {
            ImGui::BulletText("%s", p->text.c_str());
        }
    }

    void process_separator() {
        if (!can_continue()) return;
        ImGui::Separator();
    }

    void process_spacing() {
        if (!can_continue()) return;
        ImGui::Spacing();
    }

    void process_same_line(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigSameLine>(&cmd.payload)) {
            ImGui::SameLine(p->offset_from_start_x, p->spacing);
        }
    }

    void process_button(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigButton>(&cmd.payload)) {
            const bool pressed = ImGui::Button(p->text.c_str(), ImVec2(p->width, p->height));
            if (!p->str_var.empty()) {
                write_slot_bool(p->slot, p->str_var, pressed);
            }
        }
    }

    void process_push_style_color(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStyleColor>(&cmd.payload)) {
            ImGui::PushStyleColor(static_cast<ImGuiCol>(p->idx), ImVec4(p->color[0], p->color[1], p->color[2], p->color[3]));
        }
    }

    void process_pop_style_color(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigInt>(&cmd.payload)) {
            ImGui::PopStyleColor(std::max(1, p->value));
        }
    }

    Py4GW_UI& radio_button(const std::string& label, const std::string& var_name, int button_index) {
        Command cmd;
        cmd.type = OpType::RadioButton;
        cmd.payload = SigStrVarInt{ label, var_name, button_index, false };
        push_command(std::move(cmd));
        return *this;
    }

    void process_radio_button(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStrVarInt>(&cmd.payload)) {
            int current = read_slot_int(p->slot, p->str_var, 0);
            bool selected = (current == p->value);
            if (ImGui::RadioButton(p->text.c_str(), selected)) {
                current = p->value;
            }
            write_slot_int(p->slot, p->str_var, current);
        }
    }

    Py4GW_UI& collapsing_header(const std::string& label, const std::string& open_var = "", int flags = 0) {
        Command cmd;
        cmd.type = OpType::CollapsingHeader;
        cmd.payload = SigStrVarInt{ label, open_var, flags, false };
        push_command(std::move(cmd));
        return *this;
    }

    void process_collapsing_header(Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStrVarInt>(&cmd.payload)) {
            p->return_value = ImGui::CollapsingHeader(p->text.c_str(), static_cast<ImGuiTreeNodeFlags>(p->value));
            if (!p->str_var.empty()) {
                write_slot_bool(p->slot, p->str_var, p->return_value);
            }
        }
    }

    Py4GW_UI& push_item_width(float width) {
        Command cmd;
        cmd.type = OpType::PushItemWidth;
        cmd.payload = SigFloat{ width };
        push_command(std::move(cmd));
        return *this;
    }

    void process_push_item_width(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigFloat>(&cmd.payload)) {
            ImGui::PushItemWidth(p->value);
        }
    }

    Py4GW_UI& pop_item_width() {
        Command cmd;
        cmd.type = OpType::PopItemWidth;
        cmd.payload = std::monostate{};
        push_command(std::move(cmd));
        return *this;
    }

    void process_pop_item_width() {
        if (!can_continue()) return;
        ImGui::PopItemWidth();
    }

    Py4GW_UI& set_next_window_size(float width, float height, int cond = 0) {
        Command cmd;
        cmd.type = OpType::SetNextWindowSize;
        cmd.payload = SigVec2Int{ width, height, cond };
        push_command(std::move(cmd));
        return *this;
    }

    void process_set_next_window_size(const Command& cmd) {
        if (auto* p = std::get_if<SigVec2Int>(&cmd.payload)) {
            ImGui::SetNextWindowSize(ImVec2(p->x, p->y), static_cast<ImGuiCond>(p->value));
        }
    }

    Py4GW_UI& set_next_window_pos(float x, float y, int cond = 0) {
        Command cmd;
        cmd.type = OpType::SetNextWindowPos;
        cmd.payload = SigVec2Int{ x, y, cond };
        push_command(std::move(cmd));
        return *this;
    }

    void process_set_next_window_pos(const Command& cmd) {
        if (auto* p = std::get_if<SigVec2Int>(&cmd.payload)) {
            ImGui::SetNextWindowPos(ImVec2(p->x, p->y), static_cast<ImGuiCond>(p->value));
        }
    }
    Py4GW_UI& begin_tooltip() {
        Command cmd;
        cmd.type = OpType::BeginTooltip;
        cmd.payload = std::monostate{};
        push_command(std::move(cmd));
        return *this;
    }

    void process_begin_tooltip() {
        if (!can_continue()) return;
        ImGui::BeginTooltip();
    }

    Py4GW_UI& end_tooltip() {
        Command cmd;
        cmd.type = OpType::EndTooltip;
        cmd.payload = std::monostate{};
        push_command(std::move(cmd));
        return *this;
    }

    void process_end_tooltip() {
        if (!can_continue()) return;
        ImGui::EndTooltip();
    }

    Py4GW_UI& text_disabled(const std::string& value) {
        Command cmd;
        cmd.type = OpType::TextDisabled;
        cmd.payload = SigStr{ value };
        push_command(std::move(cmd));
        return *this;
    }

    void process_text_disabled(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStr>(&cmd.payload)) {
            ImGui::TextDisabled("%s", p->text.c_str());
        }
    }

    Py4GW_UI& begin_disabled(bool disabled = true) {
        Command cmd;
        cmd.type = OpType::BeginDisabled;
        cmd.payload = SigBool{ disabled };
        push_command(std::move(cmd));
        return *this;
    }

    void process_begin_disabled(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigBool>(&cmd.payload)) {
            ImGui::BeginDisabled(p->value);
        }
    }

    Py4GW_UI& end_disabled() {
        Command cmd;
        cmd.type = OpType::EndDisabled;
        cmd.payload = std::monostate{};
        push_command(std::move(cmd));
        return *this;
    }

    void process_end_disabled() {
        if (!can_continue()) return;
        ImGui::EndDisabled();
    }

    Py4GW_UI& begin_group() {
        Command cmd;
        cmd.type = OpType::BeginGroup;
        cmd.payload = std::monostate{};
        push_command(std::move(cmd));
        return *this;
    }

    void process_begin_group() {
        if (!can_continue()) return;
        ImGui::BeginGroup();
    }

    Py4GW_UI& end_group() {
        Command cmd;
        cmd.type = OpType::EndGroup;
        cmd.payload = std::monostate{};
        push_command(std::move(cmd));
        return *this;
    }

    void process_end_group() {
        if (!can_continue()) return;
        ImGui::EndGroup();
    }

    Py4GW_UI& indent(float indent_w = 0.0f) {
        Command cmd;
        cmd.type = OpType::Indent;
        cmd.payload = SigFloat{ indent_w };
        push_command(std::move(cmd));
        return *this;
    }

    void process_indent(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigFloat>(&cmd.payload)) {
            ImGui::Indent(p->value);
        }
    }

    Py4GW_UI& unindent(float indent_w = 0.0f) {
        Command cmd;
        cmd.type = OpType::Unindent;
        cmd.payload = SigFloat{ indent_w };
        push_command(std::move(cmd));
        return *this;
    }

    void process_unindent(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigFloat>(&cmd.payload)) {
            ImGui::Unindent(p->value);
        }
    }

    Py4GW_UI& small_button(const std::string& label, const std::string& clicked_var = "") {
        Command cmd;
        cmd.type = OpType::SmallButton;
        cmd.payload = SigButton{ label, clicked_var, 0.0f, 0.0f };
        push_command(std::move(cmd));
        return *this;
    }

    void process_small_button(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigButton>(&cmd.payload)) {
            const bool pressed = ImGui::SmallButton(p->text.c_str());
            if (!p->str_var.empty()) {
                write_slot_bool(p->slot, p->str_var, pressed);
            }
        }
    }

    Py4GW_UI& menu_item(const std::string& label, const std::string& clicked_var = "") {
        Command cmd;
        cmd.type = OpType::MenuItem;
        cmd.payload = SigButton{ label, clicked_var, 0.0f, 0.0f };
        push_command(std::move(cmd));
        return *this;
    }

    void process_menu_item(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigButton>(&cmd.payload)) {
            const bool pressed = ImGui::MenuItem(p->text.c_str());
            if (!p->str_var.empty()) {
                write_slot_bool(p->slot, p->str_var, pressed);
            }
        }
    }

    Py4GW_UI& open_popup(const std::string& popup_id) {
        Command cmd;
        cmd.type = OpType::OpenPopup;
        cmd.payload = SigStr{ popup_id };
        push_command(std::move(cmd));
        return *this;
    }

    void process_open_popup(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStr>(&cmd.payload)) {
            ImGui::OpenPopup(p->text.c_str());
        }
    }

    Py4GW_UI& close_current_popup() {
        Command cmd;
        cmd.type = OpType::CloseCurrentPopup;
        cmd.payload = std::monostate{};
        push_command(std::move(cmd));
        return *this;
    }

    void process_close_current_popup() {
        if (!can_continue()) return;
        ImGui::CloseCurrentPopup();
    }
    Py4GW_UI& begin_table(const std::string& table_id, int columns, int flags = 0) {
        Command cmd;
        cmd.type = OpType::BeginTable;
        cmd.payload = SigStrVarInt{ table_id, "", flags, false };
        if (auto* p = std::get_if<SigStrVarInt>(&cmd.payload)) {
            p->value = flags;
            p->str_var = std::to_string(columns);
        }
        push_command(std::move(cmd));
        return *this;
    }

    void process_begin_table(Command& cmd) {
        if (auto* p = std::get_if<SigStrVarInt>(&cmd.payload)) {
            if (!can_continue()) {
                table_scope_stack.push_back(false);
                return;
            }

            int columns = 1;
            try {
                columns = std::max(1, std::stoi(p->str_var));
            }
            catch (...) {
                columns = 1;
            }

            const bool opened = ImGui::BeginTable(p->text.c_str(), columns, static_cast<ImGuiTableFlags>(p->value));
            table_scope_stack.push_back(opened);
            if (!opened) {
                blocked_scope_depth++;
            }
        }
    }

    Py4GW_UI& end_table() {
        Command cmd;
        cmd.type = OpType::EndTable;
        cmd.payload = std::monostate{};
        push_command(std::move(cmd));
        return *this;
    }

    void process_end_table() {
        if (table_scope_stack.empty()) {
            return;
        }

        const bool opened = table_scope_stack.back();
        table_scope_stack.pop_back();

        if (opened) {
            ImGui::EndTable();
            return;
        }

        if (blocked_scope_depth > 0) {
            blocked_scope_depth--;
        }
    }

    Py4GW_UI& table_setup_column(const std::string& label, int flags = 0, float init_width_or_weight = 0.0f) {
        Command cmd;
        cmd.type = OpType::TableSetupColumn;
        cmd.payload = SigStrIntFloat{ label, flags, init_width_or_weight };
        push_command(std::move(cmd));
        return *this;
    }

    void process_table_setup_column(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStrIntFloat>(&cmd.payload)) {
            ImGui::TableSetupColumn(p->text.c_str(), static_cast<ImGuiTableColumnFlags>(p->value), p->value2);
        }
    }

    Py4GW_UI& table_next_column() {
        Command cmd;
        cmd.type = OpType::TableNextColumn;
        cmd.payload = std::monostate{};
        push_command(std::move(cmd));
        return *this;
    }

    void process_table_next_column() {
        if (!can_continue()) return;
        ImGui::TableNextColumn();
    }

    Py4GW_UI& table_next_row(int row_flags = 0, float min_row_height = 0.0f) {
        Command cmd;
        cmd.type = OpType::TableNextRow;
        cmd.payload = SigIntFloat{ row_flags, min_row_height };
        push_command(std::move(cmd));
        return *this;
    }

    void process_table_next_row(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigIntFloat>(&cmd.payload)) {
            ImGui::TableNextRow(static_cast<ImGuiTableRowFlags>(p->value), p->value2);
        }
    }

    Py4GW_UI& table_set_column_index(int column_n) {
        Command cmd;
        cmd.type = OpType::TableSetColumnIndex;
        cmd.payload = SigInt{ column_n };
        push_command(std::move(cmd));
        return *this;
    }

    void process_table_set_column_index(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigInt>(&cmd.payload)) {
            ImGui::TableSetColumnIndex(p->value);
        }
    }

    Py4GW_UI& table_headers_row() {
        Command cmd;
        cmd.type = OpType::TableHeadersRow;
        cmd.payload = std::monostate{};
        push_command(std::move(cmd));
        return *this;
    }

    void process_table_headers_row() {
        if (!can_continue()) return;
        ImGui::TableHeadersRow();
    }
    Py4GW_UI& begin_child(const std::string& child_id, float width = 0.0f, float height = 0.0f, bool border = false, int flags = 0) {
        Command cmd;
        cmd.type = OpType::BeginChild;
        cmd.payload = SigChild{ child_id, width, height, border, flags };
        push_command(std::move(cmd));
        return *this;
    }

    void process_begin_child(const Command& cmd) {
        if (auto* p = std::get_if<SigChild>(&cmd.payload)) {
            if (!can_continue()) {
                child_scope_stack.push_back(0); // skipped (parent blocked)
                return;
            }

            const bool visible = ImGui::BeginChild(
                p->text.c_str(),
                ImVec2(p->width, p->height),
                p->border,
                static_cast<ImGuiWindowFlags>(p->flags)
            );

            if (!visible) {
                blocked_scope_depth++;
                child_scope_stack.push_back(2); // begun but hidden; EndChild required
            }
            else {
                child_scope_stack.push_back(1); // begun and visible
            }
        }
    }

    Py4GW_UI& end_child() {
        Command cmd;
        cmd.type = OpType::EndChild;
        cmd.payload = std::monostate{};
        push_command(std::move(cmd));
        return *this;
    }

    void process_end_child() {
        if (child_scope_stack.empty()) {
            return;
        }

        const int state = child_scope_stack.back();
        child_scope_stack.pop_back();

        if (state == 0) {
            return;
        }

        ImGui::EndChild();

        if (state == 2 && blocked_scope_depth > 0) {
            blocked_scope_depth--;
        }
    }

    Py4GW_UI& begin_tab_bar(const std::string& str_id, int flags = 0) {
        Command cmd;
        cmd.type = OpType::BeginTabBar;
        cmd.payload = SigStrVarInt{ str_id, "", flags, false };
        push_command(std::move(cmd));
        return *this;
    }

    void process_begin_tab_bar(Command& cmd) {
        if (auto* p = std::get_if<SigStrVarInt>(&cmd.payload)) {
            if (!can_continue()) {
                tab_bar_scope_stack.push_back(0); // skipped
                return;
            }

            const bool opened = ImGui::BeginTabBar(p->text.c_str(), static_cast<ImGuiTabBarFlags>(p->value));
            p->return_value = opened;

            if (!opened) {
                blocked_scope_depth++;
                tab_bar_scope_stack.push_back(2); // blocked by BeginTabBar(false)
            }
            else {
                tab_bar_scope_stack.push_back(1);
            }
        }
    }

    Py4GW_UI& end_tab_bar() {
        Command cmd;
        cmd.type = OpType::EndTabBar;
        cmd.payload = std::monostate{};
        push_command(std::move(cmd));
        return *this;
    }

    void process_end_tab_bar() {
        if (tab_bar_scope_stack.empty()) {
            return;
        }

        const int state = tab_bar_scope_stack.back();
        tab_bar_scope_stack.pop_back();

        if (state == 1) {
            ImGui::EndTabBar();
            return;
        }

        if (state == 2 && blocked_scope_depth > 0) {
            blocked_scope_depth--;
        }
    }

    Py4GW_UI& begin_tab_item(const std::string& label, int flags = 0) {
        Command cmd;
        cmd.type = OpType::BeginTabItem;
        cmd.payload = SigStrVarInt{ label, "", flags, false };
        push_command(std::move(cmd));
        return *this;
    }

    void process_begin_tab_item(Command& cmd) {
        if (auto* p = std::get_if<SigStrVarInt>(&cmd.payload)) {
            if (!can_continue()) {
                tab_item_scope_stack.push_back(0); // skipped
                return;
            }

            const bool opened = ImGui::BeginTabItem(p->text.c_str(), nullptr, static_cast<ImGuiTabItemFlags>(p->value));
            p->return_value = opened;

            if (!opened) {
                blocked_scope_depth++;
                tab_item_scope_stack.push_back(2); // blocked by BeginTabItem(false)
            }
            else {
                tab_item_scope_stack.push_back(1);
            }
        }
    }

    Py4GW_UI& end_tab_item() {
        Command cmd;
        cmd.type = OpType::EndTabItem;
        cmd.payload = std::monostate{};
        push_command(std::move(cmd));
        return *this;
    }

    void process_end_tab_item() {
        if (tab_item_scope_stack.empty()) {
            return;
        }

        const int state = tab_item_scope_stack.back();
        tab_item_scope_stack.pop_back();

        if (state == 1) {
            ImGui::EndTabItem();
            return;
        }

        if (state == 2 && blocked_scope_depth > 0) {
            blocked_scope_depth--;
        }
    }
    Py4GW_UI& begin_with_close(const std::string& name, const std::string& p_open_var, int flags = 0) {
        return begin(name, p_open_var, flags);
    }

    Py4GW_UI& begin_popup(const std::string& popup_id, int flags = 0) {
        Command cmd;
        cmd.type = OpType::BeginPopup;
        cmd.payload = SigStrVarInt{ popup_id, "", flags, false };
        push_command(std::move(cmd));
        return *this;
    }

    void process_begin_popup(Command& cmd) {
        if (auto* p = std::get_if<SigStrVarInt>(&cmd.payload)) {
            if (!can_continue()) {
                popup_scope_stack.push_back(0);
                return;
            }

            const bool opened = ImGui::BeginPopup(p->text.c_str(), static_cast<ImGuiWindowFlags>(p->value));
            p->return_value = opened;
            if (!opened) {
                blocked_scope_depth++;
                popup_scope_stack.push_back(2);
            }
            else {
                popup_scope_stack.push_back(1);
            }
        }
    }

    Py4GW_UI& end_popup() {
        Command cmd;
        cmd.type = OpType::EndPopup;
        cmd.payload = std::monostate{};
        push_command(std::move(cmd));
        return *this;
    }

    void process_end_popup() {
        if (popup_scope_stack.empty()) {
            return;
        }

        const int state = popup_scope_stack.back();
        popup_scope_stack.pop_back();

        if (state == 1) {
            ImGui::EndPopup();
            return;
        }

        if (state == 2 && blocked_scope_depth > 0) {
            blocked_scope_depth--;
        }
    }

    Py4GW_UI& begin_popup_modal(const std::string& name, const std::string& p_open_var = "", int flags = 0) {
        Command cmd;
        cmd.type = OpType::BeginPopupModal;
        cmd.payload = SigStrVarInt{ name, p_open_var, flags, false };
        push_command(std::move(cmd));
        return *this;
    }

    void process_begin_popup_modal(Command& cmd) {
        if (auto* p = std::get_if<SigStrVarInt>(&cmd.payload)) {
            if (!can_continue()) {
                popup_modal_scope_stack.push_back(0);
                return;
            }

            bool open_value = true;
            bool* p_open_ptr = nullptr;
            if (!p->str_var.empty()) {
                open_value = read_slot_bool(p->slot, p->str_var, true);
                p_open_ptr = &open_value;
            }

            const bool opened = ImGui::BeginPopupModal(p->text.c_str(), p_open_ptr, static_cast<ImGuiWindowFlags>(p->value));
            p->return_value = opened;

            if (!p->str_var.empty()) {
                write_slot_bool(p->slot, p->str_var, open_value);
            }

            if (!opened) {
                blocked_scope_depth++;
                popup_modal_scope_stack.push_back(2);
            }
            else {
                popup_modal_scope_stack.push_back(1);
            }
        }
    }

    Py4GW_UI& end_popup_modal() {
        Command cmd;
        cmd.type = OpType::EndPopupModal;
        cmd.payload = std::monostate{};
        push_command(std::move(cmd));
        return *this;
    }

    void process_end_popup_modal() {
        if (popup_modal_scope_stack.empty()) {
            return;
        }

        const int state = popup_modal_scope_stack.back();
        popup_modal_scope_stack.pop_back();

        if (state == 1) {
            ImGui::EndPopup();
            return;
        }

        if (state == 2 && blocked_scope_depth > 0) {
            blocked_scope_depth--;
        }
    }

    Py4GW_UI& begin_combo(const std::string& label, const std::string& preview_value, int flags = 0) {
        Command cmd;
        cmd.type = OpType::BeginCombo;
        cmd.payload = SigStrVarInt{ label, preview_value, flags, false };
        push_command(std::move(cmd));
        return *this;
    }

    void process_begin_combo(Command& cmd) {
        if (auto* p = std::get_if<SigStrVarInt>(&cmd.payload)) {
            if (!can_continue()) {
                combo_scope_stack.push_back(0);
                return;
            }

            const bool opened = ImGui::BeginCombo(p->text.c_str(), p->str_var.c_str(), static_cast<ImGuiComboFlags>(p->value));
            p->return_value = opened;
            if (!opened) {
                blocked_scope_depth++;
                combo_scope_stack.push_back(2);
            }
            else {
                combo_scope_stack.push_back(1);
            }
        }
    }

    Py4GW_UI& end_combo() {
        Command cmd;
        cmd.type = OpType::EndCombo;
        cmd.payload = std::monostate{};
        push_command(std::move(cmd));
        return *this;
    }

    void process_end_combo() {
        if (combo_scope_stack.empty()) {
            return;
        }

        const int state = combo_scope_stack.back();
        combo_scope_stack.pop_back();

        if (state == 1) {
            ImGui::EndCombo();
            return;
        }

        if (state == 2 && blocked_scope_depth > 0) {
            blocked_scope_depth--;
        }
    }

    Py4GW_UI& selectable(const std::string& label, const std::string& var_name, int flags = 0, float width = 0.0f, float height = 0.0f) {
        Command cmd;
        cmd.type = OpType::Selectable;
        cmd.payload = SigSelectable{ label, var_name, flags, width, height };
        push_command(std::move(cmd));
        return *this;
    }

    void process_selectable(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigSelectable>(&cmd.payload)) {
            bool selected = read_slot_bool(p->slot, p->str_var, false);
            const bool pressed = ImGui::Selectable(p->text.c_str(), selected, static_cast<ImGuiSelectableFlags>(p->flags), ImVec2(p->width, p->height));
            if (pressed) {
                selected = !selected;
            }
            write_slot_bool(p->slot, p->str_var, selected);
        }
    }
    Py4GW_UI& new_line() {
        Command cmd;
        cmd.type = OpType::NewLine;
        cmd.payload = std::monostate{};
        push_command(std::move(cmd));
        return *this;
    }

    void process_new_line() {
        if (!can_continue()) return;
        ImGui::NewLine();
    }

    Py4GW_UI& invisible_button(const std::string& label, float width, float height, const std::string& clicked_var = "") {
        Command cmd;
        cmd.type = OpType::InvisibleButton;
        cmd.payload = SigButton{ label, clicked_var, width, height };
        push_command(std::move(cmd));
        return *this;
    }

    void process_invisible_button(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigButton>(&cmd.payload)) {
            const bool pressed = ImGui::InvisibleButton(p->text.c_str(), ImVec2(p->width, p->height));
            if (!p->str_var.empty()) {
                write_slot_bool(p->slot, p->str_var, pressed);
            }
        }
    }

    Py4GW_UI& progress_bar(float fraction, float size_arg = -1.0f, const std::string& overlay = "") {
        Command cmd;
        cmd.type = OpType::ProgressBar;
        cmd.payload = SigProgress{ fraction, size_arg, 0.0f, false, overlay };
        push_command(std::move(cmd));
        return *this;
    }

    Py4GW_UI& progress_bar_ex(float fraction, float size_arg_x, float size_arg_y, const std::string& overlay = "") {
        Command cmd;
        cmd.type = OpType::ProgressBar;
        cmd.payload = SigProgress{ fraction, size_arg_x, size_arg_y, true, overlay };
        push_command(std::move(cmd));
        return *this;
    }

    void process_progress_bar(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigProgress>(&cmd.payload)) {
            const ImVec2 size = p->has_y ? ImVec2(p->size_arg_x, p->size_arg_y) : ImVec2(p->size_arg_x, 0.0f);
            ImGui::ProgressBar(p->fraction, size, p->overlay.empty() ? nullptr : p->overlay.c_str());
        }
    }


    Py4GW_UI& dummy(float width, float height) {
        Command cmd;
        cmd.type = OpType::Dummy;
        cmd.payload = SigVec2{ width, height };
        push_command(std::move(cmd));
        return *this;
    }

    void process_dummy(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigVec2>(&cmd.payload)) {
            ImGui::Dummy(ImVec2(p->x, p->y));
        }
    }

    Py4GW_UI& get_cursor_screen_pos(const std::string& out_x_var, const std::string& out_y_var) {
        Command cmd;
        cmd.type = OpType::GetCursorScreenPos;
        cmd.payload = SigStrVar2{ "", out_x_var, out_y_var };
        push_command(std::move(cmd));
        return *this;
    }

    void process_get_cursor_screen_pos(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStrVar2>(&cmd.payload)) {
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            if (!p->str_var.empty()) write_slot_float(p->slot, p->str_var, pos.x);
            if (!p->str_var2.empty()) write_slot_float(p->slot2, p->str_var2, pos.y);
        }
    }

    Py4GW_UI& set_cursor_screen_pos(float x, float y) {
        Command cmd;
        cmd.type = OpType::SetCursorScreenPos;
        cmd.payload = SigVec2{ x, y };
        push_command(std::move(cmd));
        return *this;
    }

    void process_set_cursor_screen_pos(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigVec2>(&cmd.payload)) {
            ImGui::SetCursorScreenPos(ImVec2(p->x, p->y));
        }
    }

    Py4GW_UI& draw_list_add_line(float x1, float y1, float x2, float y2, int col, float thickness) {
        Command cmd;
        cmd.type = OpType::DrawListAddLine;
        cmd.payload = SigDrawLine{ x1, y1, x2, y2, col, thickness };
        push_command(std::move(cmd));
        return *this;
    }

    void process_draw_list_add_line(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigDrawLine>(&cmd.payload)) {
            if (ImDrawList* dl = ImGui::GetWindowDrawList()) {
                dl->AddLine(ImVec2(p->x1, p->y1), ImVec2(p->x2, p->y2), static_cast<ImU32>(p->color), p->thickness);
            }
        }
    }

    Py4GW_UI& draw_list_add_rect(float x1, float y1, float x2, float y2, int col, float rounding = 0.0f, int rounding_corners_flags = 0, float thickness = 1.0f) {
        Command cmd;
        cmd.type = OpType::DrawListAddRect;
        cmd.payload = SigDrawRect{ x1, y1, x2, y2, col, rounding, rounding_corners_flags, thickness };
        push_command(std::move(cmd));
        return *this;
    }

    void process_draw_list_add_rect(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigDrawRect>(&cmd.payload)) {
            if (ImDrawList* dl = ImGui::GetWindowDrawList()) {
                dl->AddRect(
                    ImVec2(p->x1, p->y1),
                    ImVec2(p->x2, p->y2),
                    static_cast<ImU32>(p->color),
                    p->rounding,
                    static_cast<ImDrawFlags>(p->rounding_corners_flags),
                    p->thickness
                );
            }
        }
    }

    Py4GW_UI& draw_list_add_rect_filled(float x1, float y1, float x2, float y2, int col, float rounding = 0.0f, int rounding_corners_flags = 0) {
        Command cmd;
        cmd.type = OpType::DrawListAddRectFilled;
        cmd.payload = SigDrawRect{ x1, y1, x2, y2, col, rounding, rounding_corners_flags, 1.0f };
        push_command(std::move(cmd));
        return *this;
    }

    void process_draw_list_add_rect_filled(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigDrawRect>(&cmd.payload)) {
            if (ImDrawList* dl = ImGui::GetWindowDrawList()) {
                dl->AddRectFilled(
                    ImVec2(p->x1, p->y1),
                    ImVec2(p->x2, p->y2),
                    static_cast<ImU32>(p->color),
                    p->rounding,
                    static_cast<ImDrawFlags>(p->rounding_corners_flags)
                );
            }
        }
    }

    Py4GW_UI& draw_list_add_text(float x, float y, int col, const std::string& text) {
        Command cmd;
        cmd.type = OpType::DrawListAddText;
        cmd.payload = SigDrawText{ x, y, col, text };
        push_command(std::move(cmd));
        return *this;
    }

    void process_draw_list_add_text(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigDrawText>(&cmd.payload)) {
            if (ImDrawList* dl = ImGui::GetWindowDrawList()) {
                dl->AddText(ImVec2(p->x, p->y), static_cast<ImU32>(p->color), p->text.c_str());
            }
        }
    }

    Py4GW_UI& calc_text_size(const std::string& text, const std::string& out_x_var, const std::string& out_y_var) {
        Command cmd;
        cmd.type = OpType::CalcTextSize;
        cmd.payload = SigStrVar2{ text, out_x_var, out_y_var };
        push_command(std::move(cmd));
        return *this;
    }

    void process_calc_text_size(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStrVar2>(&cmd.payload)) {
            const ImVec2 size = ImGui::CalcTextSize(p->text.c_str());
            if (!p->str_var.empty()) write_slot_float(p->slot, p->str_var, size.x);
            if (!p->str_var2.empty()) write_slot_float(p->slot2, p->str_var2, size.y);
        }
    }

    Py4GW_UI& get_content_region_avail(const std::string& out_x_var, const std::string& out_y_var) {
        Command cmd;
        cmd.type = OpType::GetContentRegionAvail;
        cmd.payload = SigStrVar2{ "", out_x_var, out_y_var };
        push_command(std::move(cmd));
        return *this;
    }

    void process_get_content_region_avail(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStrVar2>(&cmd.payload)) {
            const ImVec2 size = ImGui::GetContentRegionAvail();
            if (!p->str_var.empty()) write_slot_float(p->slot, p->str_var, size.x);
            if (!p->str_var2.empty()) write_slot_float(p->slot2, p->str_var2, size.y);
        }
    }

    Py4GW_UI& is_item_hovered(const std::string& out_var) {
        Command cmd;
        cmd.type = OpType::IsItemHovered;
        cmd.payload = SigStrVarInt{ "", out_var, 0, false };
        push_command(std::move(cmd));
        return *this;
    }

    void process_is_item_hovered(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStrVarInt>(&cmd.payload)) {
            if (!p->str_var.empty()) {
                write_slot_bool(p->slot, p->str_var, ImGui::IsItemHovered());
            }
        }
    }

    Py4GW_UI& set_clipboard_text(const std::string& text) {
        Command cmd;
        cmd.type = OpType::SetClipboardText;
        cmd.payload = SigStr{ text };
        push_command(std::move(cmd));
        return *this;
    }

    void process_set_clipboard_text(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStr>(&cmd.payload)) {
            ImGui::SetClipboardText(p->text.c_str());
        }
    }

    Py4GW_UI& get_clipboard_text(const std::string& out_var) {
        Command cmd;
        cmd.type = OpType::GetClipboardText;
        cmd.payload = SigStrVarInt{ "", out_var, 0, false };
        push_command(std::move(cmd));
        return *this;
    }

    void process_get_clipboard_text(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStrVarInt>(&cmd.payload)) {
            if (!p->str_var.empty()) {
                const char* text = ImGui::GetClipboardText();
                write_slot_string(p->slot, p->str_var, text ? text : "");
            }
        }
    }
    Py4GW_UI& color_edit4(const std::string& label, const std::string& var_name, int flags = 0) {
        Command cmd;
        cmd.type = OpType::ColorEdit4;
        cmd.payload = SigStrVarInt{ label, var_name, flags, false };
        push_command(std::move(cmd));
        return *this;
    }

    void process_color_edit4(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStrVarInt>(&cmd.payload)) {
            std::array<float, 4> color = read_slot_color4(p->slot, p->str_var, { 1.0f, 1.0f, 1.0f, 1.0f });
            ImGui::ColorEdit4(p->text.c_str(), color.data(), static_cast<ImGuiColorEditFlags>(p->value));
            write_slot_color4(p->slot, p->str_var, color);
        }
    }

    Py4GW_UI& set_tooltip(const std::string& text) {
        Command cmd;
        cmd.type = OpType::SetTooltip;
        cmd.payload = SigStr{ text };
        push_command(std::move(cmd));
        return *this;
    }

    void process_set_tooltip(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStr>(&cmd.payload)) {
            ImGui::SetTooltip("%s", p->text.c_str());
        }
    }

    Py4GW_UI& show_tooltip(const std::string& text) {
        Command cmd;
        cmd.type = OpType::ShowTooltip;
        cmd.payload = SigStr{ text };
        push_command(std::move(cmd));
        return *this;
    }

    void process_show_tooltip(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStr>(&cmd.payload)) {
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", p->text.c_str());
            }
        }
    }

    Py4GW_UI& is_window_collapsed(const std::string& out_var) {
        Command cmd;
        cmd.type = OpType::IsWindowCollapsed;
        cmd.payload = SigStrVarInt{ "", out_var, 0, false };
        push_command(std::move(cmd));
        return *this;
    }

    void process_is_window_collapsed(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStrVarInt>(&cmd.payload)) {
            if (!p->str_var.empty()) {
                write_slot_bool(p->slot, p->str_var, ImGui::IsWindowCollapsed());
            }
        }
    }

    Py4GW_UI& get_window_pos(const std::string& out_x_var, const std::string& out_y_var) {
        Command cmd;
        cmd.type = OpType::GetWindowPos;
        cmd.payload = SigStrVar2{ "", out_x_var, out_y_var };
        push_command(std::move(cmd));
        return *this;
    }

    void process_get_window_pos(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStrVar2>(&cmd.payload)) {
            const ImVec2 pos = ImGui::GetWindowPos();
            if (!p->str_var.empty()) write_slot_float(p->slot, p->str_var, pos.x);
            if (!p->str_var2.empty()) write_slot_float(p->slot2, p->str_var2, pos.y);
        }
    }

    Py4GW_UI& get_window_size(const std::string& out_x_var, const std::string& out_y_var) {
        Command cmd;
        cmd.type = OpType::GetWindowSize;
        cmd.payload = SigStrVar2{ "", out_x_var, out_y_var };
        push_command(std::move(cmd));
        return *this;
    }

    void process_get_window_size(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStrVar2>(&cmd.payload)) {
            const ImVec2 size = ImGui::GetWindowSize();
            if (!p->str_var.empty()) write_slot_float(p->slot, p->str_var, size.x);
            if (!p->str_var2.empty()) write_slot_float(p->slot2, p->str_var2, size.y);
        }
    }
    // Real ImGui signature shape: bool Checkbox(const char* label, bool* v)
    Py4GW_UI& checkbox(const std::string& label, const std::string& var_name) {
        Command cmd;
        cmd.type = OpType::Checkbox;
        cmd.payload = SigStrVarInt{ label, var_name, 0, false };
        push_command(std::move(cmd));
        return *this;
    }

    Py4GW_UI& checkbox_slot(const std::string& label, uint32_t slot) {
        Command cmd;
        cmd.type = OpType::Checkbox;
        cmd.payload = SigStrVarInt{ label, slot_name_for(slot), 0, false, slot };
        push_command(std::move(cmd));
        return *this;
    }

    void process_checkbox(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStrVarInt>(&cmd.payload)) {
            bool value = read_slot_bool(p->slot, p->str_var, false);
            ImGui::Checkbox(p->text.c_str(), &value);
            write_slot_bool(p->slot, p->str_var, value);
        }
    }

    // Real ImGui signature shape: bool InputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags)
    Py4GW_UI& input_text(const std::string& label, const std::string& var_name, int flags = 0) {
        Command cmd;
        cmd.type = OpType::InputText;
        cmd.payload = SigStrVarInt{ label, var_name, flags, false };
        push_command(std::move(cmd));
        return *this;
    }

    Py4GW_UI& input_text_slot(const std::string& label, uint32_t slot, int flags = 0) {
        Command cmd;
        cmd.type = OpType::InputText;
        cmd.payload = SigStrVarInt{ label, slot_name_for(slot), flags, false, slot };
        push_command(std::move(cmd));
        return *this;
    }

	void process_input_text(const Command& cmd) {
		if (!can_continue()) return;
        if (auto* p = std::get_if<SigStrVarInt>(&cmd.payload)) {
            const uint32_t slot = p->slot;
            std::string current = read_slot_string(slot, p->str_var, "");

            if (slot != kInvalidSlot && slot < slots_.size()) {
                SlotState& st = slots_[slot];
                if (st.type == SlotType::Unknown) st.type = SlotType::String;
                if (st.text_buf.empty()) {
                    const size_t cap = std::max<size_t>(256, current.size() + 64);
                    st.text_buf.assign(cap, '\0');
                    if (!current.empty()) {
                        std::memcpy(st.text_buf.data(), current.c_str(), std::min(current.size(), cap - 1));
                    }
                }
                ImGui::InputText(
                    p->text.c_str(),
                    st.text_buf.data(),
                    st.text_buf.size(),
                    static_cast<ImGuiInputTextFlags>(p->value)
                );
                write_slot_string(slot, p->str_var, std::string(st.text_buf.data()));
                return;
            }

            const size_t capacity = std::max<size_t>(256, current.size() + 64);
            std::vector<char> buffer(capacity, '\0');
            if (!current.empty()) {
                std::memcpy(buffer.data(), current.c_str(), std::min(current.size(), capacity - 1));
            }
            ImGui::InputText(p->text.c_str(), buffer.data(), buffer.size(), static_cast<ImGuiInputTextFlags>(p->value));
            write_slot_string(slot, p->str_var, std::string(buffer.data()));
        }
	}

    Py4GW_UI& input_int(const std::string& label, const std::string& var_name) {
        Command cmd;
        cmd.type = OpType::InputInt;
        cmd.payload = SigStrVarInt{ label, var_name, 0, false };
        push_command(std::move(cmd));
        return *this;
    }

    Py4GW_UI& input_int_slot(const std::string& label, uint32_t slot) {
        Command cmd;
        cmd.type = OpType::InputInt;
        cmd.payload = SigStrVarInt{ label, slot_name_for(slot), 0, false, slot };
        push_command(std::move(cmd));
        return *this;
    }

	void process_input_int(const Command& cmd) {
		if (!can_continue()) return;
        if (auto* p = std::get_if<SigStrVarInt>(&cmd.payload)) {
            int value = read_slot_int(p->slot, p->str_var, 0);
            ImGui::InputInt(p->text.c_str(), &value);
            write_slot_int(p->slot, p->str_var, value);
        }
	}

    Py4GW_UI& input_float(const std::string& label, const std::string& var_name) {
        Command cmd;
        cmd.type = OpType::InputFloat;
        cmd.payload = SigStrVarInt{ label, var_name, 0, false };
        push_command(std::move(cmd));
        return *this;
    }

    Py4GW_UI& input_float_slot(const std::string& label, uint32_t slot) {
        Command cmd;
        cmd.type = OpType::InputFloat;
        cmd.payload = SigStrVarInt{ label, slot_name_for(slot), 0, false, slot };
        push_command(std::move(cmd));
        return *this;
    }

    void process_input_float(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStrVarInt>(&cmd.payload)) {
            float value = read_slot_float(p->slot, p->str_var, 0.0f);
            ImGui::InputFloat(p->text.c_str(), &value);
            write_slot_float(p->slot, p->str_var, value);
        }
    }

    Py4GW_UI& slider_int(const std::string& label, const std::string& var_name, int v_min, int v_max) {
        Command cmd;
        cmd.type = OpType::SliderInt;
        cmd.payload = SigStrVarIntRange{ label, var_name, v_min, v_max, false };
        push_command(std::move(cmd));
        return *this;
    }

    Py4GW_UI& slider_int_slot(const std::string& label, uint32_t slot, int v_min, int v_max) {
        Command cmd;
        cmd.type = OpType::SliderInt;
        cmd.payload = SigStrVarIntRange{ label, slot_name_for(slot), v_min, v_max, false, slot };
        push_command(std::move(cmd));
        return *this;
    }

    void process_slider_int(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStrVarIntRange>(&cmd.payload)) {
            int value = read_slot_int(p->slot, p->str_var, p->min_value);
            ImGui::SliderInt(p->text.c_str(), &value, p->min_value, p->max_value);
            write_slot_int(p->slot, p->str_var, value);
        }
    }

    Py4GW_UI& slider_float(const std::string& label, const std::string& var_name, float v_min, float v_max) {
        Command cmd;
        cmd.type = OpType::SliderFloat;
        cmd.payload = SigStrVarFloatRange{ label, var_name, v_min, v_max, false };
        push_command(std::move(cmd));
        return *this;
    }

    Py4GW_UI& slider_float_slot(const std::string& label, uint32_t slot, float v_min, float v_max) {
        Command cmd;
        cmd.type = OpType::SliderFloat;
        cmd.payload = SigStrVarFloatRange{ label, slot_name_for(slot), v_min, v_max, false, slot };
        push_command(std::move(cmd));
        return *this;
    }

	void process_slider_float(const Command& cmd) {
		if (!can_continue()) return;
		if (auto* p = std::get_if<SigStrVarFloatRange>(&cmd.payload)) {
			float value = read_slot_float(p->slot, p->str_var, p->min_value);
			ImGui::SliderFloat(p->text.c_str(), &value, p->min_value, p->max_value);
			write_slot_float(p->slot, p->str_var, value);
		}
	}

    Py4GW_UI& combo(const std::string& label, const std::string& var_name, const std::vector<std::string>& items) {
        Command cmd;
        cmd.type = OpType::Combo;
        cmd.payload = SigStrVarItems{ label, var_name, items, false };
        push_command(std::move(cmd));
        return *this;
    }

    Py4GW_UI& combo_slot(const std::string& label, uint32_t slot, const std::vector<std::string>& items) {
        Command cmd;
        cmd.type = OpType::Combo;
        cmd.payload = SigStrVarItems{ label, slot_name_for(slot), items, false, slot };
        push_command(std::move(cmd));
        return *this;
    }

    void process_combo(const Command& cmd) {
        if (!can_continue()) return;
        if (auto* p = std::get_if<SigStrVarItems>(&cmd.payload)) {
            int value = read_slot_int(p->slot, p->str_var, 0);
            if (value < 0) {
                value = 0;
            }

            std::vector<const char*> items_cstr;
            items_cstr.reserve(p->items.size());
            for (const std::string& item : p->items) {
                items_cstr.push_back(item.c_str());
            }
            bool return_value;
            if (!items_cstr.empty()) {
                return_value = ImGui::Combo(p->text.c_str(), &value, items_cstr.data(), static_cast<int>(items_cstr.size()));
            }
            else {
                return_value = false;
            }

            write_slot_int(p->slot, p->str_var, value);
        }
    }

    

};
    
void bind_UI(py::module_& ui) {
    ui.doc() = "Cached UI schema module (prototype)";

    py::class_<Py4GW_UI>(ui, "UI")
        .def(py::init<>())
        .def("clear_ui", &Py4GW_UI::clear_ui)
        .def("clear_vars", &Py4GW_UI::clear_vars)
        .def("set_var", &Py4GW_UI::set_var, py::arg("name"), py::arg("value"))
        .def("vars", &Py4GW_UI::vars, py::arg("name"))
        .def("all_vars", &Py4GW_UI::all_vars)
        .def("register_var", &Py4GW_UI::register_var, py::arg("name"), py::arg("type"), py::arg("initial") = py::none())
        .def("is_finalized", &Py4GW_UI::is_finalized)
        .def("layout_version", &Py4GW_UI::layout_version)
        .def("recording_ignored_count", &Py4GW_UI::recording_ignored_count)
        .def("perf_stats", &Py4GW_UI::perf_stats)
        .def("begin", &Py4GW_UI::begin, py::arg("name"), py::arg("p_open_var") = "", py::arg("flags") = 0, py::return_value_policy::reference_internal)
        .def("end", &Py4GW_UI::end, py::return_value_policy::reference_internal)
        .def("text", &Py4GW_UI::text, py::arg("value"),py::return_value_policy::reference_internal)
        .def("text_colored", &Py4GW_UI::text_colored, py::arg("value"), py::arg("color"), py::return_value_policy::reference_internal)
        .def("bullet_text", &Py4GW_UI::bullet_text, py::arg("value"), py::return_value_policy::reference_internal)
        .def("separator", &Py4GW_UI::separator, py::return_value_policy::reference_internal)
        .def("spacing", &Py4GW_UI::spacing, py::return_value_policy::reference_internal)
        .def("same_line", &Py4GW_UI::same_line, py::arg("offset_from_start_x") = 0.0f, py::arg("spacing") = -1.0f, py::return_value_policy::reference_internal)
        .def("button", &Py4GW_UI::button, py::arg("label"), py::arg("clicked_var") = "", py::arg("width") = 0.0f, py::arg("height") = 0.0f, py::return_value_policy::reference_internal)
        .def("push_style_color", &Py4GW_UI::push_style_color, py::arg("idx"), py::arg("color"), py::return_value_policy::reference_internal)
        .def("pop_style_color", &Py4GW_UI::pop_style_color, py::arg("count") = 1, py::return_value_policy::reference_internal)
        .def("text_wrapped", &Py4GW_UI::text_wrapped, py::arg("value"), py::return_value_policy::reference_internal)
        .def("radio_button", &Py4GW_UI::radio_button, py::arg("label"), py::arg("var_name"), py::arg("button_index"), py::return_value_policy::reference_internal)
        .def("collapsing_header", &Py4GW_UI::collapsing_header, py::arg("label"), py::arg("open_var") = "", py::arg("flags") = 0, py::return_value_policy::reference_internal)
        .def("push_item_width", &Py4GW_UI::push_item_width, py::arg("width"), py::return_value_policy::reference_internal)
        .def("pop_item_width", &Py4GW_UI::pop_item_width, py::return_value_policy::reference_internal)
        .def("set_next_window_size", &Py4GW_UI::set_next_window_size, py::arg("width"), py::arg("height"), py::arg("cond") = 0, py::return_value_policy::reference_internal)
        .def("set_next_window_pos", &Py4GW_UI::set_next_window_pos, py::arg("x"), py::arg("y"), py::arg("cond") = 0, py::return_value_policy::reference_internal)
        .def("begin_tooltip", &Py4GW_UI::begin_tooltip, py::return_value_policy::reference_internal)
        .def("end_tooltip", &Py4GW_UI::end_tooltip, py::return_value_policy::reference_internal)
        .def("text_disabled", &Py4GW_UI::text_disabled, py::arg("value"), py::return_value_policy::reference_internal)
        .def("begin_disabled", &Py4GW_UI::begin_disabled, py::arg("disabled") = true, py::return_value_policy::reference_internal)
        .def("end_disabled", &Py4GW_UI::end_disabled, py::return_value_policy::reference_internal)
        .def("begin_group", &Py4GW_UI::begin_group, py::return_value_policy::reference_internal)
        .def("end_group", &Py4GW_UI::end_group, py::return_value_policy::reference_internal)
        .def("indent", &Py4GW_UI::indent, py::arg("indent_w") = 0.0f, py::return_value_policy::reference_internal)
        .def("unindent", &Py4GW_UI::unindent, py::arg("indent_w") = 0.0f, py::return_value_policy::reference_internal)
        .def("small_button", &Py4GW_UI::small_button, py::arg("label"), py::arg("clicked_var") = "", py::return_value_policy::reference_internal)
        .def("menu_item", &Py4GW_UI::menu_item, py::arg("label"), py::arg("clicked_var") = "", py::return_value_policy::reference_internal)
        .def("open_popup", &Py4GW_UI::open_popup, py::arg("popup_id"), py::return_value_policy::reference_internal)
        .def("close_current_popup", &Py4GW_UI::close_current_popup, py::return_value_policy::reference_internal)
        .def("begin_table", &Py4GW_UI::begin_table, py::arg("table_id"), py::arg("columns"), py::arg("flags") = 0, py::return_value_policy::reference_internal)
        .def("end_table", &Py4GW_UI::end_table, py::return_value_policy::reference_internal)
        .def("table_setup_column", &Py4GW_UI::table_setup_column, py::arg("label"), py::arg("flags") = 0, py::arg("init_width_or_weight") = 0.0f, py::return_value_policy::reference_internal)
        .def("table_next_column", &Py4GW_UI::table_next_column, py::return_value_policy::reference_internal)
        .def("table_next_row", &Py4GW_UI::table_next_row, py::arg("row_flags") = 0, py::arg("min_row_height") = 0.0f, py::return_value_policy::reference_internal)
        .def("table_set_column_index", &Py4GW_UI::table_set_column_index, py::arg("column_n"), py::return_value_policy::reference_internal)
        .def("table_headers_row", &Py4GW_UI::table_headers_row, py::return_value_policy::reference_internal)
        .def("begin_child", &Py4GW_UI::begin_child, py::arg("child_id"), py::arg("width") = 0.0f, py::arg("height") = 0.0f, py::arg("border") = false, py::arg("flags") = 0, py::return_value_policy::reference_internal)
        .def("end_child", &Py4GW_UI::end_child, py::return_value_policy::reference_internal)
        .def("begin_tab_bar", &Py4GW_UI::begin_tab_bar, py::arg("str_id"), py::arg("flags") = 0, py::return_value_policy::reference_internal)
        .def("end_tab_bar", &Py4GW_UI::end_tab_bar, py::return_value_policy::reference_internal)
        .def("begin_tab_item", &Py4GW_UI::begin_tab_item, py::arg("label"), py::arg("flags") = 0, py::return_value_policy::reference_internal)
        .def("end_tab_item", &Py4GW_UI::end_tab_item, py::return_value_policy::reference_internal)
        .def("begin_with_close", &Py4GW_UI::begin_with_close, py::arg("name"), py::arg("p_open_var"), py::arg("flags") = 0, py::return_value_policy::reference_internal)
        .def("begin_popup", &Py4GW_UI::begin_popup, py::arg("popup_id"), py::arg("flags") = 0, py::return_value_policy::reference_internal)
        .def("end_popup", &Py4GW_UI::end_popup, py::return_value_policy::reference_internal)
        .def("begin_popup_modal", &Py4GW_UI::begin_popup_modal, py::arg("name"), py::arg("p_open_var") = "", py::arg("flags") = 0, py::return_value_policy::reference_internal)
        .def("end_popup_modal", &Py4GW_UI::end_popup_modal, py::return_value_policy::reference_internal)
        .def("begin_combo", &Py4GW_UI::begin_combo, py::arg("label"), py::arg("preview_value"), py::arg("flags") = 0, py::return_value_policy::reference_internal)
        .def("end_combo", &Py4GW_UI::end_combo, py::return_value_policy::reference_internal)
        .def("selectable", &Py4GW_UI::selectable, py::arg("label"), py::arg("var_name"), py::arg("flags") = 0, py::arg("width") = 0.0f, py::arg("height") = 0.0f, py::return_value_policy::reference_internal)
        .def("new_line", &Py4GW_UI::new_line, py::return_value_policy::reference_internal)
        .def("invisible_button", &Py4GW_UI::invisible_button, py::arg("label"), py::arg("width"), py::arg("height"), py::arg("clicked_var") = "", py::return_value_policy::reference_internal)
        .def("progress_bar", &Py4GW_UI::progress_bar, py::arg("fraction"), py::arg("size_arg") = -1.0f, py::arg("overlay") = "", py::return_value_policy::reference_internal)
        .def("progress_bar_ex", &Py4GW_UI::progress_bar_ex, py::arg("fraction"), py::arg("size_arg_x"), py::arg("size_arg_y"), py::arg("overlay") = "", py::return_value_policy::reference_internal)
        .def("dummy", &Py4GW_UI::dummy, py::arg("width"), py::arg("height"), py::return_value_policy::reference_internal)
        .def("get_cursor_screen_pos", &Py4GW_UI::get_cursor_screen_pos, py::arg("out_x_var"), py::arg("out_y_var"), py::return_value_policy::reference_internal)
        .def("set_cursor_screen_pos", &Py4GW_UI::set_cursor_screen_pos, py::arg("x"), py::arg("y"), py::return_value_policy::reference_internal)
        .def("draw_list_add_line", &Py4GW_UI::draw_list_add_line, py::arg("x1"), py::arg("y1"), py::arg("x2"), py::arg("y2"), py::arg("col"), py::arg("thickness"), py::return_value_policy::reference_internal)
        .def("draw_list_add_rect", &Py4GW_UI::draw_list_add_rect, py::arg("x1"), py::arg("y1"), py::arg("x2"), py::arg("y2"), py::arg("col"), py::arg("rounding") = 0.0f, py::arg("rounding_corners_flags") = 0, py::arg("thickness") = 1.0f, py::return_value_policy::reference_internal)
        .def("draw_list_add_rect_filled", &Py4GW_UI::draw_list_add_rect_filled, py::arg("x1"), py::arg("y1"), py::arg("x2"), py::arg("y2"), py::arg("col"), py::arg("rounding") = 0.0f, py::arg("rounding_corners_flags") = 0, py::return_value_policy::reference_internal)
        .def("draw_list_add_text", &Py4GW_UI::draw_list_add_text, py::arg("x"), py::arg("y"), py::arg("col"), py::arg("text"), py::return_value_policy::reference_internal)
        .def("calc_text_size", &Py4GW_UI::calc_text_size, py::arg("text"), py::arg("out_x_var"), py::arg("out_y_var"), py::return_value_policy::reference_internal)
        .def("get_content_region_avail", &Py4GW_UI::get_content_region_avail, py::arg("out_x_var"), py::arg("out_y_var"), py::return_value_policy::reference_internal)
        .def("is_item_hovered", &Py4GW_UI::is_item_hovered, py::arg("out_var"), py::return_value_policy::reference_internal)
        .def("set_clipboard_text", &Py4GW_UI::set_clipboard_text, py::arg("text"), py::return_value_policy::reference_internal)
        .def("get_clipboard_text", &Py4GW_UI::get_clipboard_text, py::arg("out_var"), py::return_value_policy::reference_internal)
        .def("color_edit4", &Py4GW_UI::color_edit4, py::arg("label"), py::arg("var_name"), py::arg("flags") = 0, py::return_value_policy::reference_internal)
        .def("set_tooltip", &Py4GW_UI::set_tooltip, py::arg("text"), py::return_value_policy::reference_internal)
        .def("show_tooltip", &Py4GW_UI::show_tooltip, py::arg("text"), py::return_value_policy::reference_internal)
        .def("is_window_collapsed", &Py4GW_UI::is_window_collapsed, py::arg("out_var"), py::return_value_policy::reference_internal)
        .def("get_window_pos", &Py4GW_UI::get_window_pos, py::arg("out_x_var"), py::arg("out_y_var"), py::return_value_policy::reference_internal)
        .def("get_window_size", &Py4GW_UI::get_window_size, py::arg("out_x_var"), py::arg("out_y_var"), py::return_value_policy::reference_internal)
        .def("checkbox", &Py4GW_UI::checkbox,py::arg("label"), py::arg("var_name"),py::return_value_policy::reference_internal)
        .def("checkbox_slot", &Py4GW_UI::checkbox_slot, py::arg("label"), py::arg("slot"), py::return_value_policy::reference_internal)
        .def("input_text", &Py4GW_UI::input_text,py::arg("label"), py::arg("var_name"), py::arg("flags") = 0,py::return_value_policy::reference_internal)
        .def("input_text_slot", &Py4GW_UI::input_text_slot, py::arg("label"), py::arg("slot"), py::arg("flags") = 0, py::return_value_policy::reference_internal)
        .def("input_int", &Py4GW_UI::input_int,py::arg("label"), py::arg("var_name"),py::return_value_policy::reference_internal)
        .def("input_int_slot", &Py4GW_UI::input_int_slot, py::arg("label"), py::arg("slot"), py::return_value_policy::reference_internal)
        .def("input_float", &Py4GW_UI::input_float, py::arg("label"), py::arg("var_name"),py::return_value_policy::reference_internal)
        .def("input_float_slot", &Py4GW_UI::input_float_slot, py::arg("label"), py::arg("slot"), py::return_value_policy::reference_internal)
        .def("slider_int", &Py4GW_UI::slider_int,py::arg("label"), py::arg("var_name"), py::arg("v_min"), py::arg("v_max"),py::return_value_policy::reference_internal)
        .def("slider_int_slot", &Py4GW_UI::slider_int_slot, py::arg("label"), py::arg("slot"), py::arg("v_min"), py::arg("v_max"), py::return_value_policy::reference_internal)
        .def("slider_float", &Py4GW_UI::slider_float,py::arg("label"), py::arg("var_name"), py::arg("v_min"), py::arg("v_max"),py::return_value_policy::reference_internal)
        .def("slider_float_slot", &Py4GW_UI::slider_float_slot, py::arg("label"), py::arg("slot"), py::arg("v_min"), py::arg("v_max"), py::return_value_policy::reference_internal)
        .def("combo", &Py4GW_UI::combo, py::arg("label"), py::arg("var_name"), py::arg("items"),py::return_value_policy::reference_internal)
        .def("combo_slot", &Py4GW_UI::combo_slot, py::arg("label"), py::arg("slot"), py::arg("items"), py::return_value_policy::reference_internal)
        .def("python_callable", &Py4GW_UI::python_callable, py::arg("fn"),py::return_value_policy::reference_internal)
        .def("finalize", &Py4GW_UI::finalize)
        .def("render", &Py4GW_UI::render);



























}
