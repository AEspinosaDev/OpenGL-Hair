cmake_minimum_required(VERSION 3.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


project(Hair_Renderer VERSION 0.0.1)

find_package(OpenGL REQUIRED)

#Set up dependencies
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/glfw-3.4)
add_subdirectory(deps/glad)
add_subdirectory(deps/glm-1.0.1)
add_subdirectory(deps/imgui) 
add_subdirectory(deps/stb_image)
add_subdirectory(deps/tiny_obj_loader)	
add_subdirectory(deps/tinyply)	


file(GLOB SOURCES
"src/*.cpp"
"src/*.h"
"src/engine/*.cpp"
"src/engine/*.h"
"deps/imgui/*.cpp"
"deps/imgui/*.h"
)

add_executable(HairViewer ${SOURCES})


#Link libraries
target_link_libraries(HairViewer PUBLIC glm glfw glad stb_image imgui tiny_obj_loader tinyply)
#Set dependencies inside folder
set_property(TARGET glfw glad glm imgui stb_image tiny_obj_loader tinyply PROPERTY FOLDER "deps")

target_compile_definitions(HairViewer PUBLIC RESOURCES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/resources/")


set_target_properties(HairViewer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/debug
)