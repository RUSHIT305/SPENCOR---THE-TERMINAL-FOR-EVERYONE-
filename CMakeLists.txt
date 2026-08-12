cmake_minimum_required(VERSION 3.22)

project(SPENCER
  VERSION 0.2.1
  DESCRIPTION "The Terminal for Everyone"
  LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "SPENCER currently supports Linux only; use a Linux build environment.")
endif()

include(GNUInstallDirs)
include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(SPENCER_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(SPENCER_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_library(spencer_core
  src/config/config.cpp
  src/parser/terminal_parser.cpp
  src/parser/utf8_decoder.cpp
  src/pty/linux_pty.cpp
  src/terminal/terminal_state.cpp)
add_library(spencer::core ALIAS spencer_core)

target_include_directories(spencer_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_features(spencer_core PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set(SPENCER_WARNING_FLAGS
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Wformat=2
    -Werror=return-type)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(SPENCER_WARNING_FLAGS /W4)
else()
  set(SPENCER_WARNING_FLAGS)
endif()

target_compile_options(spencer_core PRIVATE ${SPENCER_WARNING_FLAGS})

if(SPENCER_WARNINGS_AS_ERRORS)
  target_compile_options(spencer_core PRIVATE -Werror)
endif()

if(SPENCER_ENABLE_SANITIZERS)
  target_compile_options(spencer_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(spencer_core PUBLIC -fsanitize=address,undefined)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)

include(CheckCXXSourceCompiles)
check_cxx_source_compiles("#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
int main() { const int fd = posix_openpt(O_RDWR | O_NOCTTY); if (fd >= 0) close(fd); return 0; }"
  SPENCER_HAS_POSIX_PTY)
if(NOT SPENCER_HAS_POSIX_PTY)
  message(FATAL_ERROR "The target Linux environment lacks the required POSIX PTY API (posix_openpt).")
endif()

add_executable(spencer src/app/main.cpp)
target_link_libraries(spencer PRIVATE spencer::core PkgConfig::GTK4)
target_compile_options(spencer PRIVATE ${SPENCER_WARNING_FLAGS})
if(SPENCER_WARNINGS_AS_ERRORS)
  target_compile_options(spencer PRIVATE -Werror)
endif()

if(BUILD_TESTING)
  add_executable(spencer_tests
    tests/test_main.cpp
    tests/unit/test_config.cpp
    tests/unit/test_terminal_state.cpp
    tests/unit/test_terminal_parser.cpp
    tests/unit/test_utf8_decoder.cpp
    tests/integration/test_linux_pty.cpp)
  target_link_libraries(spencer_tests PRIVATE spencer::core)
  target_include_directories(spencer_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_compile_options(spencer_tests PRIVATE ${SPENCER_WARNING_FLAGS})
  add_test(NAME spencer.unit COMMAND spencer_tests)
endif()

install(TARGETS spencer
  EXPORT SPENCERTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES packaging/linux/io.github.rushit305.spencer.desktop
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install(FILES packaging/linux/io.github.rushit305.spencer.metainfo.xml
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo)
install(FILES assets/icons/io.github.rushit305.spencer.svg
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)
install(FILES assets/icons/io.github.rushit305.spencer.png
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps)

set(CPACK_PACKAGE_NAME "spencer")
set(CPACK_PACKAGE_VENDOR "SPENCER contributors")
set(CPACK_PACKAGE_CONTACT "https://github.com/RUSHIT305/SPENCOR---THE-TERMINAL-FOR-EVERYONE-/issues")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SPENCER — The Terminal for Everyone")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/RUSHIT305/SPENCOR---THE-TERMINAL-FOR-EVERYONE-")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_GENERATOR "DEB;RPM")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "SPENCER contributors")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Utilities")
set(CPACK_RPM_PACKAGE_URL "https://github.com/RUSHIT305/SPENCOR---THE-TERMINAL-FOR-EVERYONE-")
set(CPACK_RPM_PACKAGE_RELEASE 1)
set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
set(CPACK_RPM_PACKAGE_AUTOREQPROV ON)
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)

include(CPack)
