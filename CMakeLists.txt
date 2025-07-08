cmake_minimum_required(VERSION 3.15)
project(VulkanApp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Vulkan SDK
find_package(Vulkan REQUIRED)

# GLFW
add_subdirectory(vendor/glfw)
set(GLFW_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/vendor/glfw/include)

# GLM
set(GLM_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/vendor/glm)

# Include directories
include_directories(
    ${GLFW_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
)

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")

foreach(source_file ${SOURCES})
    get_filename_component(source_path "${source_file}" PATH)
    file(RELATIVE_PATH source_path_rel "${CMAKE_SOURCE_DIR}/src" "${source_path}")
    string(REPLACE "/" "\\" source_group_name "${source_path_rel}")
    source_group("${source_group_name}" FILES "${source_file}")
endforeach()


add_executable(VulkanApp ${SOURCES})

# Link libraries
target_link_libraries(VulkanApp Vulkan::Vulkan glfw)
