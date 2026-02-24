cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mini_beam_nrf52)

target_sources(app PRIVATE
  src/main.c
  ../src/mb_vm.c
  ../src/mb_hal_nrf52.c
)

target_include_directories(app PRIVATE ../include)

target_compile_definitions(app PRIVATE MB_USE_NRF52)
target_compile_definitions(app PRIVATE MB_STRICT_DTS)

if(DEFINED OS2_FAULT_EVERY_N)
  target_compile_definitions(app PRIVATE OS2_FAULT_EVERY_N=${OS2_FAULT_EVERY_N})
  message(STATUS "OS2_FAULT_EVERY_N=${OS2_FAULT_EVERY_N}")
endif()
