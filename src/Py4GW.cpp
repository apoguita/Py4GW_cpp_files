#include "Py4GW.h"

#include "Headers.h"
#include <iostream>

//HeroAI* Py4GW::heroAI = nullptr;

namespace py = pybind11;

#define GAME_CMSG_INTERACT_GADGET (0x004F)      // 79
#define GAME_CMSG_SEND_SIGNPOST_DIALOG (0x0051) // 81

// Initialize the Python interpreter when the DLL is loaded
int test_result = -1;
bool script_loaded = false;

std::vector<std::string> command_history;
int history_pos = -1; // -1 means new line

ImGuiTextFilter log_filter;
bool show_timestamps = true;
bool auto_scroll = true;

Timer script_timer;
Timer script_timer2;

struct DeferredMixedCommand {
    std::function<void()> action;
    Timer timer;
    int delay_ms;
    bool active = false;
};

DeferredMixedCommand mixed_deferred;

// Initialize merchant interaction (setup callbacks)

void Py4GW::OnPriceReceived(uint32_t item_id, uint32_t price)
{
    quoted_item_id = item_id;
    quoted_value = price;
    // DetachTransactionListeners();  // Detach listeners after receiving price

    // Optionally, you can immediately buy the item here:
    // BuyItem(item_id, price, 1);  // Buying 1 item
}

// Callback when a transaction is complete
void Py4GW::OnTransactionComplete()
{
    transaction_complete = true;
    // IsRequesting = false;  // Allow further operations
    // DetachTransactionListeners();  // Detach listeners after transaction completes
}

void Py4GW::OnNormalMerchantItemsReceived(GW::Packet::StoC::WindowItems* pak)
{
    // Clear the previous list of items
    if (reset_merchant_window_item.hasElapsed(1000)) {
        merchant_window_items.clear();
        reset_merchant_window_item.reset();
    }

    // Store the new list of item IDs from the packet
    for (uint32_t i = 0; i < pak->count; ++i) {
        merchant_window_items.push_back(pak->item_ids[i]);
    }


    // Optionally, log the items received or notify another system
    // For example:
    // Py4GW::Console.Log("Normal Merchant Items received: " + std::to_string(merchant_window_items.size()));
}

void Py4GW::OnNormalMerchantItemsStreamEnd(GW::Packet::StoC::WindowItemsEnd* pak)
{
    // Optionally, handle stream-end logic, such as finalizing processing
    // For now, we can just log or clear certain states
    // Example: Py4GW::Console.Log("Normal Merchant Items stream ended.");
}

void Py4GW::DetachTransactionListeners()
{
    GW::StoC::RemoveCallback(GW::Packet::StoC::TransactionDone::STATIC_HEADER, &TransactionComplete_Entry);
    GW::StoC::RemoveCallback(GW::Packet::StoC::QuotedItemPrice::STATIC_HEADER, &QuotedItemPrice_Entry);
}

void Py4GW::InitializeMerchantCallbacks()
{
    // Register callback for quoted item prices (for material merchants)
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::QuotedItemPrice>(&QuotedItemPrice_Entry, [this](GW::HookStatus*, GW::Packet::StoC::QuotedItemPrice* pak) {
        OnPriceReceived(pak->itemid, pak->price);
        });

    // Register callback for completed transactions
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::TransactionDone>(&TransactionComplete_Entry, [this](GW::HookStatus*, GW::Packet::StoC::TransactionDone* pak) {
        OnTransactionComplete();
        });

    // Register callback for ItemStreamEnd to get merchant items
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ItemStreamEnd>(&ItemStreamEnd_Entry, [this](GW::HookStatus*, const GW::Packet::StoC::ItemStreamEnd* pak) {
        if (pak->unk1 != 12) { // 12 means we're in the "buy" tab
            return;
        }
        GW::MerchItemArray* items = GW::Merchant::GetMerchantItemsArray();
        merch_items.clear();
        if (items) {
            for (const auto item_id : *items) {
                merch_items.push_back(item_id);
            }
        }
        });

    // Register callback for normal merchant items (WindowItems)
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::WindowItems>(&WindowItems_Entry, [this](GW::HookStatus*, GW::Packet::StoC::WindowItems* pak) {
        OnNormalMerchantItemsReceived(pak);
        });

    // Register callback for the end of the normal merchant items stream (WindowItemsEnd)
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::WindowItemsEnd>(&WindowItemsEnd_Entry, [this](GW::HookStatus*, GW::Packet::StoC::WindowItemsEnd* pak) {
        OnNormalMerchantItemsStreamEnd(pak);
        });
}



bool Py4GW::Initialize() {
    py::initialize_interpreter();
    InitializeMerchantCallbacks();

    DebugMessage(L"Py4GW, Initialized.");
 
	return true;
    
}

void Py4GW::Terminate() {
    GW::DisableHooks();
    GW::Terminate();
}

bool p_open = true;
bool scroll_to_bottom = false;
ImGuiTextBuffer buffer;
ImVector<int> line_offsets;

void ScrollToBottom()
{
    scroll_to_bottom = true;
}

void Clear()
{
    buffer.clear();
    line_offsets.clear();
    line_offsets.push_back(0);
}

enum class MessageType {
    Info,        // General information
    Warning,     // Warnings about potential issues
    Error,       // Non-fatal errors
    Debug,       // Debugging information
    Success,     // Successful operation
    Performance, // Performance and profiling logs
    Notice       // Notices and tips
};

struct LogEntry {
    std::string timestamp;
    std::string module_name;
    std::string message;
    MessageType message_type;
};

std::vector<LogEntry> log_entries;

std::string TrimString(const std::string& str) {
    const std::string WHITESPACE = " \t\r\n\v\f\u00A0"; // Includes Unicode spaces
    size_t first = str.find_first_not_of(WHITESPACE);
    if (first == std::string::npos) return ""; // If all spaces or empty, return ""

    size_t last = str.find_last_not_of(WHITESPACE);
    std::string trimmed = str.substr(first, (last - first + 1));

    std::string result;
    bool in_space = false;

    for (char c : trimmed) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!in_space) {
                result += ' '; // Add only one space
                in_space = true;
            }
        }
        else {
            result += c;
            in_space = false;
        }
    }

    return result;
}

void Log(const std::string& module_name, const std::string& message, MessageType type = MessageType::Info)
{
    LogEntry entry;

    // Get the current time (you already have this code)
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S");
    entry.timestamp = ss.str();

    // Set the module name, message, and type
    entry.module_name = module_name;
    entry.message = message;
    entry.message_type = type;

    // Add the entry to the log
    log_entries.push_back(entry);

    if (!show_console) {
	    std::string timestamp = GW::Chat::FormatChatMessage("[" + entry.timestamp + "]", 180, 180, 180);
	    std::string module = GW::Chat::FormatChatMessage("[" + entry.module_name + "]", 100, 190, 255);
    
        class rgb {
	    public:
		    int r, g, b;
		    rgb(int r, int g, int b) : r(r), g(g), b(b) {}
	    } color(255, 255, 255);
    
        switch (type) {
        case MessageType::Info:         color = { 255, 255, 255 }; break;
        case MessageType::Error:        color = { 255, 0, 0 }; break;
        case MessageType::Warning:      color = { 255, 255, 0 }; break;
        case MessageType::Success:      color = { 0, 255, 0 }; break;
        case MessageType::Debug:        color = { 0, 255, 255 }; break;
        case MessageType::Performance:  color = { 255, 153, 0 }; break;
        case MessageType::Notice:       color = { 153, 255, 153 }; break;
        default:                        color = { 255, 255, 255 }; break;
        }

	    std::string formatted_message = message;
        std::replace(formatted_message.begin(), formatted_message.end(), '\\', '/');
        std::string formatted_chunk = GW::Chat::FormatChatMessage(formatted_message, color.r, color.g, color.b);
        
        formatted_chunk = timestamp + " " + module + " " + formatted_chunk;

        GW::Chat::SendFakeChat(GW::Chat::Channel::CHANNEL_EMOTE, formatted_chunk);
    }
}

