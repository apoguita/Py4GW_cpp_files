# Py4GW Crash Handler

Forensic crash capture for Py4GW. When Guild Wars or `Py4GW.dll` faults, it leaves a
per-client artifact set that answers: **what native instruction crashed**, **what GW itself
said about it**, and **which Python script was last active**.

Status: **implemented, built, deployed, and verified live.** One enhancement is pending — see
[Pending: Python breadcrumb](#pending-python-breadcrumb).

Source: `include/CrashHandler.h`, `src/CrashHandler.cpp`, `include/Breadcrumbs.h` (native);
`Py4GWCoreLib/py4gwcorelib_src/CrashLog.py` (Python). Bindings: `Py4GW.crashlog` / `Py4GW.debug`
in `src/Py4GW.cpp`.

---

## Architecture

Four capture paths feed one crash sink (`CrashHandler::OnException`), which writes a minidump +
sidecars under a single re-entrancy guard, then lets the process die.

| Path | Trigger | Mechanism |
|---|---|---|
| **A — SEH** | any unhandled native fault (AV, illegal instr, stack overflow) | `SetUnhandledExceptionFilter` |
| **B — GWCA panic** | a GWCA `FatalAssert` (e.g. a `Scanner::Find` pattern rotted after a GW update) | `GW::RegisterPanicHandler`; self-dumps via `RtlCaptureContext` (GWCA `abort()`s, so it can't rely on Path A) |
| **C — GW engine** | GW's own engine hits a fatal (assertion, packet/state error) — **captures the exact crashing GW function** | detours GW's internal crash-message builder |
| **D — Python** | uncaught Python exception | `sys.excepthook` + `threading.excepthook` write a full traceback |

The native side cannot read the live Python stack (the GIL/interpreter may be inconsistent at
fault time), so Python context comes from two places: a **pre-captured last-frame breadcrumb**
(for native faults) and the **excepthook tracebacks** (for Python-level exceptions).

### Install / teardown (`src/dllmain.cpp`)
- `CrashHandler::Instance().Initialize()` right after `InitializeGWCA()` succeeds (`dllmain.cpp:71`)
  — Paths B/C need GWCA live; the SEH filter and the process-exception-policy change happen here.
- `CrashHandler::Instance().Terminate()` before `Py4GW::Instance().Terminate()` (`dllmain.cpp:131`)
  — disables/removes the Path C hook, restores the SEH filter + exception policy.

### Process exception policy
`Initialize()` clears `PROCESS_CALLBACK_FILTER_ENABLED` (via dynamically-resolved
`Get/SetProcessUserModeExceptionPolicy`) and saves the prior value to restore at teardown. Without
this, exceptions raised inside kernel-mode callbacks (window procs / D3D present) are silently
swallowed and never reach the SEH filter — essential for a GUI overlay injected into a game.

---

## Path C — GW's internal crash-message builder (verified)

This is the path that names the exact crashing GW function. When GW's engine hits a fatal, it
builds a crash report (assertion text, registers, loaded-module list, and a stack trace) into an
internal debug-info buffer. Py4GW resolves that builder **at runtime by a stable format-string
anchor** and detours it, capturing GW's own report + a CONTEXT whose `Eip` is the faulting
instruction.

Verified against the live client (`Gw.exe`, 2026-06-02 build) — the target function and ABI:

| Fact | Value |
|---|---|
| Resolution | `GW::Scanner::FindUseOfString("%p  %08x %08x %08x %08x ")` -> `ToFunctionStart(use, 0xfff)` |
| Target (this build) | `0x00488fe0` |
| Calling convention | `__cdecl`, **7 args** `(debug_info*, u32, u32, u32, CONTEXT* ctx, u32, u32)` |
| Debug-info text offset | `+0x20c` (length-counted; bound `< 0x80000`) |
| `CONTEXT.Eip` | `0xB8` |
| Report-section delimiter | `*--> <section> <--*` |

The detour forwards all 7 args to the trampoline, lets GW fill its report, captures it, synthesizes
an `EXCEPTION_RECORD` (`EXCEPTION_BREAKPOINT`, address = `ctx->Eip`), and dumps. The `0x20c` read is
guarded by `__try/__except` and gated on the scanner hit; on a scan miss it **logs and continues**
so Paths A/B stay functional. The address is re-resolved per process, so **re-anchor (not
re-address) after a GW patch** — the convention and offsets are stable; only the address moves.

Boot-time smoke test: `Py4GW_injection_log.txt` logs `[CrashHandler] Path C attached (GW
crash-message hook).` when the anchor resolves.

---

## Crash-time safety invariants

The SEH writer path runs while the process is dying, so it is:
- **allocation-free** — static buffers only, no `new`/STL allocation;
- **lock-free** — no `std::mutex`; the breadcrumb ring is a lock-free MPSC with release/acquire seq;
- **Python-API-free** — never calls any `Py*` function;
- **non-recursive** — one `InterlockedCompareExchange` guard; a fault during dumping terminates
  rather than recursing; `MiniDumpWriteDump` is wrapped in `__try/__except`;
- **x86 only** — `static_assert(sizeof(void*) == 4)` guards the `CONTEXT.Eip` assumption.

The C++ `Logger` (heap + mutex) is used only at install/teardown, never on the crash path. The
crash path appends one line to `Py4GW_injection_log.txt` via a raw `CreateFileW(FILE_APPEND_DATA)`.

---

## Output

Per-client. The crash directory is `<client dir>/crashes/` — the process working directory captured
at `Initialize()`, which (before Py4GW's first-frame `ChangeWorkingDirectory`) is the GW client's own
folder, so each multibox instance gets an isolated `crashes/` next to its own
`Py4GW_injection_log.txt`. The Python side fetches the same path via `Py4GW.crashlog.get_crash_dir()`.

Per crash, sharing a `py4gw-<YYYYMMDD>-<HHMMSS>-<pid>-<tid>` stem:

| File | Contents |
|---|---|
| `....dmp` | minidump, flags `0x1041` (`DataSegs \| IndirectlyReferencedMemory \| ThreadInfo`), exception info + a `CommentStreamA` summary |
| `....json` | sidecar (below) |
| `...-gwtext.txt` | GW's **full** crash report verbatim (Path C only) |
| `...-<tag>-pytrace.txt` | full Python `traceback.format_exc()` (Python-uncaught only) |

Plus one `CRASH ...` line appended to `Py4GW_injection_log.txt`.

### Sidecar JSON
```json
{
  "version": "0.0.0-dev",
  "source": "gw_engine",            // seh | wndproc | gwca_assert | gw_engine
  "crash_class": "breakpoint",      // derived from the real ExceptionCode
  "exception_code": "0x80000003",
  "fault_address": "0x00EA7A9B",
  "faulting_tid": 48864,
  "dump_file": "py4gw-20260614-011912-13084-48864.dmp",
  "gw_text_file": "py4gw-20260614-011912-13084-48864-gwtext.txt",
  "python_last_frame": { "file": "", "line": 0, "func": "" },
  "assert": "",
  "gw_text": "*--> Crash <--*\r\nAssertion: ...\r\nP:\\Code\\Gw\\...(NNN)\r\n...",
  "breadcrumbs": ["gw_text: *--> Crash <--* ..."]
}
```
All interpolated strings are JSON-escaped. `gw_text` is a ~1 KB preview; the full report is in
`gw_text_file`.

### Live-verified example
A real crash captured at `...\Guild Wars alt 7\crashes\` (Path C):
`source: gw_engine`, and `gw_text`:
`Assertion: !(manualAgentId && !ManagerFindAgent(manualAgentId))  P:\Code\Gw\AgentView\AvSelect.cpp(780)`
— i.e. a change-target on a stale/despawned agent id. The handler named the exact GW function, file,
and line.

---

## Python side (`CrashLog.py`)

`install_crash_hooks()` (called once from `Py4GWCoreLib/__init__.py` after the stdout/stderr redirect):
- installs `sys.excepthook` + `threading.excepthook` -> full `traceback.format_exc()` to a
  `-pytrace.txt` for uncaught exceptions on the render thread and worker threads;
- exposes `set_breadcrumb(widget, phase)` which snapshots the current Python frame into the native
  TLS slot (via `Py4GW.crashlog.set_last_frame`) so a native crash can name the last Python frame.

`faulthandler` is **off by default**: Py4GW routinely raises+swallows native exceptions, and
`faulthandler`'s vectored handler logs every one process-wide (huge file + overhead). Pass
`install_crash_hooks(enable_faulthandler=True)` only for quiet single-client interpreter-fault
debugging.

---

## Bindings (`Py4GW.crashlog` / `Py4GW.debug`)
- `Py4GW.crashlog.set_last_frame(file, line, func)` — record the last Python frame (TLS).
- `Py4GW.crashlog.breadcrumb(text)` — append to the crash ring.
- `Py4GW.crashlog.get_crash_dir()` — the native crash dir (so Python writes beside the dump).
- `Py4GW.debug.crash()` — deliberate access violation to exercise the handler.
  **Gated behind `PY4GW_DEBUG_BUILD`** — remove that define in `CMakeLists.txt` for production.

---

## Build / deploy

- `CMakeLists.txt` links `dbghelp` and defines `PY4GW_DEBUG_BUILD` (testing). `src/*.cpp` is
  glob-included; new sources need a CMake re-configure.
- Build: `cmake --build build --config RelWithDebInfo --target Py4GW` (VS 2022, Win32, Python 3.13-32).
- Deploy: copy `bin/RelWithDebInfo/Py4GW.dll` into the runtime tree (the DLL is locked while clients
  run — close them first). Running clients keep their old code in memory until relaunched.
- Test: a fresh client -> `Py4GW.debug.crash()` -> expect one dump in that client's own `crashes/` with
  `source:"seh"`, `exception_code:"0xC0000005"`.

---

## Pending: Python breadcrumb

`python_last_frame` is currently empty — the breadcrumb infrastructure exists but nothing calls
`set_last_frame`/`set_breadcrumb` yet. Two call sites are needed (per-faulting-thread TLS):
1. **Game-thread enqueue runner** (`src/Py4GW.cpp`, the `GW::GameThread::Enqueue` lambda) — record
   the enqueued callback's `__code__` before running it. Covers deferred-call crashes like the
   AvSelect example. (C++ — needs a rebuild.)
2. **WidgetManager per-widget dispatch** (`Py4GWCoreLib/py4gwcorelib_src/WidgetManager.py`) — call
   `CrashLog.set_breadcrumb(widget, phase)` before each widget callback. Covers crashes during a
   widget's draw on the render thread. (Python only — no rebuild.)

---

## Known limitations
- The `0x20c` text offset and the Path C target address are build-specific; re-anchor on GW patches.
- Pre-existing `__except (EXCEPTION_EXECUTE_HANDLER)` swallow blocks (`GwDatTextureManager`,
  `py_dialog`, `dllmain` `SafeWndProc`) pre-empt the SEH filter by design; a missing dump there does
  not imply no fault.
- `WndProcFilter` exists but is intentionally **not wired** — GW's window proc raises survivable
  exceptions routinely, so its `__except` is left as a silent swallow.
