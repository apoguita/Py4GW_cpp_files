#include "Py4GW.h"

#include "Headers.h"
#include <iostream>

HeroAI* Py4GW::heroAI = nullptr;

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

void DrawConsole(const char* title, bool* new_p_open = nullptr)
{
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

        // Toggle Timestamp Checkbox
        ImGui::TableSetColumnIndex(3);
        ImGui::Checkbox("Timestamps", &show_timestamps);
        ShowTooltipInternal("Show or hide timestamps in the console output");

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

std::string GetCredits()
{ 
    return "Py4GW v1.0.57, Apoguita - 2024,2025";
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
    if (HeroAI_Initialized) {
        if (heroAI->IsAIEnabled() && AllowedToRender()) {
            heroAI->Update();
        }
    }
}


/* 
class KeyHandler {
public:
    // Press a single key using scan codes
    void press_key(int virtualKeyCode)
    {
        HWND targetWindow = gw_client_window_handle; // Get the target window handle
        if (!targetWindow) return;                 // Fail silently if no window is set

        // Attach input thread to target thread
        DWORD targetThreadId = GetWindowThreadProcessId(targetWindow, nullptr);
        DWORD currentThreadId = GetCurrentThreadId();
        AttachThreadInput(currentThreadId, targetThreadId, TRUE);

        // Focus the target window
        SetForegroundWindow(targetWindow);
        SetActiveWindow(targetWindow);

        // Send key press
        INPUT input = { 0 };
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = 0;                                                // Use scan code mode
        input.ki.wScan = MapVirtualKey(virtualKeyCode, MAPVK_VK_TO_VSC); // Map virtual key to scan code
        input.ki.dwFlags = 0;                                            // Key press
        SendInput(1, &input, sizeof(INPUT));

        // Detach thread input
        AttachThreadInput(currentThreadId, targetThreadId, FALSE);
    }

    // Release a single key using scan codes
    void release_key(int virtualKeyCode)
    {
        HWND targetWindow = gw_client_window_handle; // Get the target window handle
        if (!targetWindow) return;

        // Attach input thread to target thread
        DWORD targetThreadId = GetWindowThreadProcessId(targetWindow, nullptr);
        DWORD currentThreadId = GetCurrentThreadId();
        AttachThreadInput(currentThreadId, targetThreadId, TRUE);

        // Focus the target window
        SetForegroundWindow(targetWindow);
        SetActiveWindow(targetWindow);

        // Send key release
        INPUT input = { 0 };
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = 0;                                                // Use scan code mode
        input.ki.wScan = MapVirtualKey(virtualKeyCode, MAPVK_VK_TO_VSC); // Map virtual key to scan code
        input.ki.dwFlags = KEYEVENTF_KEYUP;                              // Key release
        SendInput(1, &input, sizeof(INPUT));

        // Detach thread input
        AttachThreadInput(currentThreadId, targetThreadId, FALSE);
    }

    // Press and release a key (push key)
    void push_key(int virtualKeyCode)
    {
        press_key(virtualKeyCode);
        Sleep(50); // Mimic a real press delay
        release_key(virtualKeyCode);
    }

    // Press a combination of keys
    void press_key_combo(const std::vector<int>& keys)
    {
        for (int key : keys) {
            press_key(key);
        }
    }

    // Release a combination of keys
    void release_key_combo(const std::vector<int>& keys)
    {
        for (int key : keys) {
            release_key(key);
        }
    }

    // Push a combination of keys
    void push_key_combo(const std::vector<int>& keys)
    {
        press_key_combo(keys);
        Sleep(100); // Mimic a real delay
        release_key_combo(keys);
    }
};
*/

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

    void send_key(int virtualKeyCode, bool isKeyUp = false) {
        if (!targetWindow) return;

        // Create the LPARAM with scan code and other flags
        LPARAM lParam = 1; // Repeat count set to 1
        lParam |= MapVirtualKey(virtualKeyCode, MAPVK_VK_TO_VSC) << 16; // Add scan code
        if (isKeyUp) lParam |= 0xC0000000; // Key-up flag and previous key state

        // Send the message directly to the target window
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




//KeyHandler keys = KeyHandler();

bool debug = false;

bool show_modal = false;
bool modal_result_ok = false;
bool first_run = true;
bool console_open = true;

bool check_login_screen = true;

void Py4GW::Draw(IDirect3DDevice9*) {

    DrawConsole("Py4GW Console", &console_open);
    
    if (first_run) {
        first_run = false;
        console_open = true;
		modal_result_ok = false;
		dll_shutdown = false;
		show_modal = false;

        Initialize();
        Log("Py4GW", GetCredits(), MessageType::Success);
        Log("Py4GW", "Python interpreter initialized.", MessageType::Success);

        ChangeWorkingDirectory(dllDirectory);

        script_path2 = dllDirectory + "/Py4GW_widget_manager.py";

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
        
        IniHandler ini_handler;

        if (ini_handler.Load("Py4GW.ini")) {
            std::string autoexec_file_path;
            autoexec_file_path = ini_handler.Get("settings", "autoexec_script", "");

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

}

void SetFog(bool enable) {
    GW::GameThread::Enqueue([enable]() {
        GW::CameraMgr::SetFog(enable);
        });
}






HWND Py4GW::get_gw_window_handle() { return gw_window_handle; }

bool Py4GW::HeroAI_IsAIEnabled() { return heroAI->IsAIEnabled(); }
void Py4GW::HeroAI_SetAIEnabled(bool state) { heroAI->SetAIEnabled(state); }
int  Py4GW::HeroAI_GetMyPartyPos() { return heroAI->GetMyPartyPos(); }
// ********** Python Vars **********

bool Py4GW::HeroAI_IsActive(int party_pos) { return heroAI->IsActive(party_pos); }
void Py4GW::HeroAI_SetLooting(int party_pos, bool state) { heroAI->SetLooting(party_pos, state); }
bool Py4GW::HeroAI_GetLooting(int party_pos) { return heroAI->GetLooting(party_pos); }
void Py4GW::HeroAI_SetFollowing(int party_pos, bool state) { heroAI->SetFollowing(party_pos, state); }
bool Py4GW::HeroAI_GetFollowing(int party_pos) { return heroAI->GetFollowing(party_pos); }
void Py4GW::HeroAI_SetCombat(int party_pos, bool state) { heroAI->SetCombat(party_pos, state); }
bool Py4GW::HeroAI_GetCombat(int party_pos) { return heroAI->GetCombat(party_pos); }
void Py4GW::HeroAI_SetCollision(int party_pos, bool state) { heroAI->SetCollision(party_pos, state); }
bool Py4GW::HeroAI_GetCollision(int party_pos) { return heroAI->GetCollision(party_pos); }
void Py4GW::HeroAI_SetTargetting(int party_pos, bool state) { heroAI->SetTargetting(party_pos, state); }
bool Py4GW::HeroAI_GetTargetting(int party_pos) { return heroAI->GetTargetting(party_pos); }
void Py4GW::HeroAI_SetSkill(int party_pos, int skill_pos, bool state) { heroAI->SetSkill(party_pos, skill_pos, state); }
bool Py4GW::HeroAI_GetSkill(int party_pos, int skill_pos) { return heroAI->GetSkill(party_pos, skill_pos); }

void Py4GW::HeroAI_FlagAIHero(int party_pos, float x, float y) { heroAI->FlagAIHero(party_pos, x, y); }
void  Py4GW::HeroAI_UnFlagAIHero(int party_pos) { heroAI->UnFlagAIHero(party_pos); }
float Py4GW::HeroAI_GetEnergy(int party_pos) { return heroAI->GetEnergy(party_pos); }
float Py4GW::HeroAI_GetEnergyRegen(int party_pos) { return heroAI->GetEnergyRegen(party_pos); }
int  Py4GW::HeroAI_GetPythonAgentID(int party_pos) { return heroAI->GetPythonAgentID(party_pos); }
void Py4GW::HeroAI_Resign(int party_pos) { heroAI->Resign(party_pos); }
void Py4GW::HeroAI_TakeQuest(uint32_t party_pos, uint32_t quest_id) { heroAI->TakeQuest(party_pos, quest_id); }
void Py4GW::HeroAI_Identify_cmd(int party_pos) { heroAI->Identify_cmd(party_pos); }
void Py4GW::HeroAI_Salvage_cmd(int party_pos) { heroAI->Salvage_cmd(party_pos); }


PYBIND11_EMBEDDED_MODULE(Py4GW, m)
{
    m.doc() = "Py4GW, Python Enabler Library for GuildWars"; // Optional module docstring

	py::module_ HeroAI_module = m.def_submodule("HeroAI", "Submodule for Hero AI");

    HeroAI_module.def("GetAIStatus", &Py4GW::HeroAI_IsAIEnabled, "Check if the AI is enabled");
    HeroAI_module.def("SetAIStatus", &Py4GW::HeroAI_SetAIEnabled, "Enable or disable the AI");
    HeroAI_module.def("GetMyPartyPos", &Py4GW::HeroAI_GetMyPartyPos, "Get the party position of the player");
    
    HeroAI_module.def("IsActive", &Py4GW::HeroAI_IsActive, "Check if the hero is active");
    HeroAI_module.def("SetLooting", &Py4GW::HeroAI_SetLooting, "Enable or disable looting");
    HeroAI_module.def("GetLooting", &Py4GW::HeroAI_GetLooting, "Check if looting is enabled");
    HeroAI_module.def("SetFollowing", &Py4GW::HeroAI_SetFollowing, "Enable or disable following");
    HeroAI_module.def("GetFollowing", &Py4GW::HeroAI_GetFollowing, "Check if following is enabled");
    HeroAI_module.def("SetCombat", &Py4GW::HeroAI_SetCombat, "Enable or disable combat");
    HeroAI_module.def("GetCombat", &Py4GW::HeroAI_GetCombat, "Check if combat is enabled");
    HeroAI_module.def("SetCollision", &Py4GW::HeroAI_SetCollision, "Enable or disable collision");
    HeroAI_module.def("GetCollision", &Py4GW::HeroAI_GetCollision, "Check if collision is enabled");
    HeroAI_module.def("SetTargetting", &Py4GW::HeroAI_SetTargetting, "Enable or disable targetting");
    HeroAI_module.def("GetTargetting", &Py4GW::HeroAI_GetTargetting, "Check if targetting is enabled");
    HeroAI_module.def("SetSkill", &Py4GW::HeroAI_SetSkill, "Enable or disable a skill");
    HeroAI_module.def("GetSkill", &Py4GW::HeroAI_GetSkill, "Check if a skill is enabled");

    HeroAI_module.def("FlagAIHero", &Py4GW::HeroAI_FlagAIHero, "Flag a hero to move to a location");
    HeroAI_module.def("UnFlagAIHero", &Py4GW::HeroAI_UnFlagAIHero, "Unflag a hero");
    HeroAI_module.def("GetEnergy", &Py4GW::HeroAI_GetEnergy, "Get the energy of a hero");
    HeroAI_module.def("GetEnergyRegen", &Py4GW::HeroAI_GetEnergyRegen, "Get the energy regen of a hero");
    HeroAI_module.def("GetPythonAgentID", &Py4GW::HeroAI_GetPythonAgentID, "Get the Python agent ID of a hero");
    HeroAI_module.def("Resign", &Py4GW::HeroAI_Resign, "Resign a hero");
    HeroAI_module.def("TakeQuest", &Py4GW::HeroAI_TakeQuest, "Take a quest");
    HeroAI_module.def("Identify_cmd", &Py4GW::HeroAI_Identify_cmd, "Identify all items in inventory");
    HeroAI_module.def("Salvage_cmd", &Py4GW::HeroAI_Salvage_cmd, "Salvage all items in inventory");
    
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
	game.def("SetFog", &SetFog, "Enable or disable fog");


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


class MouseHandler {
public:
    // Move the mouse to a specific position
    void move_mouse(int x, int y) {
        HWND hwnd = GetForegroundWindow(); // Get the current active window
        if (hwnd == nullptr) return;

        RECT windowRect;
        if (!GetWindowRect(hwnd, &windowRect)) return;

        int windowX = windowRect.left;
        int windowY = windowRect.top;

        // Adjust the position relative to the window's top-left corner
        int absoluteX = windowX + x;
        int absoluteY = windowY + y;

        // Normalize coordinates for the absolute positioning
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dx = absoluteX * (65535 / GetSystemMetrics(SM_CXSCREEN));
        input.mi.dy = absoluteY * (65535 / GetSystemMetrics(SM_CYSCREEN));
        input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;

        SendInput(1, &input, sizeof(INPUT));
    }

    // Press a mouse button
    void press_button(DWORD mouseEvent) {
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = mouseEvent;

        SendInput(1, &input, sizeof(INPUT));
    }

    // Release a mouse button
    void release_button(DWORD mouseEvent) {
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = mouseEvent;

        SendInput(1, &input, sizeof(INPUT));
    }

    // Click (press and release) a mouse button
    void click_button(DWORD mouseEvent) {
        press_button(mouseEvent);
        Sleep(50); // Mimic a human-like click
        release_button(mouseEvent);
    }

    // Scroll the mouse wheel
    void scroll_wheel(int delta) {
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_WHEEL;
        input.mi.mouseData = delta;

        SendInput(1, &input, sizeof(INPUT));
    }
};