// Function to load and execute Python scripts
std::string LoadPythonScript(const std::string& file_path)
{
    std::ifstream script_file(file_path);
    if (!script_file.is_open()) {
        Log("Py4GW", "Failed to open script file: " + file_path, MessageType::Error);
        return "";
    }

    std::stringstream v_buffer;
    v_buffer << script_file.rdbuf();

    if (v_buffer.str().empty()) {
        Log("Py4GW", "Script file is empty: " + file_path, MessageType::Error);
    }

    return v_buffer.str();
}

void SaveLogToFile(const std::string& filename)
{
    std::ofstream out_file(filename);
    if (!out_file) {
        Log("Py4GW", "Failed to open file for writing.", MessageType::Error);
        return;
    }

    for (const auto& entry : log_entries) {
        std::string full_message;
        if (show_timestamps) {
            full_message += "[" + entry.timestamp + "] ";
        }
        full_message += "[" + entry.module_name + "] " + entry.message;
        out_file << full_message << std::endl;
    }

    Log("Py4GW", "Log saved to " + filename);
}

std::string OpenFileDialog()
{
    OPENFILENAME ofn;
    char sz_file[260] = { 0 };
    HWND hwnd = nullptr;
    HANDLE hf;

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = sz_file;
    ofn.nMaxFile = sizeof(sz_file);
    ofn.lpstrFilter = "Python Files\0*.PY\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn) == TRUE) {
        return std::string(ofn.lpstrFile);
    }
    else {
        return "";
    }
}

std::string SaveFileDialog()
{
    OPENFILENAME ofn;
    char sz_file[260] = { 0 };
    HWND hwnd = nullptr;

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = sz_file;
    ofn.nMaxFile = sizeof(sz_file);
    ofn.lpstrFilter = "Text Files\0*.txt\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileName(&ofn) == TRUE) {
        return std::string(ofn.lpstrFile);
    }
    else {
        return "";
    }
}

enum class ScriptState { Stopped, Running, Paused };

ScriptState script_state = ScriptState::Stopped;

static char script_path[260] = "";
std::string script_content;
py::object custom_scope;
py::object script_module;
py::object main_function;

ScriptState script_state2 = ScriptState::Stopped;

std::string script_path2 = "";
std::string script_content2;
py::object custom_scope2;
py::object script_module2;
py::object main_function2;

bool LoadAndExecuteScriptOnce()
{
    if (strlen(script_path) == 0) {
        Log("Py4GW", "Script path is empty.", MessageType::Error);
        return false;
    }

    script_content = LoadPythonScript(script_path);
    if (script_content.empty()) {
        Log("Py4GW", "Failed to load script.", MessageType::Error);
        return false;
    }

    try {
        // Compile the script first to detect any syntax errors
        py::module_ py_compile = py::module_::import("py_compile");
        try {
            py_compile.attr("compile")(script_path, py::none(), py::none(), py::bool_(true));
            Log("Py4GW", "Script compiled successfully.", MessageType::Notice);
        }
        catch (const py::error_already_set& e) {
            std::cerr << "Python syntax error during script compilation: " << e.what() << std::endl;
            Log("Py4GW", "Python syntax error: " + std::string(e.what()), MessageType::Error);
            script_state = ScriptState::Stopped;
            return false;
        }

        // Isolate Python execution logic in its own try-catch
        py::object types_module = py::module_::import("types");
        script_module = types_module.attr("ModuleType")("script_module");

        try {
            py::exec(script_content, script_module.attr("__dict__"));
        }
        catch (const py::error_already_set& e) {
            // If a syntax error or any other Python error happens, log and stop
            std::cerr << "Python error during script execution: " << e.what() << std::endl;
            Log("Py4GW", "Python error: " + std::string(e.what()), MessageType::Error);
            script_state = ScriptState::Stopped;
            return false;
        }

        if (py::hasattr(script_module, "main")) {
            main_function = script_module.attr("main");
            Log("Py4GW", "main() function found.", MessageType::Notice);
            return true;
        }
        else {
            script_state = ScriptState::Stopped;
            Log("Py4GW", "main() function not found in the script.", MessageType::Error);
        }
    }
    catch (const py::error_already_set& e) {
        // This will catch other Python errors not related to the script execution itself
        std::cerr << "Python error: " << e.what() << std::endl;
        Log("Py4GW", "Python error: " + std::string(e.what()), MessageType::Error);
        script_state = ScriptState::Stopped;
    }
    catch (const std::exception& e) {
        // Catch any standard C++ exceptions
        std::cerr << "Standard exception: " << e.what() << std::endl;
        Log("Py4GW", "Standard exception: " + std::string(e.what()), MessageType::Error);
        script_state = ScriptState::Stopped;
    }
    catch (...) {
        // Catch-all for any unknown exceptions
        std::cerr << "Unknown error occurred during script execution." << std::endl;
        Log("Py4GW", "Unknown error occurred during script execution.", MessageType::Error);
        script_state = ScriptState::Stopped;
    }
    return false;
}

bool LoadAndExecuteScriptOnce2()
{
    if (script_path2.length() == 0) {
        Log("Py4GW", "Main Menu Script path is empty.", MessageType::Error);
        return false;
    }

    script_content2 = LoadPythonScript(script_path2);
    if (script_content2.empty()) {
        Log("Py4GW", "Failed to load Main Menu script.", MessageType::Error);
        return false;
    }

    try {
        // Compile the script first to detect any syntax errors
        py::module_ py_compile = py::module_::import("py_compile");
        try {
            py_compile.attr("compile")(script_path2, py::none(), py::none(), py::bool_(true));
            //Log("Py4GW", "Script compiled successfully.", MessageType::Notice);
        }
        catch (const py::error_already_set& e) {
            std::cerr << "Python syntax error during script compilation: " << e.what() << std::endl;
            Log("Py4GW", "Python syntax error in Main Menu: " + std::string(e.what()), MessageType::Error);
            script_state2 = ScriptState::Stopped;
            return false;
        }

        // Isolate Python execution logic in its own try-catch
        py::object types_module = py::module_::import("types");
        script_module2 = types_module.attr("ModuleType")("script_module");

        try {
            py::exec(script_content2, script_module2.attr("__dict__"));
        }
        catch (const py::error_already_set& e) {
            // If a syntax error or any other Python error happens, log and stop
            std::cerr << "Python error during script execution: " << e.what() << std::endl;
            Log("Py4GW", "Python error, Main Menu: " + std::string(e.what()), MessageType::Error);
            script_state2 = ScriptState::Stopped;
            return false;
        }

        if (py::hasattr(script_module2, "main")) {
            main_function2 = script_module2.attr("main");
            //Log("Py4GW", "main() function found.", MessageType::Notice);
            return true;
        }
        else {
            script_state2 = ScriptState::Stopped;
            Log("Py4GW", "main() function not found in the Main Menu script.", MessageType::Error);
        }
    }
    catch (const py::error_already_set& e) {
        // This will catch other Python errors not related to the script execution itself
        std::cerr << "Python error: " << e.what() << std::endl;
        Log("Py4GW", "Python error Main Menu: " + std::string(e.what()), MessageType::Error);
        script_state2 = ScriptState::Stopped;
    }
    catch (const std::exception& e) {
        // Catch any standard C++ exceptions
        std::cerr << "Standard exception: " << e.what() << std::endl;
        Log("Py4GW", "Standard exception Main Menu: " + std::string(e.what()), MessageType::Error);
        script_state2 = ScriptState::Stopped;
    }
    catch (...) {
        // Catch-all for any unknown exceptions
        std::cerr << "Unknown error occurred during script execution." << std::endl;
        Log("Py4GW", "Unknown error occurred during script execution.", MessageType::Error);
        script_state2 = ScriptState::Stopped;
    }
    return false;
}


