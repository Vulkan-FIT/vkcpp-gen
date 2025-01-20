cmake_minimum_required(VERSION 3.5)

project(vkcpp-gen LANGUAGES CXX)

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

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fuse-ld=lld -march=native")
endif()

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
endif()

SET(HEADERS
    src/ArgumentsParser.hpp
    src/Enums.hpp
    src/Config.hpp
    src/Variable.hpp
    src/Gui.hpp
    src/Registry.hpp
    src/Members.hpp
    src/Generator.hpp
    src/Utils.hpp
    src/Output.hpp
)

SET(SOURCES
    src/main.cpp
    src/Config.cpp
    src/Variable.cpp
    src/Gui.cpp
    src/Generator.cpp
    src/Utils.cpp
    src/Registry.cpp
    src/Members.cpp
    src/Enums.cpp
    src/Format.hpp
    src/Output.cpp
)

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES} ${HEADERS} ${IMGUI_SOURCES})

if(ENABLE_TOOL)
    add_compile_definitions(GENERATOR_TOOL)
    target_sources(${CMAKE_PROJECT_NAME}
        PRIVATE
        src/tool/tool.hpp
        src/tool/tool.cpp
    )
endif()

target_link_libraries(${CMAKE_PROJECT_NAME} tinyxml2)

set(tinyxml2_BUILD_TESTING OFF CACHE BOOL "Build tests for tinyxml2")

add_subdirectory(tinyxml2)
add_subdirectory(fmt)

target_include_directories(${CMAKE_PROJECT_NAME}
    PUBLIC fmt/include
)

target_link_libraries(${CMAKE_PROJECT_NAME} fmt)

if(GENERATOR_GUI)

    add_subdirectory(glfw)
    target_include_directories(${CMAKE_PROJECT_NAME}
                               PUBLIC ${Vulkan_INCLUDE_DIRS}
                               PUBLIC ${IMGUI_DIR}
                               PUBLIC glfw/include
                               )

    target_link_libraries(${CMAKE_PROJECT_NAME} glfw ${Vulkan_LIBRARIES})
endif()

if( MSVC )
    if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.6.0")
        set_property( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${CMAKE_PROJECT_NAME} )
    endif()
endif()

# add_subdirectory(src/tool)