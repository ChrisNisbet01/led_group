cmake_minimum_required(VERSION 3.10)

project(led_group VERSION 1.0.0)

include (CheckFunctionExists)

check_function_exists(strlcpy HAVE_STRLCPY)

configure_file ("${PROJECT_SOURCE_DIR}/config.h.in"
                "${PROJECT_BINARY_DIR}/config.h" )
include_directories(${PROJECT_BINARY_DIR})

option(WITH_LOGGING "<include LED update output>")

set(CMAKE_C_COMPILER "gcc")

add_compile_options(
  -O3 
  -Wall 
  -Wextra 
  -g 
  -D_GNU_SOURCE 
  -DWITH_LOGGING=${WITH_LOGGING}
)

add_executable(led_group led_group.c)

install(TARGETS led_group DESTINATION bin)

