#include <windows.h>

#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr wchar_t kWindowClass[] = L"SpikeRtDfuWinUsbSetupWindow";
constexpr wchar_t kWindowTitle[] = L"SPIKE-RT DFU WinUSB Setup";
constexpr UINT kMsgRefreshDone = WM_APP + 1;
constexpr UINT kMsgInstallDone = WM_APP + 2;

constexpr int kIdStatus = 1001;
constexpr int kIdBody = 1002;
constexpr int kIdDetails = 1003;
constexpr int kIdRefresh = 1004;
constexpr int kIdInstall = 1005;
constexpr int kIdClose = 1006;

enum class UiState {
    Checking,
    Ready,
    NeedsInstall,
    NoDevice,
    MultipleDevices,
    IdentityMismatch,
    Error,
};

struct WorkerResult {
    bool launched = false;
    DWORD exit_code = static_cast<DWORD>(-1);
    std::string output;
    std::wstring error;
};

struct UiPayload {
    UiState state = UiState::Error;
    std::wstring details;
    std::wstring error;
    DWORD exit_code = static_cast<DWORD>(-1);
};

struct AppUi {
    HWND status = nullptr;
    HWND body = nullptr;
    HWND details = nullptr;
    HWND refresh = nullptr;
    HWND install = nullptr;
    HWND close = nullptr;
    HFONT title_font = nullptr;
    HFONT status_font = nullptr;
    HFONT body_font = nullptr;
    UiState state = UiState::Checking;
    bool busy = false;
};

AppUi g_ui;

std::wstring windows_error_message(DWORD error) {
    wchar_t* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(
        flags,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&buffer),
        0,
        nullptr);

    std::wstring message;
    if (length != 0 && buffer != nullptr) {
        message.assign(buffer, length);
        LocalFree(buffer);
    } else {
        message = L"Windows error " + std::to_wstring(error);
    }
    return message;
}

std::filesystem::path executable_directory() {
    std::vector<wchar_t> buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }
    return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
}

std::filesystem::path worker_path() {
    return executable_directory() / L"runtime" / L"spike-rt-dfu-winusb-worker.exe";
}

std::wstring bytes_to_wide(const std::string& bytes) {
    if (bytes.empty()) {
        return {};
    }

    auto convert = [&bytes](UINT code_page, DWORD flags) -> std::wstring {
        const int required = MultiByteToWideChar(
            code_page,
            flags,
            bytes.data(),
            static_cast<int>(bytes.size()),
            nullptr,
            0);
        if (required <= 0) {
            return {};
        }
        std::wstring result(static_cast<size_t>(required), L'\0');
        if (MultiByteToWideChar(
                code_page,
                flags,
                bytes.data(),
                static_cast<int>(bytes.size()),
                result.data(),
                required) != required) {
            return {};
        }
        return result;
    };

    std::wstring result = convert(CP_UTF8, MB_ERR_INVALID_CHARS);
    if (!result.empty()) {
        return result;
    }
    return convert(CP_ACP, 0);
}

