cmake_minimum_required(VERSION 3.20)
project(fabclm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(include)

set(SOURCES
    src/main.cpp
    src/config.cpp
    src/http_client.cpp
    src/auth.cpp
    src/fabric_client.cpp
    src/azure_monitor_client.cpp
    src/terminal_ui.cpp
    src/dax_client.cpp
    src/cli.cpp
)

add_executable(fabclm ${SOURCES})

if (WIN32)
    target_link_libraries(fabclm PRIVATE winhttp)
endif()
