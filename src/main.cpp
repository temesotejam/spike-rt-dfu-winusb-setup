#include <windows.h>
#include <setupapi.h>
#include <libwdi.h>

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr wchar_t kTargetVidPid[] = L"VID_0694&PID_0008";
constexpr wchar_t kExpectedDescription[] = L"LEGO Technic Large Hub in DFU Mode";
constexpr wchar_t kExpectedManufacturerToken[] = L"LEGO";
constexpr unsigned short kTargetVid = 0x0694;
constexpr unsigned short kTargetPid = 0x0008;
constexpr char kInfName[] = "spike_rt_dfu_winusb.inf";

struct DeviceInfo {
    std::wstring instance_id;
    std::wstring description;
    std::wstring friendly_name;
    std::wstring manufacturer;
    std::wstring service;
    std::wstring device_class;
};

std::wstring to_upper(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towupper(ch));
    });
    return value;
}

bool contains_case_insensitive(const std::wstring& value, const std::wstring& token) {
    return to_upper(value).find(to_upper(token)) != std::wstring::npos;
}

bool has_argument(int argc, wchar_t* argv[], const std::wstring& expected) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == expected) {
            return true;
        }
    }
    return false;
}

int finish(int exit_code, bool pause_before_exit) {
    if (pause_before_exit) {
        std::wcout << L"\nPress Enter to close this window..." << std::flush;
        std::wstring ignored;
        std::getline(std::wcin, ignored);
    }
    return exit_code;
}

std::optional<std::wstring> get_registry_string(
    HDEVINFO device_info_set,
    SP_DEVINFO_DATA& device_info_data,
    DWORD property) {

    DWORD data_type = 0;
    DWORD required_size = 0;

    SetupDiGetDeviceRegistryPropertyW(
        device_info_set,
        &device_info_data,
        property,
        &data_type,
        nullptr,
        0,
        &required_size);

    if (required_size == 0) {
        return std::nullopt;
    }

    std::vector<BYTE> buffer(required_size + sizeof(wchar_t), 0);
    if (!SetupDiGetDeviceRegistryPropertyW(
            device_info_set,
            &device_info_data,
            property,
            &data_type,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            nullptr)) {
        return std::nullopt;
    }

    if (data_type != REG_SZ && data_type != REG_EXPAND_SZ) {
        return std::nullopt;
    }

    return std::wstring(reinterpret_cast<const wchar_t*>(buffer.data()));
}

std::optional<std::wstring> get_instance_id(
    HDEVINFO device_info_set,
    SP_DEVINFO_DATA& device_info_data) {

    DWORD required_size = 0;
    SetupDiGetDeviceInstanceIdW(
        device_info_set,
        &device_info_data,
        nullptr,
        0,
        &required_size);

    if (required_size == 0) {
        return std::nullopt;
    }

    std::vector<wchar_t> buffer(required_size + 1, L'\0');
    if (!SetupDiGetDeviceInstanceIdW(
            device_info_set,
            &device_info_data,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            nullptr)) {
        return std::nullopt;
    }

    return std::wstring(buffer.data());
}

std::vector<DeviceInfo> find_target_devices() {
    std::vector<DeviceInfo> targets;

    HDEVINFO device_info_set = SetupDiGetClassDevsW(
        nullptr,
        nullptr,
        nullptr,
        DIGCF_ALLCLASSES | DIGCF_PRESENT);

    if (device_info_set == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Windows device enumeration failed. Error=" << GetLastError() << L"\n";
        return targets;
    }

    SP_DEVINFO_DATA device_info_data{};
    device_info_data.cbSize = sizeof(device_info_data);

    for (DWORD index = 0;; ++index) {
        if (!SetupDiEnumDeviceInfo(device_info_set, index, &device_info_data)) {
            if (GetLastError() != ERROR_NO_MORE_ITEMS) {
                std::wcerr << L"Device enumeration stopped unexpectedly. Error=" << GetLastError() << L"\n";
            }
            break;
        }

        const auto instance_id = get_instance_id(device_info_set, device_info_data);
        if (!instance_id.has_value()) {
            continue;
        }

        if (to_upper(*instance_id).find(kTargetVidPid) == std::wstring::npos) {
            continue;
        }

        DeviceInfo device;
        device.instance_id = *instance_id;
        device.description = get_registry_string(device_info_set, device_info_data, SPDRP_DEVICEDESC).value_or(L"");
        device.friendly_name = get_registry_string(device_info_set, device_info_data, SPDRP_FRIENDLYNAME).value_or(L"");
        device.manufacturer = get_registry_string(device_info_set, device_info_data, SPDRP_MFG).value_or(L"");
        device.service = get_registry_string(device_info_set, device_info_data, SPDRP_SERVICE).value_or(L"");
        device.device_class = get_registry_string(device_info_set, device_info_data, SPDRP_CLASS).value_or(L"");
        targets.push_back(std::move(device));
    }

    SetupDiDestroyDeviceInfoList(device_info_set);
    return targets;
}