WorkerResult run_worker(const std::wstring& arguments, const std::string& stdin_text = {}) {
    WorkerResult result;
    const std::filesystem::path worker = worker_path();

    if (!std::filesystem::exists(worker)) {
        result.error = L"runtime\\spike-rt-dfu-winusb-worker.exe が見つかりません。\nZIPを展開したフォルダ構成を変更せずに実行してください。";
        return result;
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE stdout_read = nullptr;
    HANDLE stdout_write = nullptr;
    HANDLE stdin_read = nullptr;
    HANDLE stdin_write = nullptr;

    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0) ||
        !SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0) ||
        !CreatePipe(&stdin_read, &stdin_write, &sa, 0) ||
        !SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0)) {
        const DWORD error = GetLastError();
        if (stdout_read) CloseHandle(stdout_read);
        if (stdout_write) CloseHandle(stdout_write);
        if (stdin_read) CloseHandle(stdin_read);
        if (stdin_write) CloseHandle(stdin_write);
        result.error = L"worker用パイプを作成できませんでした: " + windows_error_message(error);
        return result;
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = stdout_write;
    startup.hStdError = stdout_write;
    startup.hStdInput = stdin_read;

    PROCESS_INFORMATION process{};
    std::wstring command = L"\"" + worker.wstring() + L"\"";
    if (!arguments.empty()) {
        command += L" ";
        command += arguments;
    }
    std::vector<wchar_t> command_buffer(command.begin(), command.end());
    command_buffer.push_back(L'\0');

    const std::wstring working_directory = worker.parent_path().wstring();
    const BOOL created = CreateProcessW(
        nullptr,
        command_buffer.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        working_directory.c_str(),
        &startup,
        &process);

    CloseHandle(stdout_write);
    CloseHandle(stdin_read);

    if (!created) {
        const DWORD error = GetLastError();
        CloseHandle(stdout_read);
        CloseHandle(stdin_write);
        result.error = L"workerを起動できませんでした: " + windows_error_message(error);
        return result;
    }

    result.launched = true;

    if (!stdin_text.empty()) {
        DWORD written = 0;
        WriteFile(
            stdin_write,
            stdin_text.data(),
            static_cast<DWORD>(stdin_text.size()),
            &written,
            nullptr);
    }
    CloseHandle(stdin_write);

    char buffer[4096];
    for (;;) {
        DWORD bytes_read = 0;
        const BOOL ok = ReadFile(stdout_read, buffer, sizeof(buffer), &bytes_read, nullptr);
        if (!ok || bytes_read == 0) {
            break;
        }
        result.output.append(buffer, buffer + bytes_read);
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    GetExitCodeProcess(process.hProcess, &result.exit_code);

    CloseHandle(stdout_read);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return result;
}

bool output_contains(const WorkerResult& result, const char* text) {
    return result.output.find(text) != std::string::npos;
}

UiPayload classify_status(const WorkerResult& result) {
    UiPayload payload;
    payload.exit_code = result.exit_code;
    payload.details = bytes_to_wide(result.output);
    payload.error = result.error;

    if (!result.launched) {
        payload.state = UiState::Error;
        return payload;
    }

    if (output_contains(result, "Status: WinUSB is already assigned")) {
        payload.state = UiState::Ready;
    } else if (output_contains(result, "Detect-only mode: WinUSB is not assigned")) {
        payload.state = UiState::NeedsInstall;
    } else if (result.exit_code == 2 || output_contains(result, "No target DFU Hub was found")) {
        payload.state = UiState::NoDevice;
    } else if (result.exit_code == 3 || output_contains(result, "devices matching VID 0694 / PID 0008 are connected")) {
        payload.state = UiState::MultipleDevices;
    } else if (result.exit_code == 4 || output_contains(result, "device metadata does not match")) {
        payload.state = UiState::IdentityMismatch;
    } else {
        payload.state = UiState::Error;
        if (payload.error.empty()) {
            payload.error = L"状態確認workerが予期しない結果を返しました。終了コード: " + std::to_wstring(result.exit_code);
        }
    }

    return payload;
}

void set_control_font(HWND control, HFONT font) {
    if (control && font) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
}

void update_buttons() {
    EnableWindow(g_ui.refresh, g_ui.busy ? FALSE : TRUE);
    EnableWindow(g_ui.install, (!g_ui.busy && g_ui.state == UiState::NeedsInstall) ? TRUE : FALSE);
    EnableWindow(g_ui.close, TRUE);
}

void set_busy(bool busy) {
    g_ui.busy = busy;
    update_buttons();
}

void apply_state(UiState state, const std::wstring& details, const std::wstring& error = {}) {
    g_ui.state = state;

    std::wstring heading;
    std::wstring body;

    switch (state) {
        case UiState::Checking:
            heading = L"Hubを確認しています…";
            body = L"LEGO SPIKE Prime / Technic Large Hub のDFUモードを確認しています。";
            break;
        case UiState::Ready:
            heading = L"準備完了";
            body = L"WinUSBは設定済みです。このPCではSPIKE-RTのWebUSB書き込みを使用できます。";
            break;
        case UiState::NeedsInstall:
            heading = L"WinUSBセットアップが必要です";
            body = L"対象のDFU Hubを確認しました。「WinUSBを設定」を押すと、このHubだけをWinUSBへ設定します。";
            break;
        case UiState::NoDevice:
            heading = L"DFUモードのHubが見つかりません";
            body = L"HubをDFUモードにしてUSB接続し、「再確認」を押してください。PC側の他のUSB機器は変更しません。";
            break;
        case UiState::MultipleDevices:
            heading = L"安全のため停止しました";
            body = L"対象となるDFU Hubが複数見つかりました。1台だけ接続して「再確認」を押してください。";
            break;
        case UiState::IdentityMismatch:
            heading = L"対象デバイスを確認できません";
            body = L"VID/PIDは一致しましたが、LEGO Large Hub DFUとして安全に確認できないため何も変更しません。";
            break;
        case UiState::Error:
            heading = L"確認に失敗しました";
            body = error.empty() ? L"予期しないエラーが発生しました。詳細を確認してください。" : error;
            break;
    }

    SetWindowTextW(g_ui.status, heading.c_str());
    SetWindowTextW(g_ui.body, body.c_str());
    SetWindowTextW(g_ui.details, details.c_str());
    update_buttons();
}

void begin_refresh(HWND window) {
    if (g_ui.busy) {
        return;
    }

    g_ui.state = UiState::Checking;
    set_busy(true);
    apply_state(UiState::Checking, L"");

    std::thread([window]() {
        const WorkerResult worker = run_worker(L"--detect-only --no-pause");
        auto* payload = new UiPayload(classify_status(worker));
        if (!PostMessageW(window, kMsgRefreshDone, 0, reinterpret_cast<LPARAM>(payload))) {
            delete payload;
        }
    }).detach();
}

void begin_install(HWND window) {
    if (g_ui.busy || g_ui.state != UiState::NeedsInstall) {
        return;
    }

    const int choice = MessageBoxW(
        window,
        L"LEGO Technic Large Hub in DFU Mode (USB 0694:0008) だけを対象にWinUSBを設定します。\n\n"
        L"他のUSB機器やSPIKE-RT実行時のCOMポートは変更しません。\n\n"
        L"続行しますか？",
        L"WinUSBを設定",
        MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2);

    if (choice != IDYES) {
        return;
    }

    set_busy(true);
    SetWindowTextW(g_ui.status, L"WinUSBを設定しています…");
    SetWindowTextW(g_ui.body, L"Windowsの確認画面が表示された場合は内容を確認してください。処理が終わるまでHubを抜かないでください。");

    std::thread([window]() {
        const WorkerResult worker = run_worker(L"--no-pause", "INSTALL\r\n");
        auto* payload = new UiPayload();
        payload->exit_code = worker.exit_code;
        payload->details = bytes_to_wide(worker.output);
        payload->error = worker.error;

        if (worker.launched && worker.exit_code == 0 &&
            (output_contains(worker, "WinUSB setup completed and was verified") ||
             output_contains(worker, "Status: WinUSB is already assigned"))) {
            payload->state = UiState::Ready;
        } else {
            payload->state = UiState::Error;
            if (payload->error.empty()) {
                payload->error = L"WinUSBセットアップを完了できませんでした。終了コード: " + std::to_wstring(worker.exit_code);
            }
        }

        if (!PostMessageW(window, kMsgInstallDone, 0, reinterpret_cast<LPARAM>(payload))) {
            delete payload;
        }
    }).detach();
}

HWND create_control(
    HWND parent,
    DWORD ex_style,
    const wchar_t* class_name,
    const wchar_t* text,
    DWORD style,
    int x,
    int y,
    int width,
    int height,
    int id) {
    return CreateWindowExW(
        ex_style,
        class_name,
        text,
        style,
        x,
        y,
        width,
        height,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr),
        nullptr);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE: {
            g_ui.title_font = CreateFontW(
                -26, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_ui.status_font = CreateFontW(
                -20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_ui.body_font = CreateFontW(
                -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            HWND title = create_control(
                window, 0, L"STATIC", L"SPIKE-RT DFU WinUSB Setup",
                WS_CHILD | WS_VISIBLE, 24, 18, 560, 34, 0);
            set_control_font(title, g_ui.title_font);

            HWND subtitle = create_control(
                window, 0, L"STATIC", L"LEGO SPIKE Prime / Technic Large Hub のDFU専用セットアップ",
                WS_CHILD | WS_VISIBLE, 25, 52, 560, 24, 0);
            set_control_font(subtitle, g_ui.body_font);

            g_ui.status = create_control(
                window, 0, L"STATIC", L"Hubを確認しています…",
                WS_CHILD | WS_VISIBLE, 24, 88, 560, 30, kIdStatus);
            set_control_font(g_ui.status, g_ui.status_font);

            g_ui.body = create_control(
                window, 0, L"STATIC", L"",
                WS_CHILD | WS_VISIBLE, 24, 122, 560, 52, kIdBody);
            set_control_font(g_ui.body, g_ui.body_font);

            g_ui.details = create_control(
                window, WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                24, 184, 560, 150, kIdDetails);
            set_control_font(g_ui.details, g_ui.body_font);

            g_ui.refresh = create_control(
                window, 0, L"BUTTON", L"再確認",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                24, 352, 120, 34, kIdRefresh);
            set_control_font(g_ui.refresh, g_ui.body_font);

            g_ui.install = create_control(
                window, 0, L"BUTTON", L"WinUSBを設定",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                154, 352, 180, 34, kIdInstall);
            set_control_font(g_ui.install, g_ui.body_font);

            g_ui.close = create_control(
                window, 0, L"BUTTON", L"閉じる",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                464, 352, 120, 34, kIdClose);
            set_control_font(g_ui.close, g_ui.body_font);

            apply_state(UiState::Checking, L"");
            PostMessageW(window, WM_COMMAND, MAKEWPARAM(kIdRefresh, BN_CLICKED), 0);
            return 0;
        }

        case WM_COMMAND: {
            switch (LOWORD(wparam)) {
                case kIdRefresh:
                    begin_refresh(window);
                    return 0;
                case kIdInstall:
                    begin_install(window);
                    return 0;
                case kIdClose:
                    DestroyWindow(window);
                    return 0;
            }
            break;
        }

        case kMsgRefreshDone: {
            auto* payload = reinterpret_cast<UiPayload*>(lparam);
            if (payload) {
                set_busy(false);
                apply_state(payload->state, payload->details, payload->error);
                delete payload;
            }
            return 0;
        }

        case kMsgInstallDone: {
            auto* payload = reinterpret_cast<UiPayload*>(lparam);
            if (payload) {
                set_busy(false);
                if (payload->state == UiState::Ready) {
                    g_ui.state = UiState::Ready;
                    SetWindowTextW(g_ui.status, L"セットアップ完了");
                    SetWindowTextW(g_ui.body, L"WinUSBの設定と再確認が完了しました。SPIKE-RTのWebUSB書き込みを使用できます。");
                    SetWindowTextW(g_ui.details, payload->details.c_str());
                    update_buttons();
                } else {
                    apply_state(UiState::Error, payload->details, payload->error);
                }
                delete payload;
            }
            return 0;
        }

        case WM_DESTROY:
            if (g_ui.title_font) DeleteObject(g_ui.title_font);
            if (g_ui.status_font) DeleteObject(g_ui.status_font);
            if (g_ui.body_font) DeleteObject(g_ui.body_font);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(window, message, wparam, lparam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    SetProcessDPIAware();

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    window_class.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    window_class.lpszClassName = kWindowClass;

    if (!RegisterClassExW(&window_class)) {
        return 1;
    }

    RECT rect{0, 0, 610, 420};
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&rect, style, FALSE);

    HWND window = CreateWindowExW(
        0,
        kWindowClass,
        kWindowTitle,
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!window) {
        return 2;
    }

    ShowWindow(window, show_command);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}
