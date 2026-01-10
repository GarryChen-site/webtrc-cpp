cmake_minimum_required(VERSION 3.12...4.2)
project(webrtc-cpp)

set(CMAKE_CXX_STANDARD 20)

set(LIBWEBRTC_PATH "${CMAKE_CURRENT_SOURCE_DIR}/local")

include_directories(${LIBWEBRTC_PATH}/include)
include_directories(${LIBWEBRTC_PATH}/include/third_party/abseil-cpp)
link_directories(${LIBWEBRTC_PATH}/lib)

list(APPEND webrtc_libs "webrtc")

set(CMAKE_OSX_ARCHITECTURES "x86_64")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWEBRTC_MAC=1 -DWEBRTC_POSIX=1")
find_library(FOUNDATION Foundation)
list(APPEND webrtc_libs ${FOUNDATION})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g")

add_executable(rebuild_sample
  main.cpp
)

target_link_libraries(rebuild_sample
  ${webrtc_libs}
)