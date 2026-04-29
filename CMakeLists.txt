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
  src/app/audio_backend_factory.cpp
  src/app/playback_service.cpp
  src/platform/config_store.cpp
  src/input/keyboard_map.cpp
  src/score/score_parser.cpp
  src/engine/score_scheduler.cpp
  src/export/wav_exporter.cpp
  src/audio/log_output_sink.cpp
  src/audio/wasapi_output_sink.cpp
  src/audio/dsound_output_sink.cpp
  src/audio/midi_output_sink.cpp
  src/audio/vsti_output_sink.cpp
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

target_link_libraries(piano_cli
  PRIVATE
    ole32
    avrt
    uuid
    dsound
    dxguid
    winmm
)

add_executable(piano_gui WIN32
  src/ui/gui_main.cpp
  src/ui/gui_app.cpp
  src/app/audio_backend_factory.cpp
  src/platform/config_store.cpp
  src/app/playback_service.cpp
  src/input/keyboard_map.cpp
  src/score/score_parser.cpp
  src/engine/score_scheduler.cpp
  src/export/wav_exporter.cpp
  src/audio/log_output_sink.cpp
  src/audio/wasapi_output_sink.cpp
  src/audio/dsound_output_sink.cpp
  src/audio/midi_output_sink.cpp
  src/audio/vsti_output_sink.cpp
)

target_include_directories(piano_gui
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(piano_gui
  PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

target_link_libraries(piano_gui
  PRIVATE
    ole32
    avrt
    uuid
    comdlg32
    comctl32
    dsound
    dxguid
    winmm
)

enable_testing()

add_executable(piano_tests
  tests/test_main.cpp
  src/platform/config_store.cpp
  src/input/keyboard_map.cpp
  src/score/score_parser.cpp
  src/engine/score_scheduler.cpp
  src/export/wav_exporter.cpp
)

target_include_directories(piano_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(piano_tests
  PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

add_test(NAME piano_tests COMMAND piano_tests)
