cmake_minimum_required(VERSION 3.10)

project(vpxds)

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)
find_package(X11 REQUIRED)

pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)

include_directories(${CMAKE_SOURCE_DIR} ${X11_INCLUDE_DIR} ${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS})

link_directories(${SDL2_LIBRARY_DIRS} ${SDL2_IMAGE_LIBRARY_DIRS})

add_executable(vpxds
   main.cpp
   inc/mongoose/mongoose.c
)

target_link_libraries(vpxds ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES} ${X11_LIBRARIES}) 
