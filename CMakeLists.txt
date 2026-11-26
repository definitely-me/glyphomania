cmake_minimum_required(VERSION 3.20)
project(glyphomania C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_FLAGS "-Wall -Wextra")
set(CMAKE_C_FLAGS_RELEASE "-O3 -march=native -DNDEBUG -flto")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-flto")

add_executable(glyphomania
        src/image.c
        src/filter.c
        src/utils.c
        src/ascii.c
        src/main.c
)

target_link_libraries(glyphomania PRIVATE m)