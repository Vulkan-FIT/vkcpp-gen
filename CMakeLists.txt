cmake_minimum_required(VERSION 3.5)

project(generator LANGUAGES CXX)

OPTION(GENERATOR_GUI "gui" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(GENERATOR_GUI)
    find_package(Vulkan REQUIRED)

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

set(FF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fastformat)
set(STL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/stlsoft)

SET(HEADERS
    src/ArgumentsParser.hpp
    src/XMLVariableParser.hpp
    src/XMLUtils.hpp
    src/StringUtils.hpp
    src/Gui.hpp
    src/Generator.hpp
    )

SET(SOURCES
    src/main.cpp
    src/XMLVariableParser.cpp
    src/Gui.cpp
    src/Generator.cpp
    )

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES} ${HEADERS} ${IMGUI_SOURCES})

target_link_libraries(${CMAKE_PROJECT_NAME} tinyxml2)

add_subdirectory(tinyxml2)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${FF_DIR}/include ${STL_DIR}/include)

if(GENERATOR_GUI)

    add_subdirectory(glfw)
    target_include_directories(${CMAKE_PROJECT_NAME}
                               PUBLIC ${Vulkan_INCLUDE_DIRS}
                               PUBLIC ${IMGUI_DIR}
                               PUBLIC glfw/include
                               )

    target_link_libraries(${CMAKE_PROJECT_NAME} ${Vulkan_LIBRARIES} glfw)
endif(GENERATOR_GUI)

