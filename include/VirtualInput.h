
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
