list(APPEND ${PROJECT_NAME}_SOURCES
    search_engine.cpp
    main.cpp)

add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SOURCES})
target_link_libraries(${PROJECT_NAME} CONAN_PKG::boost CONAN_PKG::cryptopp)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    COMPILE_OPTIONS "-Wpedantic;-Wall;-Wextra"
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)