cmake_minimum_required(VERSION 3.5)

project(generator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

SET(HEADERS
    ArgumentsParser.hpp
    XMLVariableParser.hpp
    XMLUtils.hpp
    FileHandle.hpp
    StringUtils.hpp
    )

SET(SOURCES
    main.cpp
    )

add_subdirectory(tinyxml2)
add_executable(${CMAKE_PROJECT_NAME} ${SOURCES} ${HEADERS})
target_link_libraries(${CMAKE_PROJECT_NAME} tinyxml2)
