cmake_minimum_required(VERSION 3.20)

project(spike_rt_dfu_winusb_setup VERSION 0.3.0 LANGUAGES CXX RC)

set(LIBWDI_ROOT "" CACHE PATH "Path to a built libwdi source tree")

if(NOT WIN32)
    message(FATAL_ERROR "spike-rt-dfu-winusb-setup is Windows-only")
endif()

if(NOT LIBWDI_ROOT)
    message(FATAL_ERROR "LIBWDI_ROOT must point to the pinned, built libwdi source tree")
endif()

set(LIBWDI_INCLUDE_DIR "${LIBWDI_ROOT}/libwdi")
set(LIBWDI_DLL_DIR "${LIBWDI_ROOT}/x64/Release/dll")
set(LIBWDI_IMPORT_LIB "${LIBWDI_DLL_DIR}/libwdi.lib")
set(LIBWDI_DLL "${LIBWDI_DLL_DIR}/libwdi.dll")
set(APP_ICON_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/spike_rt_winusb_setup.ico")

if(NOT EXISTS "${LIBWDI_IMPORT_LIB}" OR NOT EXISTS "${LIBWDI_DLL}")
    message(FATAL_ERROR "Built libwdi x64 Release DLL/import library not found under ${LIBWDI_DLL_DIR}")
endif()

if(NOT EXISTS "${APP_ICON_PATH}")
    message(FATAL_ERROR "Application icon not found: ${APP_ICON_PATH}")
endif()

set(EMBEDDED_RUNTIME_DIR "${CMAKE_BINARY_DIR}/embedded-runtime")

function(set_flat_runtime_output target output_dir)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${output_dir}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${output_dir}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${output_dir}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${output_dir}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${output_dir}"
    )
endfunction()

# Existing, hardware-validated command-line implementation. In v0.3 this is
# embedded as a private worker and is not the student-facing entry point.
add_executable(spike-rt-dfu-winusb-setup
    src/main.cpp
)

target_compile_features(spike-rt-dfu-winusb-setup PRIVATE cxx_std_20)

target_compile_definitions(spike-rt-dfu-winusb-setup PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_include_directories(spike-rt-dfu-winusb-setup PRIVATE
    "${LIBWDI_INCLUDE_DIR}"
)

if(MSVC)
    target_compile_options(spike-rt-dfu-winusb-setup PRIVATE /W4 /permissive- /utf-8)
    set_property(TARGET spike-rt-dfu-winusb-setup PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

target_link_libraries(spike-rt-dfu-winusb-setup PRIVATE
    setupapi
    newdev
    ntdll
    "${LIBWDI_IMPORT_LIB}"
)

set_target_properties(spike-rt-dfu-winusb-setup PROPERTIES
    OUTPUT_NAME "spike-rt-dfu-winusb-worker"
)
set_flat_runtime_output(spike-rt-dfu-winusb-setup "${EMBEDDED_RUNTIME_DIR}")

add_custom_command(TARGET spike-rt-dfu-winusb-setup POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LIBWDI_DLL}"
        "$<TARGET_FILE_DIR:spike-rt-dfu-winusb-setup>/libwdi.dll"
    VERBATIM
)

# Native Win32 GUI. It deliberately delegates all detection and installation
# policy to the validated worker above instead of duplicating USB-driver logic.
add_executable(spike-rt-dfu-winusb-setup-gui WIN32
    src/gui_main.cpp
)

target_compile_features(spike-rt-dfu-winusb-setup-gui PRIVATE cxx_std_20)

target_compile_definitions(spike-rt-dfu-winusb-setup-gui PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(spike-rt-dfu-winusb-setup-gui PRIVATE /W4 /permissive- /utf-8)
    set_property(TARGET spike-rt-dfu-winusb-setup-gui PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_link_libraries(spike-rt-dfu-winusb-setup-gui PRIVATE user32)

set_target_properties(spike-rt-dfu-winusb-setup-gui PROPERTIES
    OUTPUT_NAME "SPIKE-RT-DFU-WinUSB-Setup-inner"
)
set_flat_runtime_output(spike-rt-dfu-winusb-setup-gui "${EMBEDDED_RUNTIME_DIR}")

# The student-facing executable is a one-file bootstrapper. The already-tested
# GUI, worker and libwdi DLL are stored as RCDATA resources. At runtime they are
# expanded to a private temporary directory and the GUI is launched from there.
set(EMBEDDED_GUI_EXE "${EMBEDDED_RUNTIME_DIR}/SPIKE-RT-DFU-WinUSB-Setup-inner.exe")
set(EMBEDDED_WORKER_EXE "${EMBEDDED_RUNTIME_DIR}/spike-rt-dfu-winusb-worker.exe")
set(EMBEDDED_LIBWDI_DLL "${EMBEDDED_RUNTIME_DIR}/libwdi.dll")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/embedded_payload.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/embedded_payload.rc"
    @ONLY
)

set_source_files_properties(
    "${CMAKE_CURRENT_BINARY_DIR}/embedded_payload.rc"
    PROPERTIES
        OBJECT_DEPENDS "${EMBEDDED_GUI_EXE};${EMBEDDED_WORKER_EXE};${EMBEDDED_LIBWDI_DLL};${APP_ICON_PATH}"
)

add_executable(spike-rt-dfu-winusb-setup-bundle WIN32
    src/bootstrap_main.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/embedded_payload.rc"
)
add_dependencies(
    spike-rt-dfu-winusb-setup-bundle
    spike-rt-dfu-winusb-setup
    spike-rt-dfu-winusb-setup-gui
)

target_compile_features(spike-rt-dfu-winusb-setup-bundle PRIVATE cxx_std_20)

target_compile_definitions(spike-rt-dfu-winusb-setup-bundle PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(spike-rt-dfu-winusb-setup-bundle PRIVATE /W4 /permissive- /utf-8)
    set_property(TARGET spike-rt-dfu-winusb-setup-bundle PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_link_libraries(spike-rt-dfu-winusb-setup-bundle PRIVATE user32)

set_target_properties(spike-rt-dfu-winusb-setup-bundle PROPERTIES
    OUTPUT_NAME "SPIKE-RT-DFU-WinUSB-Setup-v0.3"
)
