#include <windows.h>
#include <setupapi.h>

#include <algorithm>
#include <cwctype>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr wchar_t kTargetVidPid[] = L"VID_0694&PID_0008";

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

void print_device(const DeviceInfo& device) {
    const std::wstring display_name = !device.friendly_name.empty()
        ? device.friendly_name
        : (!device.description.empty() ? device.description : L"(unknown)");

    std::wcout << L"Detected target DFU device\n";
    std::wcout << L"  Name         : " << display_name << L"\n";
    std::wcout << L"  Description  : " << (device.description.empty() ? L"(unknown)" : device.description) << L"\n";
    std::wcout << L"  Manufacturer : " << (device.manufacturer.empty() ? L"(unknown)" : device.manufacturer) << L"\n";
    std::wcout << L"  Class        : " << (device.device_class.empty() ? L"(unknown)" : device.device_class) << L"\n";
    std::wcout << L"  Service      : " << (device.service.empty() ? L"(none/unknown)" : device.service) << L"\n";
    std::wcout << L"  Instance ID  : " << device.instance_id << L"\n";

    if (to_upper(device.service) == L"WINUSB") {
        std::wcout << L"\nStatus: WinUSB is already assigned to this device.\n";
    } else {
        std::wcout << L"\nStatus: WinUSB is not currently reported as the device service.\n";
    }
}

}  // namespace

int wmain() {
    std::wcout << L"SPIKE-RT DFU WinUSB Setup - detector v0.1\n";
    std::wcout << L"Target: USB VID 0694 / PID 0008\n";
    std::wcout << L"This version is read-only and does not change any driver.\n\n";

    const auto targets = find_target_devices();

    if (targets.empty()) {
        std::wcout << L"No target DFU Hub was found.\n";
        std::wcout << L"Put the SPIKE Prime Hub into DFU mode, connect it by USB, and run this tool again.\n";
        return 2;
    }

    if (targets.size() > 1) {
        std::wcerr << L"Safety stop: " << targets.size()
                   << L" devices matching VID 0694 / PID 0008 are connected.\n";
        std::wcerr << L"Disconnect extra devices and leave exactly one target Hub connected.\n";
        return 3;
    }

    print_device(targets.front());
    std::wcout << L"\nDetector completed. No system changes were made.\n";
    return 0;
}
