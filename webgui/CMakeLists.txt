cmake_minimum_required(VERSION 3.20)
project(webgui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_executable(JoyCon2Mapper
  src/main.cpp
  src/config_json.cpp
  src/control_api.cpp
  src/config_store.cpp
  src/key_capture_win.cpp
  src/mapper_runtime.cpp
  src/sendinput_output_sink.cpp
)

target_include_directories(JoyCon2Mapper
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/transport/include
)

target_link_libraries(JoyCon2Mapper
  PRIVATE
    joycon_transport
    windowsapp
    ws2_32
    shell32
    user32
)

if(MSVC)
  target_compile_options(JoyCon2Mapper PRIVATE /W4 /permissive-)
else()
  target_compile_options(JoyCon2Mapper PRIVATE -Wall -Wextra -pedantic)
endif()

add_custom_command(TARGET JoyCon2Mapper POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:JoyCon2Mapper>/web"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/web" "$<TARGET_FILE_DIR:JoyCon2Mapper>/web"
)
