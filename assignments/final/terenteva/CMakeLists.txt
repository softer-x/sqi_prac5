cmake_minimum_required(VERSION 3.14)
project(MusicAiAgent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

find_package(OpenSSL REQUIRED)

add_executable(music_ai_agent
    src/AiAgent.cpp
    src/MusicAgent.cpp
    src/main.cpp
)

target_include_directories(music_ai_agent PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(music_ai_agent
    PRIVATE
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.json
    ${CMAKE_CURRENT_BINARY_DIR}/config.json
    COPYONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/prompt.json
    ${CMAKE_CURRENT_BINARY_DIR}/prompt.json
    COPYONLY
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/yandex_music_client.py
    ${CMAKE_CURRENT_BINARY_DIR}/yandex_music_client.py
    COPYONLY
)

