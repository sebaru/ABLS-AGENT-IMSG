cmake_minimum_required(VERSION 3.16)

execute_process(
  COMMAND git describe --tags
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE ABLS_AGENT_IMSG_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)

if(NOT ABLS_AGENT_IMSG_VERSION)
  set(ABLS_AGENT_IMSG_VERSION "unknown")
endif()

if(ABLS_AGENT_IMSG_VERSION MATCHES "^v?([0-9]+\\.[0-9]+)")
  set(ABLS_AGENT_IMSG_VERSION_NUMBER "${CMAKE_MATCH_1}")
else()
  set(ABLS_AGENT_IMSG_VERSION_NUMBER "0.0")
endif()

project(abls-agent-imsg
  VERSION ${ABLS_AGENT_IMSG_VERSION_NUMBER}
  DESCRIPTION "Abls Habitat IMSG runtime"
  HOMEPAGE_URL "https://docs.abls-habitat.fr"
  LANGUAGES C
)

include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(JSON_GLIB REQUIRED json-glib-1.0)
pkg_check_modules(ABLS_LIBS REQUIRED abls-libs)
pkg_check_modules(ABLS_AGENT_LIBS REQUIRED abls-agent-libs)
pkg_check_modules(LIBSTROPHE REQUIRED libstrophe)

add_executable(abls-agent-imsg
  src/imsg.c
)

target_include_directories(abls-agent-imsg PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${GLIB_INCLUDE_DIRS}
  ${JSON_GLIB_INCLUDE_DIRS}
  ${LIBSTROPHE_INCLUDE_DIRS}
  ${ABLS_LIBS_INCLUDE_DIRS}
  ${ABLS_AGENT_LIBS_INCLUDE_DIRS}
)

target_compile_options(abls-agent-imsg PRIVATE
  -Wall -Wextra -Wno-unused-parameter -Wno-unused-result -pipe -g -ggdb
)

target_compile_definitions(abls-agent-imsg PRIVATE
  ABLS_AGENT_IMSG_VERSION="${ABLS_AGENT_IMSG_VERSION}"
)

target_link_libraries(abls-agent-imsg PRIVATE
  ${GLIB_LIBRARIES}
  ${JSON_GLIB_LIBRARIES}
  ${LIBSTROPHE_LIBRARIES}
  ${ABLS_LIBS_EXACT}
  ${ABLS_AGENT_LIBS_LIBRARIES}
)

install(TARGETS abls-agent-imsg
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
)

execute_process(
  COMMAND pkg-config --variable=systemdsystemunitdir systemd
  OUTPUT_VARIABLE ABLS_SYSTEMD_UNIT_INSTALL_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)

if(NOT ABLS_SYSTEMD_UNIT_INSTALL_DIR)
  set(ABLS_SYSTEMD_UNIT_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib/systemd/system")
endif()

install(FILES packaging/systemd/abls-agent-imsg@.service
  DESTINATION ${ABLS_SYSTEMD_UNIT_INSTALL_DIR}
  COMPONENT runtime
)

set(CPACK_PACKAGE_NAME "abls-agent-imsg")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_RELEASE "1")
set(CPACK_PACKAGE_VENDOR "Abls-Habitat")
set(CPACK_PACKAGE_CONTACT "sebastien.lefevre@abls-habitat.fr")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_LICENSE "GPLv3")

set(CPACK_GENERATOR "RPM;DEB")
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL runtime)

set(CPACK_RPM_RUNTIME_PACKAGE_NAME "abls-agent-imsg")
set(CPACK_RPM_RUNTIME_PACKAGE_GROUP "abls")
set(CPACK_RPM_RUNTIME_PACKAGE_SUMMARY "Abls-Habitat IMSG runtime")
set(CPACK_RPM_RUNTIME_PACKAGE_REQUIRES "glib2, json-glib, libstrophe, abls-libs, abls-agent-libs")
set(CPACK_RPM_RUNTIME_POST_INSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/packaging/rpm/runtime.postinst")
set(CPACK_RPM_RUNTIME_PRE_UNINSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/packaging/rpm/runtime.preun")
set(CPACK_RPM_RUNTIME_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/packaging/rpm/runtime.postun")
set(CPACK_RPM_RUNTIME_FILE_NAME "abls-agent-imsg-${PROJECT_VERSION}-${CPACK_PACKAGE_RELEASE}.rpm")
set(CPACK_RPM_RUNTIME_DEBUGINFO_PACKAGE ON)
set(CPACK_RPM_RUNTIME_DEBUGINFO_FILE_NAME "RPM-DEFAULT")
set(CPACK_RPM_RUNTIME_BUILD_SOURCE_DIRS_PREFIX "/usr/src/abls/imsg")

set(ABLS_DEBIAN_GLIB_RUNTIME_DEPEND "libglib2.0-0t64 | libglib2.0-0")
set(CPACK_DEBIAN_RUNTIME_PACKAGE_NAME "abls-agent-imsg")
set(CPACK_DEBIAN_RUNTIME_PACKAGE_DEPENDS "${ABLS_DEBIAN_GLIB_RUNTIME_DEPEND}, libjson-glib-1.0-0, libstrophe0, abls-libs, abls-agent-libs")
set(CPACK_DEBIAN_RUNTIME_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_RUNTIME_PACKAGE_CONTROL_EXTRA
  "${CMAKE_SOURCE_DIR}/packaging/deb/postinst"
  "${CMAKE_SOURCE_DIR}/packaging/deb/prerm"
  "${CMAKE_SOURCE_DIR}/packaging/deb/postrm"
)

include(CPack)
