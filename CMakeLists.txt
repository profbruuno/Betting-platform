cmake_minimum_required(VERSION 3.16)
project(FootballBetting)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)

# Find SQLite3
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

add_executable(betting_server server.cpp)

target_include_directories(betting_server PRIVATE ${SQLITE3_INCLUDE_DIRS})
target_link_libraries(betting_server ${SQLITE3_LIBRARIES} pthread)

if(WIN32)
    target_link_libraries(betting_server ws2_32 wsock32)
endif()
