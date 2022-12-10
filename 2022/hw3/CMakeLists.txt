cmake_minimum_required(VERSION 3.0)
project(hw3)

cmake_policy(SET CMP0072 NEW)
cmake_policy(SET CMP0074 NEW)

set(CMAKE_CXX_STANDARD 20)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/modules")

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(SDL2 REQUIRED)

if (APPLE)
    # brew version of glew doesn't provide GLEW_* variables
    get_target_property(GLEW_INCLUDE_DIRS GLEW::GLEW INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(GLEW_LIBRARIES GLEW::GLEW INTERFACE_LINK_LIBRARIES)
    get_target_property(GLEW_LIBRARY GLEW::GLEW LOCATION)
    list(APPEND GLEW_LIBRARIES "${GLEW_LIBRARY}")
endif ()

set(TARGET_NAME "${PROJECT_NAME}")

set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(${TARGET_NAME} main.cpp
        stb_image.h stb_image.c utils/utils.cpp utils/utils.h
        rapiragl/utils/strong_typedef.h rapiragl/components/file_reader/file_reader.cpp
        rapiragl/components/file_reader/file_reader.h rapiragl/components/shader/shader.cpp
        rapiragl/components/shader/shader.h rapiragl/common/types.h rapiragl/rapiragl.h
        rapiragl/components/texture_loader/texture_loader.cpp rapiragl/components/texture_loader/texture_loader.h
        rapiragl/components/array_scene_loader/array_scene_loader.cpp
        rapiragl/components/array_scene_loader/array_scene_loader.h obj_parser.cpp obj_parser.h)
target_include_directories(${TARGET_NAME} PUBLIC
        "${SDL2_INCLUDE_DIRS}"
        "${GLEW_INCLUDE_DIRS}"
        "${OPENGL_INCLUDE_DIRS}"
        )
target_link_libraries(${TARGET_NAME} PUBLIC
        "${GLEW_LIBRARIES}"
        "${SDL2_LIBRARIES}"
        "${OPENGL_LIBRARIES}"
        )
#target_compile_definitions(${TARGET_NAME} PUBLIC -DPROJECT_ROOT= "${PROJECT_ROOT}")