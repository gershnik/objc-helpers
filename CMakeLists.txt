# Copyright 2020 Eugene Gershnik
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://github.com/gershnik/objc-helpers/blob/main/LICENSE

cmake_minimum_required(VERSION 3.25)

file(READ VERSION OBJC_HELPERS_VERSION)
if (NOT OBJC_HELPERS_VERSION)
    message(FATAL_ERROR "Cannot determine library version (VERSION file not found)")
endif()
string(STRIP ${OBJC_HELPERS_VERSION} OBJC_HELPERS_VERSION)

project(objc-helpers VERSION ${OBJC_HELPERS_VERSION} LANGUAGES CXX)


add_library(objc-helpers INTERFACE)

set(SRCDIR ${CMAKE_CURRENT_LIST_DIR})

target_include_directories(objc-helpers 
    INTERFACE
        $<BUILD_INTERFACE:${SRCDIR}/include>
        $<INSTALL_INTERFACE:include>  # <prefix>/include
)


set(PUBLIC_HEADERS 
    ${SRCDIR}/include/objc-helpers/BlockUtil.h
    ${SRCDIR}/include/objc-helpers/BoxUtil.h 
    ${SRCDIR}/include/objc-helpers/CoDispatch.h 
    ${SRCDIR}/include/objc-helpers/NSNumberUtil.h 
    ${SRCDIR}/include/objc-helpers/NSObjectUtil.h 
    ${SRCDIR}/include/objc-helpers/NSStringUtil.h 
    ${SRCDIR}/include/objc-helpers/XCTestUtil.h
)

target_sources(objc-helpers
INTERFACE 
    FILE_SET HEADERS BASE_DIRS ${SRCDIR}/include FILES
        ${PUBLIC_HEADERS}
PRIVATE 
    ${PUBLIC_HEADERS}
)

add_library(objc-helpers::objc-helpers ALIAS objc-helpers)

if (PROJECT_IS_TOP_LEVEL)
    include(.cmake/install.cmake)
    
endif()

