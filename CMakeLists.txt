cmake_minimum_required(VERSION 3.0)

project(webrtc_server)

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")
add_compile_options(-g)   

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB RTC_LIST ./rtc/*.cc ./ffmpeg_src/*.cc ./common/*.cc)

add_library(rtc ${RTC_LIST})

set(LINK_LIBRARYS 
srtp2
ssl 
crypto
dl pthread )

set(RTC_LIBRARYS rtc  muduo_http muduo_net muduo_base)

set(FFMPEG_LIBRARYS avcodec avfilter avutil)
        
add_executable(ffmpeg_src_example ./main/ffmpeg_src_example.cc)

add_executable(rtp_src_example ./main/rtp_src_example.cc)

target_link_libraries(ffmpeg_src_example ${RTC_LIBRARYS} ${FFMPEG_LIBRARYS} ${LINK_LIBRARYS})

target_link_libraries(rtp_src_example ${RTC_LIBRARYS} ${LINK_LIBRARYS})

