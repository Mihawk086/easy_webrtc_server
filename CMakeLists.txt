cmake_minimum_required(VERSION 3.0)

project(webrtc_server)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/include)

link_libraries(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lib)

file(GLOB SRC_LIST ./dtls/*.cpp ./dtls/*.c ./webrtctransport/*.cpp)

add_library(webrtc ${SRC_LIST})

file(GLOB EXAMPLE_LIST ./main/*.cpp)

set(LINK_LIBRARYS   
srtp2.a 
${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lib/libssl.a 
${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lib/libcrypto.a
dl pthread )

set(RTC_LIBRARYS webrtc avcodec avfilter avutil muduo_http muduo_net muduo_base)        

add_executable(testWebrtc ${EXAMPLE_LIST})

target_link_libraries(testWebrtc ${RTC_LIBRARYS} ${LINK_LIBRARYS})

