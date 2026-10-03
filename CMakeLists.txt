cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(JsonAdapter LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

add_library(jsonadapter STATIC Json.cpp)
target_include_directories(jsonadapter PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/rapidjson/include")
target_compile_features(jsonadapter PUBLIC cxx_std_20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(JSONADAPTER_BUILD_TESTS "Build JsonAdapter tests" ON)
else()
    option(JSONADAPTER_BUILD_TESTS "Build JsonAdapter tests" OFF)
endif()

if(JSONADAPTER_BUILD_TESTS)
    enable_testing()
    add_executable(jsonadapter_tests test/json_adapter_tests.cpp)
    target_link_libraries(jsonadapter_tests PRIVATE jsonadapter)
    add_test(NAME jsonadapter_tests COMMAND jsonadapter_tests)
endif()
