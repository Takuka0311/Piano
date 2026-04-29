cmake_minimum_required(VERSION 3.21)

project(PianoRebuild VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "This project currently supports Windows only.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(piano_cli
  src/app/main.cpp
  src/app/application.cpp
  src/input/keyboard_map.cpp
  src/score/score_parser.cpp
  src/engine/score_scheduler.cpp
  src/audio/log_output_sink.cpp
)

target_include_directories(piano_cli
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(piano_cli
  PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)
