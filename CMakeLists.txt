cmake_minimum_required(VERSION 3.20)
project(Mellzi VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDEs / clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
include(CTest)

# Platform-specific socket & thread libraries
if(WIN32)
    set(PLATFORM_LIBS ws2_32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    set(PLATFORM_LIBS pthread)
endif()

# Compiler warnings and options
if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:preprocessor /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# Core library sources
set(MELLZI_SOURCES
    src/protocol.cpp
    src/config.cpp
    src/socket.cpp
    src/platform.cpp
    src/image_process.cpp
    src/calibration.cpp
    src/acquisition.cpp
    src/client.cpp
    src/c_api.cpp
    src/server.cpp
)

# 1. Static Library
add_library(mellzi_static STATIC ${MELLZI_SOURCES})
target_include_directories(mellzi_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(mellzi_static PUBLIC ${PLATFORM_LIBS})

# 2. Shared Library (100% Rayence VADAV ABI drop-in replacement)
add_library(mellzi SHARED ${MELLZI_SOURCES})
target_include_directories(mellzi PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_definitions(mellzi PRIVATE MELLZI_BUILDING_DLL)
target_link_libraries(mellzi PUBLIC ${PLATFORM_LIBS})

# 3. Server Daemon Executable (for NVIDIA Jetson and any platform)
add_executable(mellzi_server apps/server/main.cpp)
target_link_libraries(mellzi_server PRIVATE mellzi_static)

# 4. CLI Tool Executable
add_executable(mellzi_cli apps/cli/main.cpp)
target_link_libraries(mellzi_cli PRIVATE mellzi_static)

# 5. Demo Executable
add_executable(mellzi_demo apps/demo/main.cpp)
target_link_libraries(mellzi_demo PRIVATE mellzi_static)

# 6. Unit & Integration Tests
if(BUILD_TESTING)
    enable_testing()

    add_executable(test_protocol tests/test_protocol.cpp)
    target_link_libraries(test_protocol PRIVATE mellzi_static)
    add_test(NAME ProtocolTest COMMAND test_protocol)

    add_executable(test_config tests/test_config.cpp)
    target_link_libraries(test_config PRIVATE mellzi_static)
    add_test(NAME ConfigTest COMMAND test_config)

    add_executable(test_calibration tests/test_calibration.cpp)
    target_link_libraries(test_calibration PRIVATE mellzi_static)
    add_test(NAME CalibrationTest COMMAND test_calibration)

    add_executable(test_network tests/test_network.cpp)
    target_link_libraries(test_network PRIVATE mellzi_static)
    add_test(NAME NetworkTest COMMAND test_network)
endif()

# Installation
install(TARGETS mellzi mellzi_static mellzi_server mellzi_cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/mellzi
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY config/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/mellzi/config
)

install(FILES systemd/mellzi-server.service
    DESTINATION ${CMAKE_INSTALL_DATADIR}/mellzi/systemd
)

install(DIRECTORY docs/
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/mellzi
)

install(FILES README.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/mellzi
)