void ExecutePythonScript()
{
    try {
        main_function();
    }
    catch (const py::error_already_set& e) {
        script_state = ScriptState::Stopped;
        Log("Py4GW", "Python error: " + std::string(e.what()), MessageType::Error);
    }
}

void ExecutePythonScript2()
{
    try {
        main_function2();
    }
    catch (const py::error_already_set& e) {
        script_state2 = ScriptState::Stopped;
        Log("Py4GW", "Python error (Main Menu)" + std::string(e.what()), MessageType::Error);
    }
}

void ResetScriptEnvironment()
{
    script_content.clear();
    script_state = ScriptState::Stopped;
    main_function = py::object();
    script_module = py::object();
    Log("Py4GW", "Python environment reset.", MessageType::Notice);
}

void ResetScriptEnvironment2()
{
    script_content2.clear();
    script_state2 = ScriptState::Stopped;
    main_function2 = py::object();
    script_module2 = py::object();
    Log("Py4GW", "Python environment Main Menu reset.", MessageType::Notice);
}

void ExecutePythonCommand(const std::string& command)
{
    try {
        if (!script_module) {
            script_module = py::module_::import("__main__");
        }

        py::object result = py::eval(command, script_module.attr("__dict__"));
        std::string result_str = py::str(result).cast<std::string>();
        Log("Python", ">>> " + command);

        if (!result.is_none()) {
            Log("Python", result_str);
        }
    }
    catch (const py::error_already_set& e) {
        Log("Python", "Error executing command: " + command + "\n" + e.what(), MessageType::Error);
    }
    catch (const std::exception& e) {
        Log("Python", "Standard error executing command: " + command + "\n" + std::string(e.what()), MessageType::Error);
    }
    catch (...) {
        Log("Python", "Unknown error executing command: " + command, MessageType::Error);
    }
}

int TextEditCallback(ImGuiInputTextCallbackData* data)
{
    switch (data->EventFlag) {
    case ImGuiInputTextFlags_CallbackHistory: {
        const int prev_history_pos = history_pos;
        if (data->EventKey == ImGuiKey_UpArrow) {
            if (history_pos == -1)
                history_pos = static_cast<int>(command_history.size()) - 1;
            else if (history_pos > 0)
                history_pos--;
        }
        else if (data->EventKey == ImGuiKey_DownArrow) {
            if (history_pos != -1)
                if (++history_pos >= static_cast<int>(command_history.size())) history_pos = -1;
        }

        // Update the input buffer
        if (prev_history_pos != history_pos) {
            const char* history_str = (history_pos >= 0) ? command_history[history_pos].c_str() : "";
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, history_str);
        }
    } break;
    }
    return 0;
}

void CopyLogToClipboard()
{
    std::string all_text;
    for (const auto& entry : log_entries) {
        std::string full_message;
        if (show_timestamps) {
            full_message += "[" + entry.timestamp + "] ";
        }
        full_message += "[" + entry.module_name + "] " + entry.message + "\n";
        all_text += full_message;
    }
    ImGui::SetClipboardText(all_text.c_str());
    Log("Py4GW", "Console output copied to clipboard.", MessageType::Notice);
}

void ShowTooltipInternal(const char* tooltipText)
{
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("%s", tooltipText);
        ImGui::EndTooltip();
    }
}

bool show_console = false;

ImVec2 console_pos = ImVec2(5, 30);
ImVec2 console_size = ImVec2(800, 700);
bool console_collapsed = false;
bool show_compact_console_ini = false;

ImVec2 compact_console_pos = ImVec2(5, 30);
bool compact_console_collapsed = false;



