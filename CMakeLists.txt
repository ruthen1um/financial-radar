cmake_minimum_required(VERSION 3.28.3)

project(financial_radar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror")

file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")

set(CROW_ENABLE_COMPRESSION FALSE)
set(CROW_ENABLE_SSL FALSE)
set(CROW_BUILD_EXAMPLES FALSE)
set(CROW_BUILD_TESTS FALSE)

add_subdirectory(external/Crow EXCLUDE_FROM_ALL)

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})

target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC Crow::Crow)
