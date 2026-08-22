cmake_minimum_required(VERSION 3.20)

project(spike_rt_dfu_winusb_setup VERSION 0.2.0 LANGUAGES CXX)

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

if(NOT EXISTS "${LIBWDI_IMPORT_LIB}" OR NOT EXISTS "${LIBWDI_DLL}")
    message(FATAL_ERROR "Built libwdi x64 Release DLL/import library not found under ${LIBWDI_DLL_DIR}")
endif()

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

add_custom_command(TARGET spike-rt-dfu-winusb-setup POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LIBWDI_DLL}"
        "$<TARGET_FILE_DIR:spike-rt-dfu-winusb-setup>/libwdi.dll"
    VERBATIM
)
