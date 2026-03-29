cmake_minimum_required(VERSION 3.16)
project(mini_beam_esp32 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(MB_HAL_BACKEND "host" CACHE STRING "HAL backend: host, espidf, or nrf52")
set_property(CACHE MB_HAL_BACKEND PROPERTY STRINGS host espidf nrf52)

if(MB_HAL_BACKEND STREQUAL "espidf")
  set(MB_HAL_SRC src/mb_hal_espidf.c)
  add_compile_definitions(MB_USE_ESP_IDF)
elseif(MB_HAL_BACKEND STREQUAL "nrf52")
  set(MB_HAL_SRC src/mb_hal_nrf52.c)
  add_compile_definitions(MB_USE_NRF52)
else()
  set(MB_HAL_SRC src/mb_hal_stub.c)
endif()

set(MB_CORE_SRCS src/mb_vm.c src/mb_scheduler.c src/mb_heap.c)

add_executable(mini_beam_host
  src/main_host.c
  ${MB_CORE_SRCS}
  ${MB_HAL_SRC}
)

target_include_directories(mini_beam_host PRIVATE include)

target_compile_options(mini_beam_host PRIVATE -Wall -Wextra -Werror)

add_executable(mini_beam_host_mailbox
  src/main_mailbox_host.c
  ${MB_CORE_SRCS}
  ${MB_HAL_SRC}
)

target_include_directories(mini_beam_host_mailbox PRIVATE include)
target_compile_options(mini_beam_host_mailbox PRIVATE -Wall -Wextra -Werror)

add_executable(mini_beam_host_regression
  src/main_regression_host.c
  ${MB_CORE_SRCS}
  src/mb_hal_stub.c
)

target_include_directories(mini_beam_host_regression PRIVATE include)
target_compile_options(mini_beam_host_regression PRIVATE -Wall -Wextra -Werror)

add_executable(mini_beam_host_multiproc
  src/main_multiproc_host.c
  ${MB_CORE_SRCS}
  src/mb_hal_stub.c
)

target_include_directories(mini_beam_host_multiproc PRIVATE include)
target_compile_options(mini_beam_host_multiproc PRIVATE -Wall -Wextra -Werror)

add_executable(mini_beam_host_stability
  src/main_stability_host.c
  ${MB_CORE_SRCS}
  src/mb_hal_stub.c
)

target_include_directories(mini_beam_host_stability PRIVATE include)
target_compile_options(mini_beam_host_stability PRIVATE -Wall -Wextra -Werror)

add_executable(mini_beam_host_flow
  src/main_flow_host.c
  ${MB_CORE_SRCS}
  src/mb_hal_stub.c
)

target_include_directories(mini_beam_host_flow PRIVATE include /tmp)
target_compile_options(mini_beam_host_flow PRIVATE -Wall -Wextra -Werror)