std::wstring display_name(const DeviceInfo& device) {
    if (!device.friendly_name.empty()) {
        return device.friendly_name;
    }
    if (!device.description.empty()) {
        return device.description;
    }
    return L"(unknown)";
}

void print_device(const DeviceInfo& device) {
    std::wcout << L"Detected target DFU device\n";
    std::wcout << L"  Name         : " << display_name(device) << L"\n";
    std::wcout << L"  Description  : " << (device.description.empty() ? L"(unknown)" : device.description) << L"\n";
    std::wcout << L"  Manufacturer : " << (device.manufacturer.empty() ? L"(unknown)" : device.manufacturer) << L"\n";
    std::wcout << L"  Class        : " << (device.device_class.empty() ? L"(unknown)" : device.device_class) << L"\n";
    std::wcout << L"  Service      : " << (device.service.empty() ? L"(none/driverless)" : device.service) << L"\n";
    std::wcout << L"  Instance ID  : " << device.instance_id << L"\n";
}

bool metadata_matches_expected_target(const DeviceInfo& device) {
    if (!contains_case_insensitive(device.instance_id, kTargetVidPid)) {
        return false;
    }

    const bool expected_name =
        contains_case_insensitive(device.description, kExpectedDescription) ||
        contains_case_insensitive(device.friendly_name, kExpectedDescription);
    if (!expected_name) {
        return false;
    }

    // A driverless device may not expose a useful SPDRP_MFG value. If Windows
    // does provide one, require it to identify LEGO rather than trusting it blindly.
    if (!device.manufacturer.empty() &&
        !contains_case_insensitive(device.manufacturer, kExpectedManufacturerToken)) {
        return false;
    }

    return true;
}

bool is_winusb(const DeviceInfo& device) {
    return to_upper(device.service) == L"WINUSB";
}

bool confirm_install(const DeviceInfo& device) {
    std::wcout << L"\nWinUSB is not currently assigned to this DFU Hub.\n";
    std::wcout << L"Only this device will be changed:\n";
    std::wcout << L"  " << display_name(device) << L"\n";
    std::wcout << L"  " << device.instance_id << L"\n";
    std::wcout << L"Current service: "
               << (device.service.empty() ? L"(none/driverless)" : device.service)
               << L"\n";
    std::wcout << L"\nType INSTALL and press Enter to assign WinUSB.\n";
    std::wcout << L"> " << std::flush;

    std::wstring answer;
    std::getline(std::wcin, answer);
    return to_upper(answer) == L"INSTALL";
}

std::string wide_to_utf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int required = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.c_str(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required <= 0) {
        return {};
    }

    std::string result(static_cast<size_t>(required), '\0');
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value.c_str(),
            static_cast<int>(value.size()),
            result.data(),
            required,
            nullptr,
            nullptr) != required) {
        return {};
    }
    return result;
}

struct WdiDeviceList {
    wdi_device_info* head = nullptr;

    ~WdiDeviceList() {
        if (head != nullptr) {
            wdi_destroy_list(head);
        }
    }
};

std::optional<wdi_device_info*> find_exact_wdi_target(WdiDeviceList& holder) {
    wdi_options_create_list options{};
    options.list_all = TRUE;
    options.list_hubs = FALSE;
    options.trim_whitespaces = TRUE;

    const int result = wdi_create_list(&holder.head, &options);
    if (result != WDI_SUCCESS) {
        std::cerr << "libwdi device enumeration failed: " << wdi_strerror(result) << "\n";
        return std::nullopt;
    }

    std::vector<wdi_device_info*> matches;
    for (wdi_device_info* device = holder.head; device != nullptr; device = device->next) {
        if (device->vid == kTargetVid && device->pid == kTargetPid) {
            matches.push_back(device);
        }
    }

    if (matches.size() != 1) {
        std::wcerr << L"Safety stop: libwdi independently found " << matches.size()
                   << L" matching 0694:0008 USB entries; expected exactly one.\n";
        return std::nullopt;
    }

    return matches.front();
}

std::filesystem::path make_driver_temp_directory() {
    return std::filesystem::temp_directory_path() /
        (L"spike-rt-dfu-winusb-setup-" + std::to_wstring(GetCurrentProcessId()));
}

