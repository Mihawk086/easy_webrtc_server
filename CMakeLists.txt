
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/include)

link_libraries(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lib)

file(GLOB SRC_LIST ./dtls/*.cpp ./dtls/*.c ./net/*.cpp ./xop/*.cpp ./webrtctransport/*.cpp)

add_library(webrtc ${SRC_LIST})

file(GLOB EXAMPLE_LIST ./example/*.cpp)

find_package(Boost REQUIRED COMPONENTS regex filesystem thread system)

find_library(LOG log4cxx)

set(LINK_LIBRARYS  ${Boost_LIBRARIES} ${LOG}  
		srtp2.a ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lib/libssl.a ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lib/libcrypto.a
        dl pthread )

add_executable(testWebrtc ${EXAMPLE_LIST})

target_link_libraries(testWebrtc webrtc  avcodec avfilter avutil ${LINK_LIBRARYS})