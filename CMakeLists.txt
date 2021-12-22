cmake_minimum_required(VERSION 3.12)

# Pull in PICO SDK (must be before project)
include(pico_sdk_import.cmake)

# We also need PICO EXTRAS
#include(pico_extras_import.cmake)

project(ledstrip C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

add_subdirectory(FatFs_SPI build)

include_directories(config)
include_directories(ws2812)
include_directories(tft_t)


set(WS2812_SOURCES 
		ws2812/WS2812.cpp
	)


set(PICOSTRIP_SOURCES 
		picostrip/picostrip.cpp 
		picostrip/filesys.c 
		picostrip/keyapi.c 
		picostrip/gifdec.c 
	)

set(TFT_T_SOURCES 
		tft_t/tft_t_dma.cpp
	)
	
add_executable(ledstrip
		${WS2812_SOURCES}
		${PICOSTRIP_SOURCES}
		${TFT_T_SOURCES}
	)

pico_generate_pio_header(ledstrip ${CMAKE_CURRENT_LIST_DIR}/ws2812/WS2812.pio)


target_link_libraries(ledstrip pico_multicore
	    pico_stdlib
	    FatFs_SPI
	    hardware_adc
	    hardware_pio
        hardware_pwm	    
	)


#target_compile_options(ledstrip PUBLIC -O3)
#target_compile_options(ledstrip PUBLIC -Wall -Wextra -Wno-unused-function -Wno-unused-parameter)
IF (NOT DEFINED N_SD_CARDS)
    SET(N_SD_CARDS 1)
ENDIF()
target_compile_definitions(ledstrip PUBLIC N_SD_CARDS=${N_SD_CARDS})


#pico_enable_stdio_uart(ledstrip 0)
#pico_enable_stdio_usb(ledstrip 0)

pico_add_extra_outputs(ledstrip)