bool wait_for_winusb_verification(DeviceInfo& verified_device) {
    for (int attempt = 0; attempt < 20; ++attempt) {
        const auto targets = find_target_devices();
        if (targets.size() == 1 && metadata_matches_expected_target(targets.front())) {
            if (is_winusb(targets.front())) {
                verified_device = targets.front();
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return false;
}

int install_winusb() {
    if (!wdi_is_driver_supported(WDI_WINUSB, nullptr)) {
        std::wcerr << L"This libwdi build does not contain WinUSB support.\n";
        return 7;
    }

    WdiDeviceList wdi_devices;
    const auto target = find_exact_wdi_target(wdi_devices);
    if (!target.has_value()) {
        return 8;
    }

    std::error_code fs_error;
    const std::filesystem::path driver_dir = make_driver_temp_directory();
    std::filesystem::remove_all(driver_dir, fs_error);
    fs_error.clear();
    std::filesystem::create_directories(driver_dir, fs_error);
    if (fs_error) {
        std::wcerr << L"Could not create temporary driver directory: "
                   << driver_dir.wstring() << L"\n";
        return 9;
    }

    const std::string driver_dir_utf8 = wide_to_utf8(driver_dir.wstring());
    if (driver_dir_utf8.empty()) {
        std::wcerr << L"Could not convert the temporary driver path to UTF-8.\n";
        std::filesystem::remove_all(driver_dir, fs_error);
        return 9;
    }

    wdi_set_log_level(WDI_LOG_LEVEL_WARNING);

    wdi_options_prepare_driver prepare{};
    prepare.driver_type = WDI_WINUSB;
    prepare.vendor_name = const_cast<char*>("Lego Group");
    prepare.disable_cat = FALSE;
    prepare.disable_signing = FALSE;
    prepare.use_wcid_driver = FALSE;
    prepare.external_inf = FALSE;

    std::wcout << L"\nPreparing a signed WinUSB driver package...\n";
    int result = wdi_prepare_driver(
        *target,
        driver_dir_utf8.c_str(),
        kInfName,
        &prepare);
    if (result != WDI_SUCCESS) {
        std::cerr << "libwdi prepare failed: " << wdi_strerror(result) << "\n";
        std::filesystem::remove_all(driver_dir, fs_error);
        return 10;
    }

    wdi_options_install_driver install{};
    install.hWnd = GetConsoleWindow();
    install.install_filter_driver = FALSE;
    install.pending_install_timeout = 30000;

    std::wcout << L"Installing WinUSB. Windows may show a UAC/driver confirmation dialog...\n";
    result = wdi_install_driver(
        *target,
        driver_dir_utf8.c_str(),
        kInfName,
        &install);

    if (result != WDI_SUCCESS) {
        std::cerr << "libwdi install failed: " << wdi_strerror(result) << "\n";
        std::filesystem::remove_all(driver_dir, fs_error);
        return 11;
    }

    DeviceInfo verified;
    const bool verified_ok = wait_for_winusb_verification(verified);
    std::filesystem::remove_all(driver_dir, fs_error);

    if (!verified_ok) {
        std::wcerr << L"Installation reported success, but Service=WinUSB could not be verified.\n";
        std::wcerr << L"Reconnect the Hub in DFU mode and run the tool again before using WebUSB.\n";
        return 12;
    }

    std::wcout << L"\nWinUSB setup completed and was verified.\n";
    std::wcout << L"Verified service: " << verified.service << L"\n";
    return 0;
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
    const bool pause_before_exit = !has_argument(argc, argv, L"--no-pause");
    const bool detect_only = has_argument(argc, argv, L"--detect-only");

    std::wcout << L"SPIKE-RT DFU WinUSB Setup v0.2\n";
    std::wcout << L"Target: LEGO Technic Large Hub in DFU Mode / USB 0694:0008\n";
    std::wcout << L"This tool never selects an arbitrary USB device.\n\n";

    const auto targets = find_target_devices();

    if (targets.empty()) {
        std::wcout << L"No target DFU Hub was found.\n";
        std::wcout << L"Put the SPIKE Prime Hub into DFU mode, connect it by USB, and run this tool again.\n";
        return finish(2, pause_before_exit);
    }

    if (targets.size() > 1) {
        std::wcerr << L"Safety stop: " << targets.size()
                   << L" devices matching VID 0694 / PID 0008 are connected.\n";
        std::wcerr << L"Disconnect extra devices and leave exactly one target Hub connected.\n";
        return finish(3, pause_before_exit);
    }

    const DeviceInfo& target = targets.front();
    print_device(target);

    if (!metadata_matches_expected_target(target)) {
        std::wcerr << L"\nSafety stop: device metadata does not match the expected SPIKE Prime DFU Hub.\n";
        std::wcerr << L"No driver changes were made.\n";
        return finish(4, pause_before_exit);
    }

    if (is_winusb(target)) {
        std::wcout << L"\nStatus: WinUSB is already assigned. No changes are needed.\n";
        return finish(0, pause_before_exit);
    }

    if (detect_only) {
        std::wcout << L"\nDetect-only mode: WinUSB is not assigned, but no changes were requested.\n";
        return finish(0, pause_before_exit);
    }

    if (!confirm_install(target)) {
        std::wcout << L"\nCancelled. No driver changes were made.\n";
        return finish(5, pause_before_exit);
    }

    const int install_result = install_winusb();
    return finish(install_result, pause_before_exit);
}
