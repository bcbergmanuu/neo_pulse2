# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(neopulse2 C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(neopulse2 
    ad7124_console_app.c    
    ad7124_support.c
    ad7124.c      
    lcd1602a.c
)

pico_set_program_name(neopulse2 "neopulse2")
pico_set_program_version(neopulse2 "0.1")

# Pull in FreeRTOS
include($ENV{FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)


# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(neopulse2 1)
pico_enable_stdio_usb(neopulse2 0)

# Add the standard library to the build
target_link_libraries(neopulse2 PRIVATE
    pico_stdlib       
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4    
    hardware_spi
    
)

# Add the standard include files to the build
target_include_directories(neopulse2  PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(neopulse2)

