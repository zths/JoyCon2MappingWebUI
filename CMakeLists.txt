cmake_minimum_required(VERSION 3.20)
project(joycon2cpp LANGUAGES CXX)

option(JOYCON2CPP_BUILD_TRANSPORT "Build the transport/protocol sample" ON)
option(JOYCON2CPP_BUILD_WEBGUI "Build the local Web GUI" ON)


if(JOYCON2CPP_BUILD_TRANSPORT)
  add_subdirectory(transport)
endif()

if(JOYCON2CPP_BUILD_WEBGUI)
  add_subdirectory(webgui)
endif()
