add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/core)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/webserver)

target_sources(${CMAKE_PROJECT_NAME}
    PUBLIC
        main.cpp
)
