cmake_minimum_required(VERSION 3.23)
project(ShaderPT)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(ShaderPT
        glad.c
        glad_wgl.c
        main.c
)

add_custom_command(TARGET ShaderPT POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/compute.glsl" "${CMAKE_SOURCE_DIR}/fragment.glsl" "${CMAKE_SOURCE_DIR}/vertex.glsl" $<TARGET_FILE_DIR:ShaderPT>)

find_package(OpenGL REQUIRED)
target_link_libraries(ShaderPT OpenGL::GL)