void DrawConsole(const char* title, bool* new_p_open = nullptr)
{
    
    ImGui::SetNextWindowPos(console_pos, ImGuiCond_Once);
    ImGui::SetNextWindowSize(console_size, ImGuiCond_Once);
    ImGui::SetNextWindowCollapsed(console_collapsed, ImGuiCond_Once);

    // Start the main window
    if (!ImGui::Begin(title, new_p_open)) {
        ImGui::End();
        return;
    }

    // Top row options (Script Path and Buttons)
    if (ImGui::BeginTable("ScriptOptionsTable", 4, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableNextRow();

        // Script Path Input (disable if script is running)
        ImGui::TableSetColumnIndex(0);
        ImGui::SetNextItemWidth(-FLT_MIN);

        if (script_state == ScriptState::Running) {
            ImGui::BeginDisabled(); // Disable input if script is running
        }
        ImGui::InputText("##Path", script_path, IM_ARRAYSIZE(script_path));
        if (script_state == ScriptState::Running) {
            ImGui::EndDisabled(); // Re-enable input after the disabled block
        }

        // Browse Button (disable if script is running)
        ImGui::TableSetColumnIndex(1);
        if (script_state == ScriptState::Running) {
            ImGui::BeginDisabled(); // Disable the button
        }
        if (ImGui::Button(ICON_FA_FOLDER_OPEN "##Open")) {
            std::string selected_file_path = OpenFileDialog();
            if (!selected_file_path.empty()) {
                strcpy(script_path, selected_file_path.c_str());
                Log("Py4GW", "Selected script: " + selected_file_path, MessageType::Notice);
                script_state = ScriptState::Stopped;
            }
        }
        if (script_state == ScriptState::Running) {
            ImGui::EndDisabled(); // Re-enable the button
        }
        ShowTooltipInternal("Select a Python script");

        // Control Buttons (Load, Run, Pause, Stop)
        ImGui::TableSetColumnIndex(2);
        if (strlen(script_path) > 0) {
            if (script_state == ScriptState::Stopped) {
                if (ImGui::Button(ICON_FA_PLAY "##Load & Run")) {
                    if (LoadAndExecuteScriptOnce()) {
                        script_state = ScriptState::Running;
                        script_timer.reset(); // Reset and start the timer
                        Log("Py4GW", "Script started.", MessageType::Notice);
                    }
                    else {
                        ResetScriptEnvironment();
                        script_state = ScriptState::Stopped;
                        script_timer.stop(); // Stop the timer
                        Log("Py4GW", "Script stopped.", MessageType::Notice);
                    }
                }
                ShowTooltipInternal("Load and run script");
            }
            else if (script_state == ScriptState::Running) {
                if (ImGui::Button(ICON_FA_PAUSE "##Pause")) {
                    script_state = ScriptState::Paused;
                    script_timer.Pause(); // Pause the timer
                    Log("Py4GW", "Script paused.", MessageType::Notice);
                }
                ShowTooltipInternal("Pause execution");
            }
            else if (script_state == ScriptState::Paused) {
                if (ImGui::Button(ICON_FA_PLAY "##Resume")) {
                    script_state = ScriptState::Running;
                    script_timer.Resume(); // Resume the timer
                    Log("Py4GW", "Script resumed.", MessageType::Notice);
                }
                ShowTooltipInternal("Resume execution");
            }

            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_STOP "##Reset")) {
                ResetScriptEnvironment();
                script_state = ScriptState::Stopped;
                script_timer.stop(); // Stop the timer
                Log("Py4GW", "Script stopped.", MessageType::Notice);
            }
            ShowTooltipInternal("Reset environment");
        }

        ImGui::EndTable();
    }

    // Add buttons for handling console output
    if (ImGui::BeginTable("ConsoleControlsTable", 6, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableNextRow();

        // Clear Console Button
        ImGui::TableSetColumnIndex(0);
        if (ImGui::Button("Clear")) {
            log_entries.clear();
        }
        ShowTooltipInternal("Clear the console output");

        // Save Log Button
        ImGui::TableSetColumnIndex(1);
        if (ImGui::Button("Save Log")) {
            std::string save_path = SaveFileDialog();
            if (!save_path.empty()) {
                SaveLogToFile(save_path);
            }
        }
        ShowTooltipInternal("Save console output to file");

        // Copy All Button
        ImGui::TableSetColumnIndex(2);
        if (ImGui::Button("Copy All")) {
            CopyLogToClipboard();
        }
        ShowTooltipInternal("Copy console output to clipboard");

        ImGui::TableNextColumn();
        if (ImGui::Button(ICON_FA_WINDOW_MAXIMIZE "##MaximizeFULL")) {
            show_console = !show_console;
            Log("Py4GW", "Toggled Compact Cosole.", MessageType::Notice);
        }
        if (show_console) {
            ShowTooltipInternal("Hide Console");
        }
        else {
            ShowTooltipInternal("Show Compact Console");
        }

        /*
        // Toggle Timestamp Checkbox
        ImGui::TableSetColumnIndex(3);
        ImGui::Checkbox("Timestamps", &show_timestamps);
        ShowTooltipInternal("Show or hide timestamps in the console output");
        */

        // Auto-Scroll Checkbox
        ImGui::TableSetColumnIndex(4);
        ImGui::Checkbox("Auto-Scroll", &auto_scroll);
        ShowTooltipInternal("Toggle auto-scrolling of console output");
        ImGui::EndTable();
    }
    if (ImGui::BeginTable("ConsoleFilterTable", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableNextRow();
        // Filter Input
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Filter:");
        ImGui::TableSetColumnIndex(1);
        log_filter.Draw("##LogFilter", -FLT_MIN);
        ImGui::EndTable();
    }

    ImGui::Separator();

    // Main console area with adjusted size for the status bar
    ImGui::BeginChild("ConsoleArea", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), false, ImGuiWindowFlags_HorizontalScrollbar);

    if (ImGui::BeginPopupContextWindow()) {
        if (ImGui::Selectable("Clear")) {
            log_entries.clear();
        }
        ImGui::EndPopup();
    }

    // Display each log entry with different colors
    for (const auto& entry : log_entries) {
        // Apply filter to check if the log should be displayed
        std::string full_message = "[" + entry.timestamp + "] [" + entry.module_name + "] " + entry.message;
        if (!log_filter.PassFilter(full_message.c_str())) continue;

        // Set color for the timestamp (gray)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f)); // Gray for timestamp
        ImGui::Text("[%s]", entry.timestamp.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();

        // Set color for the module name (light blue)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.75f, 1.0f, 1.0f)); // Light blue for module name
        ImGui::Text("[%s]", entry.module_name.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();

        // Set color based on the message type
        switch (entry.message_type) {
        case MessageType::Error:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); // Red for errors
            break;
        case MessageType::Warning:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f)); // Yellow for warnings
            break;
        case MessageType::Success:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green for success
            break;
        case MessageType::Debug:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 1.0f, 1.0f)); // Cyan for debug
            break;
        case MessageType::Performance:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.0f, 1.0f)); // Orange for performance
            break;
        case MessageType::Notice:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 1.0f, 0.6f, 1.0f)); // Light green for notices
            break;
        case MessageType::Info:
        default:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); // White for info
            break;
        }

        // Display the message
        ImGui::TextUnformatted(entry.message.c_str());
        ImGui::PopStyleColor();
    }

    if (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();

    // Command-line input
    ImGui::Separator();
    static char input_buf[256] = "";
    ImGui::PushItemWidth(-1);

    ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory;

    if (ImGui::InputText("##CommandInput", input_buf, IM_ARRAYSIZE(input_buf), input_text_flags, [](ImGuiInputTextCallbackData* data) -> int {
        return TextEditCallback(data);
        })) {
        std::string command(input_buf);
        if (!command.empty()) {
            ExecutePythonCommand(command);
            command_history.push_back(command);
            history_pos = -1;
            strcpy(input_buf, "");
        }
    }
    ImGui::PopItemWidth();

    // STATUS BAR

    // Get current time from the custom timer
    double elapsed_time_ms = script_timer.getElapsedTime();
    int minutes = static_cast<int>(elapsed_time_ms) / 60000;
    int seconds = (static_cast<int>(elapsed_time_ms) % 60000) / 1000;

    ImGui::Separator();                                                                   // Separate status bar from the console area
    ImGui::BeginChild("StatusBar", ImVec2(0, ImGui::GetFrameHeightWithSpacing()), false); // Status bar with fixed height

    // Display Script Status
    ImGui::Text("Status: ");
    ImGui::SameLine();
    switch (script_state) {
    case ScriptState::Running:
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Running");
        break;
    case ScriptState::Paused:
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Paused");
        break;
    case ScriptState::Stopped:
    default:
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Stopped");
        break;
    }

    // Display the elapsed time from the timer
    ImGui::SameLine();
    ImGui::Text(" | Script Time: %02d:%02d", minutes, seconds);

    ImGui::EndChild(); // Close the status bar child

    ImGui::End(); // Close the main window
}




