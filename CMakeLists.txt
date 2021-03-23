cmake_minimum_required(VERSION 3.0)

project(webrtc_server)

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
add_compile_options(-g)   

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

#include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/include)

file(GLOB SRC_LIST ./dtls/*.cc  ./srtp/*.cc ./webrtctransport/*.cpp)

add_library(webrtc ${SRC_LIST})

file(GLOB EXAMPLE_LIST ./main/*.cpp)

set(LINK_LIBRARYS 
srtp2
ssl 
crypto
dl pthread )

set(RTC_LIBRARYS webrtc avcodec avfilter avutil muduo_http muduo_net muduo_base)        

add_executable(testWebrtc ${EXAMPLE_LIST})

target_link_libraries(testWebrtc ${RTC_LIBRARYS} ${LINK_LIBRARYS})

