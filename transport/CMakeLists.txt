cmake_minimum_required(VERSION 3.20)
project(transport LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_library(joycon_transport
  src/NintendoControllerProtocol.cpp
  src/NintendoControllerTransport.cpp
)

add_library(joycon::transport ALIAS joycon_transport)

target_include_directories(joycon_transport
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(joycon_transport
  PUBLIC
    windowsapp
)

if(MSVC)
  target_compile_options(joycon_transport PRIVATE /W4 /permissive-)
else()
  target_compile_options(joycon_transport PRIVATE -Wall -Wextra -pedantic)
endif()
