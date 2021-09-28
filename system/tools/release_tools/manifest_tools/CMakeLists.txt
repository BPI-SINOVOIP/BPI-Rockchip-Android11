set (PROJECT_NAME "manifest_tool")

file (GLOB LOCAL_H_FILES ../*.h)
file (GLOB LOCAL_CPP_FILES *.cpp ../release_utils.cpp)
set (SOURCE_FILES ${SOURCE_FILES} ${LOCAL_CPP_FILES})
set (HEADER_FILES ${HEADER_FILES} ${LOCAL_H_FILES})

cmake_minimum_required(VERSION 3.5)

project (${PROJECT_NAME})

add_executable (${PROJECT_NAME} ${SOURCE_FILES})
