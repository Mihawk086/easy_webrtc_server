cmake_minimum_required(VERSION 3.16)

project(webrtc_server)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/include)

link_libraries(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lib)

file(GLOB SRC_LIST ./dtls/*.cpp ./dtls/*.c ./webrtctransport/*.cpp)

add_library(webrtc ${SRC_LIST})

file(GLOB EXAMPLE_LIST ./main/*.cpp)

find_package(Boost REQUIRED COMPONENTS regex filesystem thread system)

find_library(LOG log4cxx)

set(LINK_LIBRARYS  ${Boost_LIBRARIES} ${LOG}  
${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lib/srtp2.a 
${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lib/libssl.a 
${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lib/libcrypto.a
        dl pthread )

set(RTC_LIBRARYS webrtc avcodec avfilter avutil muduo_http muduo_net muduo_base)        

#add_executable(testWebrtc ${EXAMPLE_LIST})

#target_link_libraries(testWebrtc ${RTC_LIBRARYS} ${LINK_LIBRARYS})

set(ToolKit_Include ${CMAKE_CURRENT_SOURCE_DIR}/ZLMediaKit/3rdpart/ZLToolKit/src)
set(MediaKit_Include ${CMAKE_CURRENT_SOURCE_DIR}/ZLMediaKit/src)
include_directories(${ToolKit_Include})
include_directories(${MediaKit_Include})
