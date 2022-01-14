cmake_minimum_required(VERSION 3.17)
project(Lightmapper)

set(CMAKE_CXX_STANDARD 14)

add_definitions(-DPROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(Lightmapper main.cpp Model.cpp LightMap.cpp Importer.cpp)

# glfw
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/External/glfw EXCLUDE_FROM_ALL glfw.out)
target_link_libraries(Lightmapper PRIVATE glfw)

# blast
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/External/Blast Blast.out)
target_link_libraries(Lightmapper PUBLIC Blast)

# xatlas
add_library(xatlas ${CMAKE_CURRENT_SOURCE_DIR}/External/xatlas/xatlas.cpp)
target_include_directories(xatlas INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/External/xatlas)
target_link_libraries(Lightmapper PRIVATE xatlas)

# cgltf
add_library(cgltf INTERFACE)
target_include_directories(cgltf INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/External/cgltf)
target_link_libraries(Lightmapper PRIVATE cgltf)

# glm
add_library(glm INTERFACE)
target_compile_definitions(glm INTERFACE GLM_FORCE_DEPTH_ZERO_TO_ONE=1)
target_include_directories(glm INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/External/glm)
target_link_libraries(Lightmapper PUBLIC glm)

# stb
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/External/stb)
target_link_libraries(Lightmapper PRIVATE stb)