void DrawCompactConsole(bool* new_p_open = nullptr) {
    ImGui::SetNextWindowPos(compact_console_pos, ImGuiCond_Once);
    ImGui::SetNextWindowCollapsed(compact_console_collapsed, ImGuiCond_Once);

    // Compact console window with fixed auto-resizing
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_AlwaysAutoResize;
    if (!ImGui::Begin("Py4GW##compactPy4GWconsole", new_p_open, flags)) {
        ImGui::End();
        return;
    }

    // Table for Script Path Input + Browse Button
    if (ImGui::BeginTable("compactPy4GWconsoletable", 2, ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("InputColumn");
        ImGui::TableSetupColumn("ButtonColumn");

        ImGui::TableNextRow();

        // First Column: Script Path Input
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(70);
        ImGui::InputText("##Path", script_path, IM_ARRAYSIZE(script_path));
        if (ImGui::IsItemHovered() && strlen(script_path) > 0) {
            ShowTooltipInternal(script_path);
        }

        // Second Column: Browse Button
        ImGui::TableNextColumn();
        if (ImGui::Button(ICON_FA_FOLDER_OPEN "##Open", ImVec2(30, 30))) {
            std::string selected_file_path = OpenFileDialog();
            if (!selected_file_path.empty()) {
                strcpy(script_path, selected_file_path.c_str());
                Log("Py4GW", "Selected script: " + selected_file_path, MessageType::Notice);
                script_state = ScriptState::Stopped;
            }
        }
		ShowTooltipInternal("Open Python script");

        ImGui::EndTable();
    }

    // Table for Action Buttons
    if (ImGui::BeginTable("compactPy4GWButtonTable", 3)) {
        ImGui::TableNextColumn();
        if (script_state == ScriptState::Stopped) {
            if (ImGui::Button(ICON_FA_PLAY "##Run", ImVec2(30, 30))) {
                if (LoadAndExecuteScriptOnce()) {
                    script_state = ScriptState::Running;
                    script_timer.reset();
                    Log("Py4GW", "Script started.", MessageType::Notice);
                }
                else {
                    ResetScriptEnvironment();
                    script_state = ScriptState::Stopped;
                    script_timer.stop();
                    Log("Py4GW", "Script stopped.", MessageType::Notice);
                }
				ShowTooltipInternal("Load and run script");
            }
        }
        else if (script_state == ScriptState::Running) {
            if (ImGui::Button(ICON_FA_PAUSE "##Pause", ImVec2(30, 30))) {
                script_state = ScriptState::Paused;
                script_timer.Pause();
                Log("Py4GW", "Script paused.", MessageType::Notice);
            }
			ShowTooltipInternal("Pause execution");
        }
        else if (script_state == ScriptState::Paused) {
            if (ImGui::Button(ICON_FA_PLAY "##Resume", ImVec2(30, 30))) {
                script_state = ScriptState::Running;
                script_timer.Resume();
                Log("Py4GW", "Script resumed.", MessageType::Notice);
            }
			ShowTooltipInternal("Resume execution");
        }

        ImGui::TableNextColumn();
        if (ImGui::Button(ICON_FA_STOP "##Stop", ImVec2(30, 30))) {
            ResetScriptEnvironment();
            script_state = ScriptState::Stopped;
            script_timer.stop();
            Log("Py4GW", "Script stopped.", MessageType::Notice);
        }
		ShowTooltipInternal("Stop execution");

        ImGui::TableNextColumn();
        if (ImGui::Button(ICON_FA_WINDOW_MAXIMIZE "##Maximize", ImVec2(30, 30))) {
			show_console = !show_console;
            Log("Py4GW", "Toggled Full Cosole.", MessageType::Notice);
        }
		if (show_console) {
			ShowTooltipInternal("Hide Full Console");
		}
		else {
			ShowTooltipInternal("Show Full Console");
		}

        // Second row
        ImGui::TableNextColumn();
        if (ImGui::Button(ICON_FA_STICKY_NOTE "##StickyNote", ImVec2(30, 30))) {
            log_entries.clear();
        }
        ShowTooltipInternal("Clear the console output");

        ImGui::TableNextColumn();
        if (ImGui::Button(ICON_FA_SAVE "##Save", ImVec2(30, 30))) {
            std::string save_path = SaveFileDialog();
            if (!save_path.empty()) {
                SaveLogToFile(save_path);
            }
        }
		ShowTooltipInternal("Save console output to file");

        ImGui::TableNextColumn();
        if (ImGui::Button(ICON_FA_COPY "##Copy", ImVec2(30, 30))) {
            CopyLogToClipboard();
        }
		ShowTooltipInternal("Copy console output to clipboard");

        ImGui::EndTable();
    }

    ImGui::Separator();

    // Display Script Status (Red if Stopped, Green if Running)
    ImVec4 status_color = (script_state == ScriptState::Running) ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1);
    ImGui::TextColored(status_color, script_state == ScriptState::Running ? "Running" : "Stopped");

    ImGui::End();
}


std::string GetCredits()
{ 
    return "Py4GW v2.0.0, Apoguita - 2024,2025";
}

std::string GetLicense()
{
    return std::string("MIT License\n\n") + "Copyright " + GetCredits() +
        "\n\n"
        "Permission is hereby granted, free of charge, to any person obtaining a copy\n"
        "of this software and associated documentation files (the \"Software\"), to deal\n"
        "in the Software without restriction, including without limitation the rights\n"
        "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
        "copies of the Software, and to permit persons to whom the Software is\n"
        "furnished to do so, subject to the following conditions:\n\n"
        "The above copyright notice and this permission notice shall be included in\n"
        "all copies or substantial portions of the Software.\n\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
        "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
        "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
        "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
        "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
        "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN\n"
        "THE SOFTWARE.\n";
}

GlobalMouseClass GlobalMouse;


bool AllowedToRender() {
    const auto map = GW::Map::GetMapInfo();
    const auto instance_type = GW::Map::GetInstanceType();

    if (!map) return false;

    if (map->GetIsPvP()) return false;

    if (map->GetIsGuildHall()) {
        return instance_type == GW::Constants::InstanceType::Outpost;
    }

    return true;
}

bool ChangeWorkingDirectory(const std::string& new_directory) {
    std::wstring wide_directory = std::wstring(new_directory.begin(), new_directory.end());
    return SetCurrentDirectoryW(wide_directory.c_str()) != 0;
}

bool HeroAI_Initialized = false;

void Py4GW::Update() {
    /*
    if (HeroAI_Initialized) {
        if (heroAI->IsAIEnabled() && AllowedToRender()) {
            heroAI->Update();
        }
    }
    */
}


class KeyHandler {
    HWND targetWindow;
public:
	KeyHandler() {
		targetWindow = gw_client_window_handle;
	}

    void set_target_window(HWND windowHandle) {
        targetWindow = windowHandle;
    }

    HWND get_target_window() const {
        return targetWindow;
    }

    /*
    void send_key(int virtualKeyCode, bool isKeyUp = false) {
        if (!targetWindow) return;

        // Create the LPARAM with scan code and other flags
        LPARAM lParam = 1; // Repeat count set to 1
        lParam |= MapVirtualKey(virtualKeyCode, MAPVK_VK_TO_VSC) << 16; // Add scan code
        if (isKeyUp) lParam |= 0xC0000000; // Key-up flag and previous key state

        // Send the message directly to the target window
        SendMessage(targetWindow, isKeyUp ? WM_KEYUP : WM_KEYDOWN, virtualKeyCode, lParam);
    }*/

    bool is_extended_key(int vk) {
        switch (vk) {
        case VK_RIGHT:
        case VK_LEFT:
        case VK_UP:
        case VK_DOWN:
        case VK_INSERT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:  // Page Up
        case VK_NEXT:   // Page Down
        case VK_NUMLOCK:
        case VK_DIVIDE: // Numpad Divide
        case VK_RETURN: // Only if it's the numpad Enter
            return true;
        default:
            return false;
        }
    }


    void send_key(int virtualKeyCode, bool isKeyUp = false) {
        if (!targetWindow) return;

        // Base lParam setup
        LPARAM lParam = 1; // Repeat count = 1
        lParam |= MapVirtualKey(virtualKeyCode, MAPVK_VK_TO_VSC) << 16;

        // Check if it's an extended key
        if (is_extended_key(virtualKeyCode)) {
            lParam |= 0x01000000;  // Set Extended Key flag
        }

        if (isKeyUp) {
            lParam |= 0xC0000000;  // Key-up flag and previous key state
        }

        SendMessage(targetWindow, isKeyUp ? WM_KEYUP : WM_KEYDOWN, virtualKeyCode, lParam);
    }


    // Press a single key
    void press_key(int virtualKeyCode) {
        send_key(virtualKeyCode, false);
    }

    // Release a single key
    void release_key(int virtualKeyCode) {
        send_key(virtualKeyCode, true);
    }


    // Press and release a key (push key)
    void push_key(int virtualKeyCode) {
        press_key(virtualKeyCode);
        Sleep(100); // Small delay to mimic key press duration
        release_key(virtualKeyCode);
    }

    // Press a combination of keys
    void press_key_combo(const std::vector<int>& keys) {
        for (int key : keys) {
            press_key(key);
        }
    }

    // Release a combination of keys
    void release_key_combo(const std::vector<int>& keys) {
        for (int key : keys) {
            release_key(key);
        }
    }

    // Push a combination of keys
    void push_key_combo(const std::vector<int>& keys) {
        press_key_combo(keys);
        Sleep(100); // Mimic a real delay
        release_key_combo(keys);
    }


};

