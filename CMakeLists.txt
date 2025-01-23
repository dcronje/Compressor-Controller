cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()

set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/cjson)
add_library(cjson lib/cjson/cJSON.c)

# Include FreeRTOS Kernel if needed
include($ENV{FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

project(compressor-controller C CXX ASM)

pico_sdk_init()

add_executable(compressor-controller 
    main.cpp
    src/wifi.cpp
    src/settings.cpp
    src/dhcpserver.c
    src/httpserver.cpp
    src/control.cpp
    src/sensors.cpp
    src/ws2812.pio
)

pico_generate_pio_header(compressor-controller ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio
    OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/include
)

pico_set_program_name(compressor-controller "compressor-controller")
pico_set_program_version(compressor-controller "0.1")

pico_enable_stdio_uart(compressor-controller 1)
pico_enable_stdio_usb(compressor-controller 0)

target_include_directories(compressor-controller PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(compressor-controller
    pico_stdlib
    pico_cyw43_arch_lwip_sys_freertos
    pico_lwip_http
    pico_lwip_mdns
    hardware_adc
    hardware_pio
    FreeRTOS-Kernel-Heap4
    cjson
)

# Add static content directory (if needed later)
# pico_generate_freertos_httpd_content(
#     compressor-controller
#     ${CMAKE_CURRENT_LIST_DIR}/content
# )

pico_add_extra_outputs(compressor-controller)
