cmake_minimum_required(VERSION 3.20)

project(spike_rt_dfu_winusb_setup VERSION 0.1.0 LANGUAGES CXX)

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

if(MSVC)
    target_compile_options(spike-rt-dfu-winusb-setup PRIVATE /W4 /permissive- /utf-8)
endif()

target_link_libraries(spike-rt-dfu-winusb-setup PRIVATE setupapi)
