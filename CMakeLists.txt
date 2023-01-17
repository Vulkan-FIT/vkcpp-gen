cmake_minimum_required(VERSION 3.5)

project(generator LANGUAGES CXX)

OPTION(GENERATOR_GUI "gui" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# add_shaders macro to convert GLSL shaders to spir-v
# and creates depsList containing name of files that should be included among the source files
macro(add_shaders nameList depsList)
        foreach(name ${nameList})
                get_filename_component(directory ${name} DIRECTORY)
                if(directory)
                        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${directory}")
                endif()
                add_custom_command(COMMENT "Converting ${name} to spir-v..."
                                   MAIN_DEPENDENCY ${name}
                                   OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${name}.spv
                                   COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V -x ${CMAKE_CURRENT_SOURCE_DIR}/${name} -o ${CMAKE_CURRENT_SOURCE_DIR}/${name}.spv)
                source_group("Shaders" FILES ${name} ${CMAKE_CURRENT_SOURCE_DIR}/${name}.spv)
                list(APPEND ${depsList} ${name} ${CMAKE_CURRENT_SOURCE_DIR}/${name}.spv)
        endforeach()
endmacro()

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
    src/Enums.h
    src/XMLVariableParser.hpp
    src/XMLUtils.hpp
    src/StringUtils.hpp
    src/Gui.hpp
    src/Generator.hpp
    src/Instrumentation.h
    )

SET(SOURCES
    src/main.cpp
    src/XMLVariableParser.cpp
    src/Gui.cpp
    src/Generator.cpp
    src/Instrumentation.cpp
    )

set(APP_SHADERS
    shaders/vs.vert
    shaders/ps.frag
   )

add_shaders("${APP_SHADERS}" APP_SHADER_DEPS)

message("deps: ${APP_SHADER_DEPS}")

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES} ${HEADERS} ${IMGUI_SOURCES} ${APP_SHADER_DEPS})

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

