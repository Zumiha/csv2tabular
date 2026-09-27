cmake_minimum_required(VERSION 3.20.0)
project(csvToXLTabular VERSION 0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/libdir)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/archdir)

set(PROJECT_SOURCES
    main.cpp
)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/csvparser)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/iniparser)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libs/utils)

add_library(csv2xltabular STATIC
    ConversionSettings.cpp
    ConversionSettings.h
    csv2xltabular.cpp
    csv2xltabular.h
)

target_include_directories(csv2xltabular PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(${PROJECT_NAME} main.cpp)

set_target_properties(Utils PROPERTIES LINKER_LANGUAGE CXX)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/csvparser
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/iniparser
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/utils
)
target_link_libraries(csv2xltabular PUBLIC csvParser IniParser Utils)
target_link_libraries(${PROJECT_NAME} PRIVATE csv2xltabular)