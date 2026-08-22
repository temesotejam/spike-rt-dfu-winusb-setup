#include <windows.h>

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace {

constexpr int kResourceGui = 201;
constexpr int kResourceWorker = 202;
constexpr int kResourceLibwdi = 203;

struct PayloadPaths {
    std::filesystem::path root;
    std::filesystem::path gui;
    std::filesystem::path runtime;
    std::filesystem::path worker;
    std::filesystem::path libwdi;
};

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

PayloadPaths make_payload_paths() {
    PayloadPaths paths;
    paths.root = std::filesystem::temp_directory_path() /
        (L"spike-rt-dfu-winusb-setup-v0.3-" + std::to_wstring(GetCurrentProcessId()));
    paths.gui = paths.root / L"SPIKE-RT-DFU-WinUSB-Setup-inner.exe";
    paths.runtime = paths.root / L"runtime";
    paths.worker = paths.runtime / L"spike-rt-dfu-winusb-worker.exe";
    paths.libwdi = paths.runtime / L"libwdi.dll";
    return paths;
}

bool write_resource_to_file(int resource_id, const std::filesystem::path& destination, std::wstring& error) {
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resource_id), RT_RCDATA);
    if (resource == nullptr) {
        error = L"埋め込みリソースを見つけられませんでした: " + std::to_wstring(resource_id);
        return false;
    }

    HGLOBAL loaded = LoadResource(module, resource);
    if (loaded == nullptr) {
        error = L"埋め込みリソースを読み込めませんでした: " + std::to_wstring(resource_id);
        return false;
    }

    const DWORD size = SizeofResource(module, resource);
    const void* data = LockResource(loaded);
    if (data == nullptr || size == 0) {
        error = L"埋め込みリソースの内容が空です: " + std::to_wstring(resource_id);
        return false;
    }

    std::error_code fs_error;
    std::filesystem::create_directories(destination.parent_path(), fs_error);
    if (fs_error) {
        error = L"一時フォルダを作成できませんでした: " + destination.parent_path().wstring();
        return false;
    }

    HANDLE file = CreateFileW(
        destination.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = L"一時ファイルを作成できませんでした: " + destination.wstring() + L"\n" +
            windows_error_message(GetLastError());
        return false;
    }

    const auto* bytes = static_cast<const unsigned char*>(data);
    DWORD total_written = 0;
    bool ok = true;
    while (total_written < size) {
        DWORD written = 0;
        const DWORD remaining = size - total_written;
        if (!WriteFile(file, bytes + total_written, remaining, &written, nullptr) || written == 0) {
            error = L"一時ファイルへ書き込めませんでした: " + destination.wstring() + L"\n" +
                windows_error_message(GetLastError());
            ok = false;
            break;
        }
        total_written += written;
    }

    CloseHandle(file);
    return ok;
}

bool extract_payload(PayloadPaths& paths, std::wstring& error) {
    std::error_code fs_error;
    std::filesystem::remove_all(paths.root, fs_error);
    fs_error.clear();
    std::filesystem::create_directories(paths.runtime, fs_error);
    if (fs_error) {
        error = L"一時フォルダを準備できませんでした: " + paths.root.wstring();
        return false;
    }

    if (!write_resource_to_file(kResourceGui, paths.gui, error)) {
        return false;
    }
    if (!write_resource_to_file(kResourceWorker, paths.worker, error)) {
        return false;
    }
    if (!write_resource_to_file(kResourceLibwdi, paths.libwdi, error)) {
        return false;
    }

    return true;
}

DWORD run_process(
    const std::filesystem::path& executable,
    const std::wstring& arguments,
    const std::filesystem::path& working_directory,
    bool hidden,
    std::wstring& error) {

    std::wstring command = L"\"" + executable.wstring() + L"\"";
    if (!arguments.empty()) {
        command += L" ";
        command += arguments;
    }

    std::vector<wchar_t> command_buffer(command.begin(), command.end());
    command_buffer.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    if (hidden) {
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_HIDE;
    }

    PROCESS_INFORMATION process{};
    const DWORD creation_flags = hidden ? CREATE_NO_WINDOW : 0;
    const BOOL created = CreateProcessW(
        nullptr,
        command_buffer.data(),
        nullptr,
        nullptr,
        FALSE,
        creation_flags,
        nullptr,
        working_directory.c_str(),
        &startup,
        &process);

    if (!created) {
        error = L"内部プログラムを起動できませんでした。\n" + windows_error_message(GetLastError());
        return static_cast<DWORD>(-1);
    }

    WaitForSingleObject(process.hProcess, INFINITE);

    DWORD exit_code = static_cast<DWORD>(-1);
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return exit_code;
}

void best_effort_cleanup(const std::filesystem::path& root) {
    std::error_code fs_error;
    std::filesystem::remove_all(root, fs_error);
    // If a child process still has a file open, leaving this directory in the
    // user's temporary area is safer than forcing removal while it is in use.
}

bool has_bundle_self_test_argument(PWSTR command_line) {
    if (command_line == nullptr) {
        return false;
    }
    const std::wstring args(command_line);
    return args.find(L"--bundle-self-test") != std::wstring::npos;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR command_line, int) {
    PayloadPaths paths = make_payload_paths();
    std::wstring error;

    if (!extract_payload(paths, error)) {
        MessageBoxW(
            nullptr,
            error.c_str(),
            L"SPIKE-RT DFU WinUSB Setup",
            MB_OK | MB_ICONERROR);
        best_effort_cleanup(paths.root);
        return 20;
    }

    if (has_bundle_self_test_argument(command_line)) {
        const DWORD exit_code = run_process(
            paths.worker,
            L"--self-test --no-pause",
            paths.runtime,
            true,
            error);
        best_effort_cleanup(paths.root);
        return static_cast<int>(exit_code);
    }

    const DWORD exit_code = run_process(
        paths.gui,
        L"",
        paths.root,
        false,
        error);

    if (exit_code == static_cast<DWORD>(-1)) {
        MessageBoxW(
            nullptr,
            error.c_str(),
            L"SPIKE-RT DFU WinUSB Setup",
            MB_OK | MB_ICONERROR);
        best_effort_cleanup(paths.root);
        return 21;
    }

    best_effort_cleanup(paths.root);
    return static_cast<int>(exit_code);
}
