add_executable(example main.cpp)

target_link_libraries(example zlmediakit zltoolkit mov mpeg flv ${RTC_LIBRARYS} ${LINK_LIBRARYS})