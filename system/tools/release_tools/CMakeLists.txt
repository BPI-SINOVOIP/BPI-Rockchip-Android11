set (PROJECT_NAME "release_tool")

file (GLOB LOCAL_H_FILES *.h)
file (GLOB LOCAL_CPP_FILES *.cpp)
set (SOURCE_FILES ${SOURCE_FILES} ${LOCAL_CPP_FILES})
set (HEADER_FILES ${HEADER_FILES} ${LOCAL_H_FILES})

cmake_minimum_required(VERSION 3.5)

execute_process(
    COMMAND git log -1 --format=%H
    OUTPUT_VARIABLE GIT_VERSION
)

execute_process(
    COMMAND git remote -v
    OUTPUT_VARIABLE GIT_REMOTE_PATH
)

string (REGEX REPLACE ".*\t| .*" "" GIT_PATH ${GIT_REMOTE_PATH})
string (REGEX REPLACE "[\n\t\r]" "" GIT_VERSION ${GIT_VERSION})

add_definitions( -DGIT_VERSION=\"${GIT_VERSION}\")
add_definitions( -DGIT_PATH=\"${GIT_PATH}\")

project (${PROJECT_NAME})

add_executable (${PROJECT_NAME} ${SOURCE_FILES})
