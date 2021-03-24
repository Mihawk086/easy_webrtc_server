cmake_minimum_required(VERSION 3.0)

project(webrtc_server)

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
add_compile_options(-g)   

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB SRC_LIST ./dtls/*.cc  ./srtp/*.cc ./webrtctransport/*.cc ./ffmpeg_src/*.cc)

file(GLOB RTC_LIST ./rtc/*.cc ./ffmpeg_src/*.cc ./common/*.cc)

add_library(webrtc ${SRC_LIST})

add_library(rtc ${RTC_LIST})

file(GLOB MAIN_LIST ./main/main.cc)

file(GLOB EXAMPLE_LIST ./main/example.cc)

set(LINK_LIBRARYS 
srtp2
ssl 
crypto
dl pthread )

set(RTC_LIBRARYS webrtc avcodec avfilter avutil muduo_http muduo_net muduo_base)

set(RTC_LIBRARYS2 rtc avcodec avfilter avutil muduo_http muduo_net muduo_base)
        
add_executable(test_webrtc ${MAIN_LIST})

add_executable(example ${EXAMPLE_LIST})

target_link_libraries(test_webrtc ${RTC_LIBRARYS} ${LINK_LIBRARYS})

target_link_libraries(example ${RTC_LIBRARYS2} ${LINK_LIBRARYS})