class MouseHandler {
    HWND targetWindow;

public:
    MouseHandler() {
        targetWindow = gw_client_window_handle;
    }

    void set_target_window(HWND windowHandle) {
        targetWindow = windowHandle;
    }

    HWND get_target_window() const {
        return targetWindow;
    }

    // Move mouse virtually within client area (no real cursor movement)
    void MoveMouse(int x, int y) {
        if (!targetWindow) return;

        LPARAM lParam = (y << 16) | (x & 0xFFFF);
        PostMessage(targetWindow, WM_MOUSEMOVE, 0, lParam);
    }

    // Click (Left=0, Right=1, Middle=2)
    void Click(int button = 0, int x = 0, int y = 0) {
        if (!targetWindow) return;

        PressButton(button, x, y);
        ReleaseButton(button, x, y);
    }

    // Double Click
    void DoubleClick(int button = 0, int x = 0, int y = 0) {
        if (!targetWindow) return;

        LPARAM lParam = (y << 16) | (x & 0xFFFF);
        UINT msg;

        if (button == 2)
            msg = WM_MBUTTONDBLCLK;
        else if (button == 1)
            msg = WM_RBUTTONDBLCLK;
        else
            msg = WM_LBUTTONDBLCLK;

        PostMessage(targetWindow, msg, 0, lParam);
    }

    // Press button down
    void PressButton(int button = 0, int x = 0, int y = 0) {
        if (!targetWindow) return;

        LPARAM lParam = (y << 16) | (x & 0xFFFF);
        UINT msg;

        if (button == 2)
            msg = WM_MBUTTONDOWN;
        else if (button == 1)
            msg = WM_RBUTTONDOWN;
        else
            msg = WM_LBUTTONDOWN;

        PostMessage(targetWindow, msg, 0, lParam);
    }

    // Release button
    void ReleaseButton(int button = 0, int x = 0, int y = 0) {
        if (!targetWindow) return;

        LPARAM lParam = (y << 16) | (x & 0xFFFF);
        UINT msg;

        if (button == 2)
            msg = WM_MBUTTONUP;
        else if (button == 1)
            msg = WM_RBUTTONUP;
        else
            msg = WM_LBUTTONUP;

        PostMessage(targetWindow, msg, 0, lParam);
    }

    // Scroll (positive = up, negative = down)
    void Scroll(int delta, int x = 0, int y = 0) {
        if (!targetWindow) return;

        LPARAM lParam = (y << 16) | (x & 0xFFFF);
        WPARAM wParam = (delta << 16);  // Scroll amount in high word

        PostMessage(targetWindow, WM_MOUSEWHEEL, wParam, lParam);
    }
};





bool debug = false;

bool show_modal = false;
bool modal_result_ok = false;
bool first_run = true;
bool console_open = true;

bool check_login_screen = true;


