# Copyright 2020 Eugene Gershnik
#
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file or at
# https://github.com/gershnik/objc-helpers/blob/master/LICENSE


include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS objc-helpers EXPORT objc-helpers FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT objc-helpers NAMESPACE objc-helpers:: FILE objc-helpers-exports.cmake DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/objc-helpers)


configure_package_config_file(
        ${CMAKE_CURRENT_LIST_DIR}/objc-helpers-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/objc-helpers-config.cmake
    INSTALL_DESTINATION
        ${CMAKE_INSTALL_DATAROOTDIR}/objc-helpers
)

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/objc-helpers-config-version.cmake
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/objc-helpers-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/objc-helpers-config-version.cmake
    DESTINATION
        ${CMAKE_INSTALL_DATAROOTDIR}/objc-helpers
)

file(RELATIVE_PATH FROM_PCFILEDIR_TO_PREFIX ${CMAKE_INSTALL_FULL_DATAROOTDIR}/objc-helpers ${CMAKE_INSTALL_PREFIX})
string(REGEX REPLACE "/+$" "" FROM_PCFILEDIR_TO_PREFIX "${FROM_PCFILEDIR_TO_PREFIX}") 

configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/objc-helpers.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/objc-helpers.pc
    @ONLY
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/objc-helpers.pc
    DESTINATION
        ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig
)