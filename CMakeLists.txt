cmake_minimum_required(VERSION 3.10)

project(financial_radar LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
endif()

file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")
file(GLOB SOURCES "${CMAKE_SOURCE_DIR}/src/*.h")

# Dependencies via submodules
add_subdirectory(external/spdlog)


add_subdirectory(external/AMQP-CPP EXCLUDE_FROM_ALL)

set(CROW_ENABLE_COMPRESSION FALSE)
set(CROW_ENABLE_SSL FALSE)
set(CROW_BUILD_EXAMPLES FALSE)
set(CROW_BUILD_TESTS FALSE)
add_subdirectory(external/Crow EXCLUDE_FROM_ALL)
add_subdirectory(external/uuid)

# Подключаем date
add_subdirectory(external/date)

find_package(Boost REQUIRED COMPONENTS uuid)

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})
<<<<<<< HEAD

target_link_libraries(
    ${CMAKE_PROJECT_NAME}
    PRIVATE
        amqpcpp
        Crow::Crow
        spdlog::spdlog
        date::date
        Boost::uuid
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Werror)
elseif(MSVC)
    target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /WX) 
endif()
=======
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE amqpcpp Crow::Crow spdlog::spdlog Boost::uuid)
>>>>>>> ffde0fdd5992f9f2dc38fec1d873159e0e2c0177
