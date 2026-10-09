
cmake_minimum_required(VERSION 3.16)

project(SFML_First_Project LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML 3 REQUIRED COMPONENTS Graphics)

add_executable(SFML_First_Project main.cpp)

target_link_libraries(
    SFML_First_Project
    PRIVATE
    SFML::Graphics
    comdlg32
)
