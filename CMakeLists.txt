cmake_minimum_required(VERSION 3.28.3)

project(financial_radar LANGUAGES CXX)

# Dependency check
find_package(Crow REQUIRED)
find_package(spdlog REQUIRED)
find_package(amqpcpp REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror")

file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Crow::Crow spdlog::spdlog amqpcpp)
