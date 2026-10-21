cmake_minimum_required(VERSION 3.16)
project(ProCreateStabiliserExample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Concurrent)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/canvasview.cpp
    src/canvasview.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Widgets Qt5::Concurrent)
