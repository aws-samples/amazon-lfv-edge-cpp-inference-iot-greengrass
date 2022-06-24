#
 # Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 # SPDX-License-Identifier: MIT-0
 #
 # Permission is hereby granted, free of charge, to any person obtaining a copy of this
 # software and associated documentation files (the "Software"), to deal in the Software
 # without restriction, including without limitation the rights to use, copy, modify,
 # merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 # permit persons to whom the Software is furnished to do so.
 #
 # THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 # INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 # PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 # HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 # OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 # SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 #

cmake_minimum_required (VERSION 3.16)

project(lookoutvision-inference-src)

message (STATUS "PROJECT_NAME ...................... " ${PROJECT_NAME})
message (STATUS "CMAKE_SOURCE_DIR ...................... " ${CMAKE_SOURCE_DIR})
message (STATUS "CMAKE_INSTALL_PREFIX ...................... " ${CMAKE_INSTALL_PREFIX})

set(PROJECT_BUILD_DIR ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_INSTALL_LIBDIR ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_INSTALL_BINDIR ${CMAKE_INSTALL_PREFIX}/bin)
message (STATUS "PROJECT_BUILD_DIR ...................... " ${PROJECT_BUILD_DIR})
message (STATUS "CMAKE_INSTALL_LIBDIR ...................... " ${CMAKE_INSTALL_LIBDIR})
message (STATUS "CMAKE_INSTALL_BINDIR ...................... " ${CMAKE_INSTALL_BINDIR})

include(${CMAKE_SOURCE_DIR}/cmake/common.cmake)
find_package(gRPC REQUIRED HINTS ${CMAKE_SOURCE_DIR}/third_party/grpc)
find_package(OpenCV REQUIRED)

# variable from main cmake
find_package(Threads REQUIRED)

add_executable(
    main src/main.cc
)

target_include_directories(main
    PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(main
    PUBLIC
	hw_grpc_proto
	gRPC::grpc++
    ${OpenCV_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
)

#install scripts
install(TARGETS main
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)