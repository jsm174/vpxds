cmake_minimum_required(VERSION 3.10)

project(vpxds)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

find_package(PkgConfig REQUIRED)
find_package(X11 REQUIRED)
find_package(OpenMP REQUIRED)

pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
pkg_check_modules(IMAGEQUANT REQUIRED imagequant)

include_directories(
   ${CMAKE_SOURCE_DIR}
   ${X11_INCLUDE_DIR}
   ${SDL2_INCLUDE_DIRS}
   ${SDL2_IMAGE_INCLUDE_DIRS}
   ${IMAGEQUANT_INCLUDE_DIRS}
)

link_directories(
   ${SDL2_LIBRARY_DIRS}
   ${SDL2_IMAGE_LIBRARY_DIRS}
   ${IMAGEQUANT_LIBRARY_DIRS}
)

add_executable(vpxds
   main.cpp
   VPXDisplayServer.h
   VPXDisplayServer.cpp
   inc/mongoose/mongoose.c
   inc/mongoose/mongoose.h
   inc/lodepng/lodepng.cpp
   inc/lodepng/lodepng.h
)

target_link_libraries(vpxds
   ${SDL2_LIBRARIES}
   ${SDL2_IMAGE_LIBRARIES}
   ${X11_LIBRARIES}
   libimagequant.a
   OpenMP::OpenMP_CXX
)

add_custom_command(TARGET vpxds POST_BUILD
   COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:vpxds>/assets"
)
