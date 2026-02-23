#include "Py4GW.h"

#include "Headers.h"
#include <iostream>

//HeroAI* Py4GW::heroAI = nullptr;

namespace py = pybind11;

/* ------------------------------------------------------------------*/
/* ------------------- TYPES AND GLOBALS ----------------------------*/
/* ------------------------------------------------------------------*/

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


bool p_open = true;
bool scroll_to_bottom = false;
ImGuiTextBuffer buffer;
ImVector<int> line_offsets;

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

bool show_console = false;

ImVec2 console_pos = ImVec2(5, 30);
ImVec2 console_size = ImVec2(800, 700);
bool console_collapsed = false;
bool show_compact_console_ini = false;

ImVec2 compact_console_pos = ImVec2(5, 30);
bool compact_console_collapsed = false;

GlobalMouseClass GlobalMouse;

using FrameCallbackId = uint64_t;

struct FrameCallback {
    FrameCallbackId id;
    std::string name;
    py::object fn;
};

static std::vector<FrameCallback> g_frame_callbacks;
static std::mutex g_frame_callbacks_mutex;
static FrameCallbackId g_next_callback_id = 1;

bool debug = false;

bool show_modal = false;
bool modal_result_ok = false;
bool first_run = true;
bool console_open = true;

bool enabled_update_to_run = false;

bool check_login_screen = true;

py::object update_function;
py::object draw_function;

py::object update_function2;
py::object draw_function2;

// 1. A dedicated storage for a single Metric's history
struct MetricData {
    static const int MAX_SAMPLES = 600;
    double samples[MAX_SAMPLES] = { 0 };
    int head = 0;
    bool full = false;

    // Throttle State
    uint64_t last_frame_id = 0;
    double accumulator = 0;
    int frames_in_window = 0;

    void push_frame_throttled(uint64_t current_frame, double ms) {
        accumulator += ms;
        frames_in_window++;

        // Trigger every 6th frame
        if (frames_in_window >= 6) {
            double avg_load = accumulator / frames_in_window;

            samples[head] = avg_load;
            head = (head + 1) % MAX_SAMPLES;
            if (head == 0) full = true;

            // Reset
            accumulator = 0;
            frames_in_window = 0;
            last_frame_id = current_frame;
        }
    }

    // Stats functions (unchanged from previous)
    size_t count() const { return full ? MAX_SAMPLES : head; }
};

using MetricTuple = std::tuple<double, double, double, double, double, double>;

class profiler {
private:
    struct StartPoint {
        LARGE_INTEGER start_time;
    };
    // Maps Metric Name -> Active Start call
    inline static std::map<std::string, StartPoint> active_starts;

public:
    // Maps Metric Name -> The History Buffer
    inline static std::map<std::string, MetricData> history;
    inline static LARGE_INTEGER frequency = { 0 };
    inline static bool freq_init = false;

    static void start(const char* name) {
        if (!freq_init) {
            QueryPerformanceFrequency(&frequency);
            freq_init = true;
        }
        LARGE_INTEGER t;
        QueryPerformanceCounter(&t);
        active_starts[name] = {  t };
    }

    static void end(uint64_t frame_id, const char* name) {
        LARGE_INTEGER t_end;
        QueryPerformanceCounter(&t_end);

        auto it = active_starts.find(name);
        if (it != active_starts.end()) {
            double duration = (double)(t_end.QuadPart - it->second.start_time.QuadPart) * 1000.0 / frequency.QuadPart;

            // Log to the metric's specific 60-second buffer
            history[name].push_frame_throttled(frame_id, duration);

            active_starts.erase(it);
        }
    }

