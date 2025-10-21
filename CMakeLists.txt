cmake_minimum_required(VERSION 3.28.3)

project(financial_radar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependency configuration
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(spdlog REQUIRED)
add_subdirectory(external/SimpleAmqpClient EXCLUDE_FROM_ALL)

set(CROW_ENABLE_COMPRESSION FALSE)
set(CROW_ENABLE_SSL FALSE)
set(CROW_BUILD_EXAMPLES FALSE)
set(CROW_BUILD_TESTS FALSE)
add_subdirectory(external/Crow EXCLUDE_FROM_ALL)

add_subdirectory(external/date EXCLUDE_FROM_ALL)
add_subdirectory(external/stduuid EXCLUDE_FROM_ALL)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror")

file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/external/SimpleAmqpClient/src>
)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    Crow::Crow
    spdlog::spdlog
    date::date
    SimpleAmqpClient
    rabbitmq
    stduuid
)