void Py4GW::Draw(IDirect3DDevice9* device) {

    if (!g_d3d_device)
        g_d3d_device = device;

	std::string autoexec_file_path = "";

    if (first_run) {
        ChangeWorkingDirectory(dllDirectory);
        script_path2 = dllDirectory + "/Py4GW_widget_manager.py";
    
        // === INI HANDLING (early load) ===
        IniHandler ini_handler;
        if (ini_handler.Load("Py4GW.ini")) {
            // Layout
            console_pos.x = std::stof(ini_handler.Get("expanded_console", "pos_x", "50"));
            console_pos.y = std::stof(ini_handler.Get("expanded_console", "pos_y", "50"));
            console_size.x = std::stof(ini_handler.Get("expanded_console", "width", "400"));
            console_size.y = std::stof(ini_handler.Get("expanded_console", "height", "300"));
            console_collapsed = ini_handler.Get("expanded_console", "expanded_console_collapsed", "0") == "1";

            compact_console_pos.x = std::stof(ini_handler.Get("compact_console", "pos_x", "50"));
            compact_console_pos.y = std::stof(ini_handler.Get("compact_console", "pos_y", "50"));
            compact_console_collapsed = ini_handler.Get("compact_console", "collapsed", "0") == "1";

            show_compact_console_ini = ini_handler.Get("settings", "show_compact_console", "0") == "1";

            show_console = !show_compact_console_ini;

            // Autoexec script path (optional)
            autoexec_file_path = ini_handler.Get("settings", "autoexec_script", "");
            if (!autoexec_file_path.empty()) {
                strcpy(script_path, autoexec_file_path.c_str());
            }
        }
    }

	bool is_map_loading = GW::Map::GetInstanceType() == GW::Constants::InstanceType::Loading;

	if (show_console || is_map_loading) {
		DrawConsole("Py4GW Console", &console_open);
	}
	else {
		DrawCompactConsole(&console_open);
	}
    //DrawConsole("Py4GW Console", &console_open);
    
    if (first_run) {
        first_run = false;
        console_open = true;
		modal_result_ok = false;
		dll_shutdown = false;
		show_modal = false;

        Initialize();
        Log("Py4GW", GetCredits(), MessageType::Success);
        Log("Py4GW", "Python interpreter initialized.", MessageType::Success);

        reset_merchant_window_item.start();

        if (LoadAndExecuteScriptOnce2()) {
            script_state2 = ScriptState::Running;
            script_timer2.reset(); // Reset and start the timer
            //Log("Py4GW", "Script started.", MessageType::Notice);
        }
        else {
            ResetScriptEnvironment2();
            script_state2 = ScriptState::Stopped;
            script_timer2.stop(); // Stop the timer
            //Log("Py4GW", "Script stopped.", MessageType::Notice);
        }
        

        if (!autoexec_file_path.empty()) {
            strcpy(script_path, autoexec_file_path.c_str());
            Log("Py4GW", "Selected script: " + autoexec_file_path, MessageType::Notice);
            script_state = ScriptState::Stopped;

            if (LoadAndExecuteScriptOnce()) {
                script_state = ScriptState::Running;
                script_timer.reset(); // Reset and start the timer
                Log("Py4GW", "Script started.", MessageType::Notice);
            }
            else {
                ResetScriptEnvironment();
                script_state = ScriptState::Stopped;
                script_timer.stop(); // Stop the timer
                Log("Py4GW", "Script stopped.", MessageType::Notice);
            }
        }

	}

	//if (!AllowedToRender()) {return;}

    if (debug) {
        if (ImGui::Begin("debug window")) {
            const auto& io = ImGui::GetIO();
            bool want_capture_mouse = io.WantCaptureMouse;
            bool want_capture_keyboard = io.WantCaptureKeyboard;
            bool want_text_input = io.WantTextInput;

		    ImGui::Text("WantCaptureMouse: %d", want_capture_mouse);
		    ImGui::Text("WantCaptureKeyboard: %d", want_capture_keyboard);
		    ImGui::Text("WantTextInput: %d", want_text_input);
		    ImGui::Text("Is dragging: %d", is_dragging);
		    ImGui::Text("Is dragging ImGui: %d", is_dragging_imgui);
		    ImGui::Text("dragging_initialized: %d", dragging_initialized);
        }
	    ImGui::End();
    }

	if ((!console_open) && (!show_modal)) {
		show_modal = true;
        ImGui::OpenPopup("Shutdown");
	}

    // Display the modal
    if (ImGui::BeginPopupModal("Shutdown", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to shutdown?");
        ImGui::Separator();

        // OK Button
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            modal_result_ok = true;  // Set result to OK
            show_modal = false;     // Close modal
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();

        // Cancel Button
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            modal_result_ok = false; // Set result to Cancel
            show_modal = false;      // Close modal
            console_open = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (modal_result_ok && !dll_shutdown) {
        DebugMessage(L"Closing Py4GW");
        dll_shutdown = true;
        modal_result_ok = false; // Reset modal result state
    }

    // Check if the script is in the running state and the script content is loaded
    if (script_state == ScriptState::Running && !script_content.empty()) {
        ExecutePythonScript();
    }
    
	if (script_state2 == ScriptState::Running && !script_content2.empty()) {
		ExecutePythonScript2();
	}

    if (mixed_deferred.active && mixed_deferred.timer.hasElapsed(mixed_deferred.delay_ms)) {
        if (mixed_deferred.action) {
            mixed_deferred.action();
        }
        mixed_deferred.active = false;
    }

    
}

// Wrapper functions for script control
bool Py4GW_LoadScript(const std::string& path) {
    strcpy(script_path, path.c_str());
    return LoadAndExecuteScriptOnce();
}

bool Py4GW_RunScript() {
    if (script_state == ScriptState::Stopped) {
        if (LoadAndExecuteScriptOnce()) {
            script_state = ScriptState::Running;
            script_timer.reset();
            Log("Py4GW", "Script started from binding.", MessageType::Notice);
            return true;
        }
    }
    return false;
}

void Py4GW_StopScript() {
    ResetScriptEnvironment();
    script_state = ScriptState::Stopped;
    script_timer.stop();
    Log("Py4GW", "Script stopped from binding.", MessageType::Notice);
}

bool Py4GW_PauseScript() {
    if (script_state == ScriptState::Running) {
        script_state = ScriptState::Paused;
        script_timer.Pause();
        Log("Py4GW", "Script paused from binding.", MessageType::Notice);
        return true;
    }
    return false;
}

bool Py4GW_ResumeScript() {
    if (script_state == ScriptState::Paused) {
        script_state = ScriptState::Running;
        script_timer.Resume();
        Log("Py4GW", "Script resumed from binding.", MessageType::Notice);
        return true;
    }
    return false;
}

std::string Py4GW_GetScriptStatus() {
    switch (script_state) {
    case ScriptState::Running: return "Running";
    case ScriptState::Paused:  return "Paused";
    case ScriptState::Stopped: return "Stopped";
    default: return "Unknown";
    }
}


void ScheduleDeferredAction(std::function<void()> fn, int delay_ms) {
    mixed_deferred.action = fn;
    mixed_deferred.delay_ms = delay_ms;
    mixed_deferred.timer.reset();
    mixed_deferred.timer.start();
    mixed_deferred.active = true;
}

// === Mixed deferred wrappers ===
void Py4GW_DeferLoadAndRun(const std::string& path, int delay_ms) {
    ScheduleDeferredAction([path]() {
        strcpy(script_path, path.c_str());
        if (LoadAndExecuteScriptOnce()) {
            script_state = ScriptState::Running;
            script_timer.reset();
            Log("Py4GW", "Deferred: script loaded and started.", MessageType::Notice);
        }
        }, delay_ms);
}

void Py4GW_DeferStopLoadAndRun(const std::string& path, int delay_ms) {
    ScheduleDeferredAction([path]() {
        Py4GW_StopScript();
        strcpy(script_path, path.c_str());
        if (LoadAndExecuteScriptOnce()) {
            script_state = ScriptState::Running;
            script_timer.reset();
            Log("Py4GW", "Deferred: stopped, loaded and started.", MessageType::Notice);
        }
        }, delay_ms);
}

void Py4GW_DeferStopAndRun(int delay_ms) {
    ScheduleDeferredAction([]() {
        Py4GW_StopScript();
        Py4GW_RunScript();
        Log("Py4GW", "Deferred: stopped and restarted.", MessageType::Notice);
        }, delay_ms);
}


HWND Py4GW::get_gw_window_handle() { 
    return GW::MemoryMgr::GetGWWindowHandle();
    //return gw_window_handle; 
}

void ResizeWindow(int width, int height) {
    const HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
    if (!hwnd) return;

    RECT rect;
    GetWindowRect(hwnd, &rect);
    MoveWindow(hwnd, rect.left, rect.top, width, height, TRUE);
}

void MoveWindowTo(int x, int y) {
    const HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
    if (!hwnd) return;

    RECT rect;
    GetWindowRect(hwnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    MoveWindow(hwnd, x, y, width, height, TRUE);
}

void SetWindowGeometry(int x, int y, int width, int height) {
    const HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
    if (!hwnd) return;

    RECT rect;
    GetWindowRect(hwnd, &rect);

    if (x < 0) x = rect.left;
    if (y < 0) y = rect.top;
    if (width < 0)  width = rect.right - rect.left;
    if (height < 0) height = rect.bottom - rect.top;

    MoveWindow(hwnd, x, y, width, height, TRUE);
}


std::tuple<int, int, int, int> GetWindowRectFn() {
    HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
    RECT rect;
    if (hwnd && GetWindowRect(hwnd, &rect)) {
        return { rect.left, rect.top, rect.right, rect.bottom };
    }
    return { 0, 0, 0, 0 };
}

std::tuple<int, int, int, int> GetClientRectFn() {
	HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
	RECT rect;
	if (hwnd && GetClientRect(hwnd, &rect)) {
		return { rect.left, rect.top, rect.right, rect.bottom };
	}
	return { 0, 0, 0, 0 };
}

void SetWindowTitle(const std::string& title) {
    HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
    if (!hwnd || title.empty()) return;

    // Step 1: update the window title buffer
    SendMessageA(hwnd, WM_SETTEXT, 0, (LPARAM)title.c_str());

    // Step 2: force Windows to redraw the caption itself
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    HDC hdc = GetWindowDC(hwnd);
    if (hdc) {
        RECT rect;
        GetWindowRect(hwnd, &rect);
        OffsetRect(&rect, -rect.left, -rect.top); // normalize to (0,0)
        // Ask Windows to draw the caption using its buffer
        DrawCaption(hwnd, hdc, &rect, DC_TEXT);
        ReleaseDC(hwnd, hdc);
    }

    // Step 3: read back for confirmation
    char buf[256] = { 0 };
    GetWindowTextA(hwnd, buf, sizeof(buf));
    MessageBoxA(nullptr, buf, "After Set + Redraw", MB_OK);
}


bool IsWindowActive() {
    return GW::MemoryMgr::GetGWWindowHandle() == GetActiveWindow();
}

bool IsWindowMinimized() {
    const HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
    return hwnd && IsIconic(hwnd);
}

bool IsWindowInBackground() {
    const HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
    return hwnd && GetActiveWindow() != hwnd;
}

void SetBorderless(bool enable) {
    const HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
    if (!hwnd) return;

    LONG style = GetWindowLong(hwnd, GWL_STYLE);

    if (enable) {
        style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU);
    }
    else {
        style |= (WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU);
    }

    SetWindowLong(hwnd, GWL_STYLE, style);

    // apply changes
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
}




PYBIND11_EMBEDDED_MODULE(Py4GW, m)
{
    m.doc() = "Py4GW, Python Enabler Library for GuildWars"; // Optional module docstring
    
    py::class_<Timer>(m, "Timer")
        .def(py::init<>())                               // Expose the constructor
        .def("start", &Timer::start)                     // Bind the start method
        .def("stop", &Timer::stop)                       // Bind the stop method
        .def("pause", &Timer::Pause)                     // Bind the Pause method
        .def("resume", &Timer::Resume)                   // Bind the Resume method
        .def("is_stopped", &Timer::isStopped)            // Bind the isStopped method
        .def("is_running", &Timer::isRunning)            // Bind the isRunning method
        .def("is_paused", &Timer::IsPaused)              // Bind the IsPaused method
        .def("reset", &Timer::reset)                     // Bind the reset method
        .def("get_elapsed_time", &Timer::getElapsedTime) // Bind the getElapsedTime method
        .def("has_elapsed", &Timer::hasElapsed)          // Bind the hasElapsed method
        .def("__repr__", [](const Timer& t) {            // Optional: Add a Python-like string representation
        return "<Timer running=" + std::to_string(t.isRunning()) + ">";
            });

    // Create the 'Console' submodule under 'Py4GW'
    py::module_ console = m.def_submodule("Console", "Submodule for console logging");

	py::module_ game = m.def_submodule("Game", "Submodule for game functions");

    // Bind the MessageType enum inside the 'Console' submodule
    py::enum_<MessageType>(console, "MessageType")
        .value("Info", MessageType::Info)
        .value("Warning", MessageType::Warning)
        .value("Error", MessageType::Error)
        .value("Debug", MessageType::Debug)
        .value("Success", MessageType::Notice)
        .value("Performance", MessageType::Performance)
        .value("Notice", MessageType::Notice)
        .export_values(); // Expose enum values as Py4GW.Console.MessageType.Info, etc.

    // Bind the Log function to the 'Console' submodule
    console.def(
        "Log", &Log, "Log a message to the console", py::arg("module_name"), py::arg("message"),
        py::arg("type") = MessageType::Info // Handle the default argument here
    );

    console.def("GetCredits", &GetCredits, "Get the credits for the Py4GW library");
    console.def("GetLicense", &GetLicense, "Get the license for the Py4GW library");

    //get_gw_window_handle
    console.def("get_gw_window_handle", []() -> uintptr_t {
        return reinterpret_cast<uintptr_t>(Py4GW::get_gw_window_handle());
        }, "Get the Guild Wars window handle as an integer");
    
	console.def("get_projects_path", []() -> std::string {
		return dllDirectory;
		}, "Get the path where Py4GW.dll is located");


	console.def("resize_window", &ResizeWindow, "Resize the Guild Wars window", py::arg("width"), py::arg("height"));
	console.def("move_window_to", &MoveWindowTo, "Move the Guild Wars window to (x, y)", py::arg("x"), py::arg("y"));
	console.def("set_window_geometry", &SetWindowGeometry, "Set the Guild Wars window geometry (x, y, width, height). Use -1 to keep current value.", py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"));
	console.def("get_window_rect", &GetWindowRectFn, "Get the Guild Wars window rectangle (left, top, right, bottom)");
	console.def("get_client_rect", &GetClientRectFn, "Get the Guild Wars client rectangle (left, top, right, bottom)");
    console.def("set_window_title", [](py::str s) {
        std::string w = py::cast<std::string>(s); // Python str -> wstring
        SetWindowTitle(w);
        }, py::arg("title"));
 
	console.def("is_window_active", &IsWindowActive, "Check if the Guild Wars window is active (focused)");
	console.def("is_window_minimized", &IsWindowMinimized, "Check if the Guild Wars window is minimized");
	console.def("is_window_in_background", &IsWindowInBackground, "Check if the Guild Wars window is in the background (not focused)");
	console.def("set_borderless", &SetBorderless, "Enable or disable borderless window mode for Guild Wars", py::arg("enable"));

    // === Script Control Bindings ===
    console.def("load", &Py4GW_LoadScript, "Load a Python script from path", py::arg("path"));
    console.def("run", &Py4GW_RunScript, "Run the currently loaded script");
    console.def("stop", &Py4GW_StopScript, "Stop the currently running script");
    console.def("pause", &Py4GW_PauseScript, "Pause the running script");
    console.def("resume", &Py4GW_ResumeScript, "Resume the paused script");
    console.def("status", &Py4GW_GetScriptStatus, "Get current script status (Running, Paused, Stopped)");
    console.def("defer_load_and_run", &Py4GW_DeferLoadAndRun,
        "Stop current if needed, then load and run new script after delay (ms)",
        py::arg("path"), py::arg("delay_ms") = 1000);

    console.def("defer_stop_load_and_run", &Py4GW_DeferStopLoadAndRun,
        "Force stop, then load and run new script after delay (ms)",
        py::arg("path"), py::arg("delay_ms") = 1000);

    console.def("defer_stop_and_run", &Py4GW_DeferStopAndRun,
        "Stop current script, then rerun it after delay (ms)",
        py::arg("delay_ms") = 1000);




    
    py::class_<PingTracker>(m, "PingHandler")
        .def(py::init<>())                         // Constructor
        .def("Terminate", &PingTracker::Terminate) // Manual cleanup
        .def("GetCurrentPing", &PingTracker::GetCurrentPing)
        .def("GetAveragePing", &PingTracker::GetAveragePing)
        .def("GetMinPing", &PingTracker::GetMinPing)
        .def("GetMaxPing", &PingTracker::GetMaxPing);
}



// pybind11 module binding
PYBIND11_EMBEDDED_MODULE(PyKeystroke, m)
{
    // Binding for the scan code-based key handler
    py::class_<KeyHandler>(m, "PyScanCodeKeystroke")
        .def(pybind11::init<>()) // Constructor

        // Single key functions
        .def("PressKey", &KeyHandler::press_key, "Press a single key using scan code", pybind11::arg("virtualKeyCode"))
        .def("ReleaseKey", &KeyHandler::release_key, "Release a single key using scan code", pybind11::arg("virtualKeyCode"))
        .def("PushKey", &KeyHandler::push_key, "Press and release a single key using scan code", pybind11::arg("virtualKeyCode"))

        // Key combination functions
        .def("PressKeyCombo", &KeyHandler::press_key_combo, "Press a combination of keys using scan codes", pybind11::arg("keys"))
        .def("ReleaseKeyCombo", &KeyHandler::release_key_combo, "Release a combination of keys using scan codes", pybind11::arg("keys"))
        .def("PushKeyCombo", &KeyHandler::push_key_combo, "Press and release a combination of keys using scan codes", pybind11::arg("keys"));
}

// pybind11 mouse module binding
PYBIND11_EMBEDDED_MODULE(PyMouse, m)
{
	// Binding for the mouse handler
	py::class_<MouseHandler>(m, "PyMouse")
		.def(pybind11::init<>()) // Constructor
		// Mouse movement
		.def("MoveMouse", &MouseHandler::MoveMouse, "Move mouse to (x, y) relative to the client window", pybind11::arg("x"), pybind11::arg("y"))
		// Mouse click functions
		.def("Click", &MouseHandler::Click, "Click the mouse button at (x, y)", pybind11::arg("button") = 0, pybind11::arg("x") = 0, pybind11::arg("y") = 0)
		.def("DoubleClick", &MouseHandler::DoubleClick, "Double click the mouse button at (x, y)", pybind11::arg("button") = 0, pybind11::arg("x") = 0, pybind11::arg("y") = 0)
		// Mouse scroll
		.def("Scroll", &MouseHandler::Scroll, "Scroll the mouse wheel", pybind11::arg("delta"), pybind11::arg("x") = 0, pybind11::arg("y") = 0)
		// Mouse button press/release
		.def("PressButton", &MouseHandler::PressButton, "Press a mouse button at (x, y)", pybind11::arg("button") = 0, pybind11::arg("x") = 0, pybind11::arg("y") = 0)
		.def("ReleaseButton", &MouseHandler::ReleaseButton, "Release a mouse button at (x, y)", pybind11::arg("button") = 0, pybind11::arg("x") = 0, pybind11::arg("y") = 0);
}

