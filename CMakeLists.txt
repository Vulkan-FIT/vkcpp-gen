cmake_minimum_required(VERSION 3.5)

project(generator LANGUAGES CXX)

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

OPTION(GENERATOR_GUI "gui" ON)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND AND USE_CCACHE)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    message("using CCACHE")
endif()

if(GENERATOR_GUI)
    find_package(Vulkan REQUIRED)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(GENERATOR_GUI)
    set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

    SET(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp        
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    )

    add_compile_definitions(GENERATOR_GUI)
endif(GENERATOR_GUI)

SET(HEADERS
    src/ArgumentsParser.hpp
    src/Enums.h
    src/XMLVariableParser.hpp
    src/Gui.hpp
    src/Generator.hpp
    src/Utils.hpp
    )

SET(SOURCES
    src/main.cpp
    src/XMLVariableParser.cpp
    src/Gui.cpp
    src/Generator.cpp
    src/Utils.cpp
    )

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES} ${HEADERS} ${IMGUI_SOURCES})

target_link_libraries(${CMAKE_PROJECT_NAME} tinyxml2)

add_subdirectory(tinyxml2)

if(GENERATOR_GUI)

    add_subdirectory(glfw)
    target_include_directories(${CMAKE_PROJECT_NAME}
                               PUBLIC ${Vulkan_INCLUDE_DIRS}
                               PUBLIC ${IMGUI_DIR}
                               PUBLIC glfw/include
                               )

    target_link_libraries(${CMAKE_PROJECT_NAME} glfw ${Vulkan_LIBRARIES})
endif(GENERATOR_GUI)

