if(NOT WIN32)
  message(FATAL_ERROR "windows-msvc.cmake is for Windows only.")
endif()

set(CMAKE_SYSTEM_NAME Windows)
