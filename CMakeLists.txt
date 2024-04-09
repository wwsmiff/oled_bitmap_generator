cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
project(oled-bitmap-generator)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(${PROJECT_NAME})

find_package(FLTK)
find_package(X11)
find_package(OpenGL)
find_package(GLEW)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)
if(UNIX)
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
endif(UNIX)
target_link_libraries(${PROJECT_NAME} PRIVATE fltk fltk_gl X11 Xext GL GLEW)
target_sources(${PROJECT_NAME} PRIVATE 
 src/main.cpp
)
