
class PyScanner {
public:
    // --- Initialize ---
    static void Initialize(const std::string& module_name = "") {
        if (module_name.empty())
            GW::Scanner::Initialize((const char*)nullptr);
        else
            GW::Scanner::Initialize(module_name.c_str());
    }

    // --- Find ---
    static uintptr_t Find(py::bytes pattern, const std::string& mask,
        int offset, uint8_t section) {
        std::string pat = pattern;
        return GW::Scanner::Find(pat.c_str(), mask.c_str(), offset,
            (GW::ScannerSection)section);
    }

    // --- FindAssertion ---
    static uintptr_t FindAssertion(const std::string& assertion_file,
        const std::string& assertion_msg,
        uint32_t line_number = 0,
        int offset = 0) {
        return GW::Scanner::FindAssertion(assertion_file.c_str(),
            assertion_msg.c_str(),
            line_number,
            offset);
    }

    // --- FindInRange ---
    static uintptr_t FindInRange(py::bytes pattern, const std::string& mask,
        int offset, uintptr_t start, uintptr_t end) {
        std::string pat = pattern;
        return GW::Scanner::FindInRange(pat.c_str(), mask.c_str(), offset,
            (DWORD)start, (DWORD)end);
    }

    // --- GetSectionAddressRange ---
    static py::tuple GetSectionAddressRange(uint8_t section) {
        uintptr_t start = 0;
        uintptr_t end = 0;
        GW::Scanner::GetSectionAddressRange((GW::ScannerSection)section, &start, &end);
        return py::make_tuple(start, end);
    }


    // --- FunctionFromNearCall ---
    static uintptr_t FunctionFromNearCall(uintptr_t call_addr,
        bool check_valid_ptr = true) {
        return GW::Scanner::FunctionFromNearCall(call_addr, check_valid_ptr);
    }

    // --- IsValidPtr ---
    static bool IsValidPtr(uintptr_t addr, uint8_t section) {
        return GW::Scanner::IsValidPtr(addr, (GW::ScannerSection)section);
    }

    // --- ToFunctionStart ---
    static uintptr_t ToFunctionStart(uintptr_t addr,
        uint32_t scan_range = 0xFF) {
        return GW::Scanner::ToFunctionStart(addr, scan_range);
    }

    // --- FindNthUseOfAddress ---
    static uintptr_t FindNthUseOfAddress(uintptr_t address, size_t nth,
        int offset, uint8_t section) {
        return GW::Scanner::FindNthUseOfAddress(address, nth, offset,
            (GW::ScannerSection)section);
    }

    // --- FindUseOfAddress ---
    static uintptr_t FindUseOfAddress(uintptr_t address, int offset,
        uint8_t section) {
        return GW::Scanner::FindUseOfAddress(address, offset,
            (GW::ScannerSection)section);
    }

    // --- FindUseOfString (ANSI) ---
    static uintptr_t FindUseOfStringA(const std::string& str, int offset,
        uint8_t section) {
        return GW::Scanner::FindUseOfString(str.c_str(), offset,
            (GW::ScannerSection)section);
    }

    // --- FindUseOfString (WCHAR) ---
    static uintptr_t FindUseOfStringW(const std::wstring& str, int offset,
        uint8_t section) {
        return GW::Scanner::FindUseOfString(str.c_str(), offset,
            (GW::ScannerSection)section);
    }

    // --- FindNthUseOfString (ANSI) ---
    static uintptr_t FindNthUseOfStringA(const std::string& str, size_t nth,
        int offset, uint8_t section) {
        return GW::Scanner::FindNthUseOfString(str.c_str(), nth, offset,
            (GW::ScannerSection)section);
    }

    // --- FindNthUseOfString (WCHAR) ---
    static uintptr_t FindNthUseOfStringW(const std::wstring& str, size_t nth,
        int offset, uint8_t section) {
        return GW::Scanner::FindNthUseOfString(str.c_str(), nth, offset,
            (GW::ScannerSection)section);
    }

    // --- GetScanStatus ---
    static py::dict GetScanStatus() {
        py::dict result;
        py::dict scans;
        for (const auto& [name, addr] : Logger::GetScanResults()) {
            scans[py::cast(name)] = addr;
        }
        py::dict hooks;
        for (const auto& [name, status] : Logger::GetHookResults()) {
            hooks[py::cast(name)] = status;
        }
        result["scans"] = scans;
        result["hooks"] = hooks;
        return result;
    }
};


// pybind11 scanner module binding
PYBIND11_EMBEDDED_MODULE(PyScanner, m)
{
    py::class_<PyScanner>(m, "PyScanner")
        // Init
        .def_static("Initialize", &PyScanner::Initialize,
            py::arg("module_name") = "")

        // Pattern scanning
        .def_static("Find", &PyScanner::Find)
        .def_static("FindInRange", &PyScanner::FindInRange)

        // Function resolution
        .def_static("FunctionFromNearCall", &PyScanner::FunctionFromNearCall)
        .def_static("ToFunctionStart", &PyScanner::ToFunctionStart)

        // Pointer validation
        .def_static("IsValidPtr", &PyScanner::IsValidPtr)

        // Address usage scanning
        .def_static("FindUseOfAddress", &PyScanner::FindUseOfAddress)
        .def_static("FindNthUseOfAddress", &PyScanner::FindNthUseOfAddress)

        // String usage scanning
        .def_static("FindUseOfStringA", &PyScanner::FindUseOfStringA)
        .def_static("FindUseOfStringW", &PyScanner::FindUseOfStringW)
        .def_static("FindNthUseOfStringA", &PyScanner::FindNthUseOfStringA)
        .def_static("FindNthUseOfStringW", &PyScanner::FindNthUseOfStringW)

        .def_static("FindAssertion", &PyScanner::FindAssertion,
            py::arg("assertion_file"),
            py::arg("assertion_msg"),
            py::arg("line_number") = 0,
            py::arg("offset") = 0)

        .def_static("GetSectionAddressRange", &PyScanner::GetSectionAddressRange,
            py::arg("section"))

        .def_static("GetScanStatus", &PyScanner::GetScanStatus);

}