    static MetricTuple CalculateReport(const std::string& metric_name) {
        auto it = history.find(metric_name);
        if (it == history.end() || it->second.count() == 0) {
            return { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
        }

        auto& data = it->second;
        size_t n = data.count();

        // 1. Calculate Mean (Average) and Min
        double sum = 0;
        double min_val = data.samples[0];
        for (size_t i = 0; i < n; ++i) {
            sum += data.samples[i];
            if (data.samples[i] < min_val) min_val = data.samples[i];
        }
        double avg = sum / n;

        // 2. Calculate Percentiles (Sort required)
        std::vector<double> sorted(data.samples, data.samples + n);
        std::sort(sorted.begin(), sorted.end());

        double p50 = sorted[static_cast<size_t>(n * 0.50)];
        double p95 = sorted[static_cast<size_t>(n * 0.95)];
        double p99 = sorted[static_cast<size_t>(n * 0.99)];
        double max_val = sorted.back();

        return { min_val, avg, p50, p95, p99, max_val };
    }

    // Returns a vector of tuples containing: { "MetricName", Min, Avg, P50, P95, P99, Max }
    static std::vector<std::tuple<std::string, double, double, double, double, double, double>> CalculateReportAll() {
        std::vector<std::tuple<std::string, double, double, double, double, double, double>> reports;

        for (const auto& pair : history) {
            MetricTuple stats = CalculateReport(pair.first);

            // Unpack and pack with the name for the Python side
            reports.emplace_back(
                pair.first,
                std::get<0>(stats), // Min
                std::get<1>(stats), // Avg
                std::get<2>(stats), // P50
                std::get<3>(stats), // P95
                std::get<4>(stats), // P99
                std::get<5>(stats)  // Max
            );
        }
        return reports;

    }
    static std::vector<std::string> GetMetricNames() {
        std::vector<std::string> names;
        for (const auto& pair : history) {
            names.push_back(pair.first);
        }
        return names;
    }

    static std::vector<double> GetMetricHistory(const std::string& name) {
        auto it = history.find(name);
        if (it == history.end()) return {};

        const auto& data = it->second;
        size_t n = data.count();
        std::vector<double> out;
        out.reserve(n);

        if (!data.full) {
            // Buffer isn't full: samples are just 0 to head-1
            for (int i = 0; i < data.head; ++i) {
                out.push_back(data.samples[i]);
            }
        }
        else {
            // Buffer is revolving: 
            // Part 1: From head to end (Oldest data)
            for (int i = data.head; i < data.MAX_SAMPLES; ++i) {
                out.push_back(data.samples[i]);
            }
            // Part 2: From 0 to head-1 (Newer data)
            for (int i = 0; i < data.head; ++i) {
                out.push_back(data.samples[i]);
            }
        }
        return out;
    }

    static void Reset() {
        active_starts.clear();
        history.clear();
    }

};

static profiler py4gw_profiler;
static uint64_t frame_id_timestamp;

static std::vector<std::string> GetProfilerMetricNames() {
	return profiler::GetMetricNames();
}

static std::vector<std::tuple<std::string, double, double, double, double, double, double>> GetProfilerReport() {
	return profiler::CalculateReportAll();
}

static std::vector<double> GetProfilerHistory(const std::string& metric_name) {
	return profiler::GetMetricHistory(metric_name);
}

static void ResetProfiler() {
	profiler::Reset();
}



/* ------------------------------------------------------------------*/
/* ------------------------ CALLBACKS -------------------------------*/
/* ------------------------------------------------------------------*/

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

/* ------------------------------------------------------------------*/
/* -------------------------- Helpers -------------------------------*/
/* ------------------------------------------------------------------*/

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


std::string GetCredits()
{
    return "Py4GW v3.0.0, Apoguita - 2024,2026";
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

static uint64_t Get_Tick_Count64() {
    return GetTickCount64();
}

HWND Py4GW::get_gw_window_handle() {
    return GW::MemoryMgr::GetGWWindowHandle();
    //return gw_window_handle; 
}



/* ------------------------------------------------------------------*/
/* -------------------------- Python --------------------------------*/
/* ------------------------------------------------------------------*/

static bool CallPythonFunctionSafe(py::object& fn, const char* label, ScriptState& state_to_stop)
{
    if (!fn || fn.is_none())
        return false;

    try {
        fn();
        return true;
    }
    catch (const py::error_already_set& e) {
        state_to_stop = ScriptState::Stopped;
        Log("Py4GW", std::string("Python error (") + label + "): " + e.what(), MessageType::Error);
        return false;
    }
}

static py::object GetCallableIfExists(py::object& module, const char* name)
{
    if (!module || module.is_none())
        return py::object();

    if (!py::hasattr(module, name))
        return py::object();

    py::object obj = module.attr(name);
    if (!obj || obj.is_none())
        return py::object();

    // pybind11 does not have py::callable(); use CPython API
    if (!PyCallable_Check(obj.ptr()))
        return py::object();

    return obj;
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

        // Capture entrypoints
        main_function = GetCallableIfExists(script_module, "main");
        update_function = GetCallableIfExists(script_module, "update");
        draw_function = GetCallableIfExists(script_module, "draw");

        if (draw_function && !draw_function.is_none()) {
            Log("Py4GW", "draw() function found.", MessageType::Notice);
        }
        if (update_function && !update_function.is_none()) {
            Log("Py4GW", "update() function found.", MessageType::Notice);
        }
        if (main_function && !main_function.is_none()) {
            Log("Py4GW", "main() function found.", MessageType::Notice);
        }

        // Valid script if it has at least one entrypoint
        if ((main_function && !main_function.is_none()) ||
            (update_function && !update_function.is_none()) ||
            (draw_function && !draw_function.is_none()))
        {
            return true;
        }

        script_state = ScriptState::Stopped;
        Log("Py4GW", "No main()/update()/draw() function found in the script.", MessageType::Error);
        return false;

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

        main_function2 = GetCallableIfExists(script_module2, "main");
        update_function2 = GetCallableIfExists(script_module2, "update");
        draw_function2 = GetCallableIfExists(script_module2, "draw");

        // Valid if at least one exists
        if ((main_function2 && !main_function2.is_none()) ||
            (update_function2 && !update_function2.is_none()) ||
            (draw_function2 && !draw_function2.is_none()))
        {
            return true;
        }

        script_state2 = ScriptState::Stopped;
        Log("Py4GW", "No main()/update()/draw() function found in the Main Menu script.", MessageType::Error);
        return false;

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

void ExecutePythonScript_Update()
{
    // prefer update()
    if (update_function && !update_function.is_none()) {
		py4gw_profiler.start("Update.Console.Update");
        CallPythonFunctionSafe(update_function, "update()", script_state);
		py4gw_profiler.end(frame_id_timestamp, "Update.Console.Update");
        //return;
    }

    // If no update(), do nothing.
    // IMPORTANT: We do NOT run main() here, because main may contain ImGui calls.
}

void ExecutePythonScript_Draw()
{
    // prefer draw()
    if (draw_function && !draw_function.is_none()) {
		py4gw_profiler.start("Draw.Console.Draw");
        CallPythonFunctionSafe(draw_function, "draw()", script_state);
		py4gw_profiler.end(frame_id_timestamp, "Draw.Console.Draw");
        //return;
    }

    // fallback to legacy main()
    if (main_function && !main_function.is_none()) {
		py4gw_profiler.start("Draw.Console.Main");
        CallPythonFunctionSafe(main_function, "main()", script_state);
		py4gw_profiler.end(frame_id_timestamp, "Draw.Console.Main");
    }
}

void ExecutePythonScript2_Update()
{
    if (update_function2 && !update_function2.is_none()) {
		py4gw_profiler.start("Update.WidgetManager.Update");
        CallPythonFunctionSafe(update_function2, "update2()", script_state2);
		py4gw_profiler.end(frame_id_timestamp, "Update.WidgetManager.Update");
    }
}

void ExecutePythonScript2_Draw()
{
    if (draw_function2 && !draw_function2.is_none()) {
        py4gw_profiler.start("Draw.WidgetManager.Draw");
        CallPythonFunctionSafe(draw_function2, "draw2()", script_state2);
		py4gw_profiler.end(frame_id_timestamp, "Draw.WidgetManager.Draw");
        //return;
    }

    if (main_function2 && !main_function2.is_none()) {
        py4gw_profiler.start("Draw.WidgetManager.Main");
        CallPythonFunctionSafe(main_function2, "main2()", script_state2);
		py4gw_profiler.end(frame_id_timestamp, "Draw.WidgetManager.Main");
    }
}


void ResetScriptEnvironment()
{
    script_content.clear();
    script_state = ScriptState::Stopped;

    main_function = py::object();
    update_function = py::object();
    draw_function = py::object();

    script_module = py::object();
    Log("Py4GW", "Python environment reset.", MessageType::Notice);
}

void ResetScriptEnvironment2()
{
    script_content2.clear();
    script_state2 = ScriptState::Stopped;

    main_function2 = py::object();
    update_function2 = py::object();
    draw_function2 = py::object();

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

void EnqueuePythonCallback(py::function func) {
    // move func into the lambda so it stays alive
    GW::GameThread::Enqueue([func = std::move(func)]() mutable {
        // We're now running on the GW game thread here

        auto instance_type = GW::Map::GetInstanceType();
        bool is_map_ready = (GW::Map::GetIsMapLoaded()) && (instance_type != GW::Constants::InstanceType::Loading);

        if (!is_map_ready) {

            return;
        }

        py::gil_scoped_acquire gil;
        try {
            func();  // Call the Python function with no args
        }
        catch (const py::error_already_set& e) {
            // TODO: log this somewhere sane
            // Logger::Instance().LogError(e.what(), "PyGameThread");
        }
        });
}


/* ------------------------------------------------------------------*/
/* ----------------- Python Frame Callbacks -------------------------*/
/* ------------------------------------------------------------------*/

using CallbackId = uint64_t;

class PyCallback {
public:
    enum class Phase : uint8_t {
        PreUpdate = 0,
        Data = 1,
        Update = 2
    };

    enum class Context : uint8_t {
        Update = 0,
        Draw = 1,
        Main = 2
    };

    struct Task {
        CallbackId id;
        std::string name;
        Phase phase;
        Context context;
        int priority;
        uint64_t order;   // registration order
        py::function fn;
		bool paused = false;
    };

private:
    static inline std::mutex _mutex;
    static inline std::vector<Task> _tasks;
    static inline CallbackId _next_id = 1;
    static inline uint64_t _next_order = 1;

public:
    // -------------------------------------------------
    // Register
    // -------------------------------------------------
    static CallbackId Register(
        const std::string& name,
        Phase phase,
        py::function fn,
        int priority = 99,
        Context context = Context::Draw
    ) {
        std::lock_guard<std::mutex> lock(_mutex);

        // replace by name (keep id + order)
        for (auto& t : _tasks) {
            if (t.name == name && t.phase == phase && t.context == context) {
                t.fn = std::move(fn);
                t.priority = priority;
                return t.id;
            }
        }

        CallbackId id = _next_id++;

        _tasks.push_back(Task{
            id,
            name,
            phase,
			context,
            priority,
            _next_order++,
            std::move(fn)
            });

        return id;
    }

    // -------------------------------------------------
    // Remove
    // -------------------------------------------------
    static bool RemoveById(CallbackId id) {
        std::lock_guard<std::mutex> lock(_mutex);

        auto it = std::remove_if(
            _tasks.begin(),
            _tasks.end(),
            [&](const Task& t) { return t.id == id; }
        );

        if (it == _tasks.end())
            return false;

        _tasks.erase(it, _tasks.end());
        return true;
    }

    static bool RemoveByName(const std::string& name) {
        std::lock_guard<std::mutex> lock(_mutex);

        auto it = std::remove_if(
            _tasks.begin(),
            _tasks.end(),
            [&](const Task& t) { return t.name == name; }
        );

        if (it == _tasks.end())
            return false;

        _tasks.erase(it, _tasks.end());
        return true;
    }

    static void RemoveAll() {
        std::lock_guard<std::mutex> lock(_mutex);
        _tasks.clear();
    }

	static bool PauseById(CallbackId id) {
		std::lock_guard<std::mutex> lock(_mutex);
		for (auto& t : _tasks) {
			if (t.id == id) {
				t.paused = true;
				return true;
			}
		}
		return false;
	}

	static bool ResumeById(CallbackId id) {
		std::lock_guard<std::mutex> lock(_mutex);   
		for (auto& t : _tasks) {    
			if (t.id == id) {
				t.paused = false;
				return true;
			}
		}
		return false;
	}

	static bool IsPaused(CallbackId id) {
		std::lock_guard<std::mutex> lock(_mutex);
		for (const auto& t : _tasks) {
			if (t.id == id) {
				return t.paused;
			}
		}
		return false; // or throw an exception if not found
	}

	static bool IsRegistered(CallbackId id) {
		std::lock_guard<std::mutex> lock(_mutex);
		for (const auto& t : _tasks) {
			if (t.id == id) {
				return true;
			}
		}
		return false;
	}

    // -------------------------------------------------
    // Execute one phase (barrier enforced externally)
    // -------------------------------------------------
    static void ExecutePhase(Phase phase, Context context) {
        std::vector<Task*> phase_tasks;

        {
            std::lock_guard<std::mutex> lock(_mutex);
            for (auto& t : _tasks) {
				if (t.phase == phase && t.context == context && !t.paused)
                    phase_tasks.push_back(&t);
            }
        }

        std::sort(
            phase_tasks.begin(),
            phase_tasks.end(),
            [](const Task* a, const Task* b) {
                if (a->priority != b->priority)
                    return a->priority < b->priority;
                return a->order < b->order;
            }
        );

        
        for (Task* t : phase_tasks) {
            // Construct a clean name for the profiler
            const char* ctx_name = (context == Context::Draw) ? "Draw" : "Update";
			ctx_name = (context == Context::Main) ? "Main" : ctx_name;
            const char* phase_suffix;
            switch (phase) {
            case Phase::PreUpdate: phase_suffix = "PreUpdate"; break;
            case Phase::Data:      phase_suffix = "Data";      break;
            default:               phase_suffix = "Update";    break;
            }

            std::string full_prof_name = std::string(ctx_name) + ".Callback." + phase_suffix + "." + t->name;


            // ---- PROFILING START ----
            py4gw_profiler.start(full_prof_name.c_str());
            // ---- PROFILING END ----

            try {
                t->fn();
            }
            catch (const py::error_already_set&) {
                PyErr_Print();
            }

            // ---- PROFILING STOP ----
            py4gw_profiler.end(frame_id_timestamp, full_prof_name.c_str());
        }
    }

    static std::vector<
        std::tuple<
        uint64_t,     // id
        std::string,    // name
        int,            // phase
		int,            // context
        int,            // priority
        uint64_t,       // order
		bool			// paused
        >
    > GetCallbackInfo()
    {
        std::lock_guard<std::mutex> lock(_mutex);

        std::vector<
            std::tuple<
            uint64_t,
            std::string,
            int,
            int,
            int,
            uint64_t,
			bool
            >
        > out;

        out.reserve(_tasks.size());

        for (const auto& t : _tasks) {
            out.emplace_back(
                t.id,
                t.name,
                static_cast<int>(t.phase),
				static_cast<int>(t.context),
                t.priority,
                t.order,
				t.paused
            );
        }

        return out;
    }

	static void Clear() {
		std::lock_guard<std::mutex> lock(_mutex);
		_tasks.clear();
	}
};


/* ------------------------------------------------------------------*/
/* -------------------------- ImGui ---------------------------------*/
/* ------------------------------------------------------------------*/


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

/* ------------------------------------------------------------------*/
/* ---------------------- DLL Handling ------------------------------*/
/* ------------------------------------------------------------------*/

bool Py4GW::Initialize() {
    py::initialize_interpreter();
    InitializeMerchantCallbacks();

    DebugMessage(L"Py4GW, Initialized.");

    return true;

}

void Py4GW::Terminate() {
    GW::DisableHooks();
    GW::Terminate();
    if (Py_IsInitialized()) {
        py::finalize_interpreter();
    }
}

// Global/Static sync variables
static std::mutex g_update_mutex;
static std::condition_variable g_update_cv;
static bool g_update_ready = false;
static uint32_t g_draw_frame_count = 0;

void Py4GW::Update()
{
    if (!enabled_update_to_run) {
        return;
    }

    
    // 1. WAIT FOR THE DRAW THREAD (MUST be outside the GIL!)
    {
        std::unique_lock<std::mutex> lock(g_update_mutex);

        // The thread goes to sleep here (0% CPU) until Draw wakes it up.
        // Add a shutdown condition if you have a global "is_running" flag
        // so it doesn't hang on exit, e.g., `return g_update_ready || !is_running;`
        g_update_cv.wait(lock, [] { return g_update_ready; });

        // Consume the signal so it waits again next time
        g_update_ready = false;
    }
    

    // 2. NOW GRAB THE GIL AND EXECUTE
    py::gil_scoped_acquire gil;

    //Update
    PyCallback::ExecutePhase(PyCallback::Phase::PreUpdate, PyCallback::Context::Update);
    PyCallback::ExecutePhase(PyCallback::Phase::Data, PyCallback::Context::Update);
    PyCallback::ExecutePhase(PyCallback::Phase::Update, PyCallback::Context::Update);

    if (script_state == ScriptState::Running && !script_content.empty()) {
        py4gw_profiler.start("Update.Console");
        ExecutePythonScript_Update();
        py4gw_profiler.end(frame_id_timestamp, "Update.Console");
    }

    if (script_state2 == ScriptState::Running && !script_content2.empty()) {
        py4gw_profiler.start("Update.WidgetManager");
        ExecutePythonScript2_Update();
        py4gw_profiler.end(frame_id_timestamp, "Update.WidgetManager");
    }
}



void Py4GW::Draw(IDirect3DDevice9* device) {
	frame_id_timestamp = GetTickCount64();

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


        //Widget Manager

        if (LoadAndExecuteScriptOnce2()) {
            script_state2 = ScriptState::Running;
            script_timer2.reset(); // Reset and start the timer
        }
        else {
            ResetScriptEnvironment2();
            script_state2 = ScriptState::Stopped;
            script_timer2.stop(); // Stop the timer
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

    enabled_update_to_run = true;


    py::gil_scoped_acquire gil;
    
    //Update
    // If you still want these phases to be logic-only, keep them here:
    PyCallback::ExecutePhase(PyCallback::Phase::PreUpdate, PyCallback::Context::Draw);
    PyCallback::ExecutePhase(PyCallback::Phase::Data, PyCallback::Context::Draw);

    // --- SYNC START: Signal the Update thread here ---
    g_draw_frame_count++;
    if (g_draw_frame_count % 2 == 0) {
        {
            std::lock_guard<std::mutex> lock(g_update_mutex);
            g_update_ready = true;
        }
        g_update_cv.notify_one(); // Wake up Py4GW::Update()
    }
    // --- SYNC END ---

    PyCallback::ExecutePhase(PyCallback::Phase::Update, PyCallback::Context::Draw);

	//Main thread callbacks (legacy, not phased)
    PyCallback::ExecutePhase(PyCallback::Phase::Update, PyCallback::Context::Main);

    // -------------------------------------------------
    // Script draw execution
    // -------------------------------------------------

        if (script_state == ScriptState::Running && !script_content.empty()) {
            ExecutePythonScript_Draw();
        }

        if (script_state2 == ScriptState::Running && !script_content2.empty()) {
            ExecutePythonScript2_Draw();
        }



     // -------------------------------------------------
    // Deferred actions
    // -------------------------------------------------

        if (mixed_deferred.active &&
            mixed_deferred.timer.hasElapsed(mixed_deferred.delay_ms))
        {
            py4gw_profiler.start("Draw.Deferred.Draw");

            if (mixed_deferred.action) {
                mixed_deferred.action();
            }

            py4gw_profiler.end(frame_id_timestamp, "Draw.Deferred.Draw");

            mixed_deferred.active = false;
        }
}




/* ------------------------------------------------------------------*/
/* --------------------- Misc Functions  ----------------------------*/
/* ------------------------------------------------------------------*/

bool InCharacterSelectScreen()
{
    const GW::PreGameContext* pgc = GW::GetPreGameContext();
    if (!pgc || !pgc->chars.valid()) {
        return false;
    }
    uint32_t ui_state = 10;
    SendUIMessage(GW::UI::UIMessage::kCheckUIState, nullptr, &ui_state);
    return ui_state == 2;
}

/* ------------------------------------------------------------------*/
/* --------------------- Python Bindings ----------------------------*/
/* ------------------------------------------------------------------*/

void bind_Game(py::module_& game)
{
    game.def("InCharacterSelectScreen", &InCharacterSelectScreen, "Check if the character select screen is ready");
    
    game.def("enqueue",&EnqueuePythonCallback,"Enqueue a Python callback to run on the GW game thread");

    game.def("get_tick_count64",&Get_Tick_Count64,"Get the current tick count as a 64-bit integer");
    //game.def("register_callback",&RegisterFrameCallback,"Register a named per-frame callback (idempotent by name)" );
    //game.def("remove_callback_by_id",&RemoveFrameCallbackById,"Remove a per-frame callback by id");
    //game.def("remove_callback",&RemoveFrameCallbackByName,"Remove a per-frame callback by name");
    //game.def("clear_callbacks", &RemoveAllFrameCallbacks, "Remove all registered per-frame callbacks");
    ;

        
}

void bind_Console(py::module_& console)
{
    py::enum_<MessageType>(console, "MessageType")
        .value("Info", MessageType::Info)
        .value("Warning", MessageType::Warning)
        .value("Error", MessageType::Error)
        .value("Debug", MessageType::Debug)
        .value("Success", MessageType::Notice)
        .value("Performance", MessageType::Performance)
        .value("Notice", MessageType::Notice)
        .export_values();

    console.def("Log",&Log,"Log a message to the console",
        py::arg("module_name"),
        py::arg("message"),
        py::arg("type") = MessageType::Info
    );

    console.def("GetCredits", &GetCredits, "Get the credits for the Py4GW library");
    console.def("GetLicense", &GetLicense, "Get the license for the Py4GW library");
    console.def("change_working_directory",&ChangeWorkingDirectory,"Change the current working directory",
        py::arg("path")
    );
}

void bind_Environment(py::module_& console)
{
    console.def(
        "get_gw_window_handle",
        []() -> uintptr_t {
            return reinterpret_cast<uintptr_t>(
                Py4GW::get_gw_window_handle()
                );
        },
        "Get the Guild Wars window handle as an integer"
    );

    console.def(
        "get_projects_path",
        []() -> std::string {
            return dllDirectory;
        },
        "Get the path where Py4GW.dll is located"
    );
}

void bind_Window(py::module_& console)
{
    console.def("resize_window",&WindowCfg::ResizeWindow,"Resize the Guild Wars window",
        py::arg("width"), py::arg("height"));
    console.def("move_window_to",&WindowCfg::MoveWindowTo,"Move the Guild Wars window to (x, y)",
        py::arg("x"), py::arg("y"));
    console.def("set_window_geometry",&WindowCfg::SetWindowGeometry,"Set the Guild Wars window geometry (x, y, width, height)",
        py::arg("x"), py::arg("y"),
        py::arg("width"), py::arg("height")
    );
    console.def("get_window_rect",&WindowCfg::GetWindowRectFn,"Get the Guild Wars window rectangle (left, top, right, bottom)");
    console.def("get_client_rect",&WindowCfg::GetClientRectFn,"Get the Guild Wars client rectangle (left, top, right, bottom)");
    console.def("set_window_active",&WindowCfg::SetWindowActive,"Set the Guild Wars window as active (focused)");
    console.def("set_window_title",
        [](const std::wstring& title) {
            WindowCfg::SetWindowTitle(title);
        },
        py::arg("title"));
    console.def("is_window_active",&WindowCfg::IsWindowActive,"Check if the Guild Wars window is active (focused)");
    console.def("is_window_minimized",&WindowCfg::IsWindowMinimized,"Check if the Guild Wars window is minimized");
    console.def("is_window_in_background",&WindowCfg::IsWindowInBackground,"Check if the Guild Wars window is in the background");
    console.def("set_borderless",&WindowCfg::SetBorderless,"Enable or disable borderless window mode",
        py::arg("enable"));
    console.def("set_always_on_top", &WindowCfg::SetAlwaysOnTop,"Set or unset always-on-top",
        py::arg("enable"));
    console.def("flash_window",&WindowCfg::Flash_Window,"Flash the Guild Wars taskbar button",
        py::arg("repeat_count") = 1);
    console.def("request_attention",&WindowCfg::RequestAttention,"Keep flashing until the window comes to foreground");
    console.def("get_z_order",&WindowCfg::GetZOrder,"Get the Z-order index of the Guild Wars window");
    console.def("set_z_order",&WindowCfg::SetZOrder,"Set the Z-order of the Guild Wars window relative to another window",
        py::arg("insert_after") = (int)HWND_TOP);
    console.def("send_window_to_back",&WindowCfg::SendWindowToBack,"Send the Guild Wars window to the bottom of the Z-order stack");
    console.def("bring_window_to_front",&WindowCfg::BringWindowToFront,"Bring the Guild Wars window to the front of the Z-order stack");
    console.def("transparent_click_through",&WindowCfg::TransparentClickThrough,"Make the Guild Wars window click-through",
        py::arg("enable"));
    console.def("adjust_window_opacity", &WindowCfg::AdjustWindowOpacity,"Adjust the Guild Wars window opacity (0–255)",
        py::arg("alpha") );
    console.def( "hide_window",&WindowCfg::HideWindow, "Hide the Guild Wars window");
    console.def("show_window",&WindowCfg::ShowWindowAgain,"Show the Guild Wars window if hidden");
}

void bind_ScriptControl(py::module_& console)
{
    console.def("load", &Py4GW_LoadScript, "Load a Python script from path", py::arg("path"));
    console.def("run", &Py4GW_RunScript, "Run the currently loaded script");
    console.def("stop", &Py4GW_StopScript, "Stop the currently running script");
    console.def("pause", &Py4GW_PauseScript, "Pause the running script");
    console.def("resume", &Py4GW_ResumeScript, "Resume the paused script");
    console.def("status", &Py4GW_GetScriptStatus, "Get current script status");

    console.def( "defer_load_and_run",&Py4GW_DeferLoadAndRun,"Stop current if needed, then load and run new script after delay (ms)",
        py::arg("path"), py::arg("delay_ms") = 1000);
    console.def("defer_stop_load_and_run",&Py4GW_DeferStopLoadAndRun,"Force stop, then load and run new script after delay (ms)",
        py::arg("path"), py::arg("delay_ms") = 1000);
    console.def("defer_stop_and_run",&Py4GW_DeferStopAndRun,"Stop current script, then rerun it after delay (ms)",
        py::arg("delay_ms") = 1000);
}

void bind_Profiler(py::module_& console)
{
	console.def("get_profiler_metric_names", &GetProfilerMetricNames, "Get a list of all profiler metric names");
	console.def("get_profiler_reports", &GetProfilerReport, "Get a list of profiler reports with their metrics and values");
    console.def("get_profiler_history", GetProfilerHistory, "Get the history of profiler reports for a specific metric");
	console.def("clear_profiler_history", &ResetProfiler, "Clear the profiler history data");
}


void bind_Ping(py::module_& m)
{
    py::class_<PingTracker>(m, "PingHandler")
        .def(py::init<>())
        .def("Terminate", &PingTracker::Terminate)
        .def("GetCurrentPing", &PingTracker::GetCurrentPing)
        .def("GetAveragePing", &PingTracker::GetAveragePing)
        .def("GetMinPing", &PingTracker::GetMinPing)
        .def("GetMaxPing", &PingTracker::GetMaxPing);
}


PYBIND11_EMBEDDED_MODULE(Py4GW, m)
{
    m.doc() = "Py4GW, Python Enabler Library for GuildWars"; // Optional module docstring

    py::module_ console = m.def_submodule("Console", "Submodule for console logging");
	py::module_ game = m.def_submodule("Game", "Submodule for game functions");
    
    bind_Game(game);
	bind_Console(console);
	bind_Environment(console);
	bind_Window(console);
	bind_ScriptControl(console);
	bind_Profiler(console);
	bind_Ping(m);
}


PYBIND11_EMBEDDED_MODULE(PyCallback, m)
{
    m.doc() = "Frame callback scheduler with phased execution and priorities";

    // Optional but recommended: expose Phase enum
    py::enum_<PyCallback::Phase>(m, "Phase")
        .value("PreUpdate", PyCallback::Phase::PreUpdate)
        .value("Data", PyCallback::Phase::Data)
        .value("Update", PyCallback::Phase::Update)
        .export_values();

    py::enum_<PyCallback::Context>(m, "Context")
        .value("Update", PyCallback::Context::Update)
        .value("Draw", PyCallback::Context::Draw)
        .value("Main", PyCallback::Context::Main)
        .export_values();

    py::class_<PyCallback>(m, "PyCallback")
        .def_static(
            "Register",
            &PyCallback::Register,
            py::arg("name"),
            py::arg("fn"),
            py::arg("phase"),
            py::arg("priority") = 99,
			py::arg("context") = PyCallback::Context::Draw
        )
        .def_static(
            "RemoveById",
            &PyCallback::RemoveById,
            py::arg("id")
        )
        .def_static(
            "RemoveByName",
            &PyCallback::RemoveByName,
            py::arg("name")
        )
        .def_static(
			"PauseById",
			&PyCallback::PauseById,
			py::arg("id")
		)
        .def_static(
			"ResumeById",
			&PyCallback::ResumeById,
			py::arg("id")
		)

        .def_static(
			"IsPaused",
			&PyCallback::IsPaused,
			py::arg("id")
		)

        .def_static(
			"IsRegistered",
			&PyCallback::IsRegistered,
			py::arg("id")
		)

        .def_static(
            "Clear",
            &PyCallback::Clear
        )
        .def_static(
            "GetCallbackInfo",
            &PyCallback::GetCallbackInfo,
            R"doc(Returns a list of tuples)doc"
                );

}
