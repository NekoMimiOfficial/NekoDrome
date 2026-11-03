set(NEKO_DEPS_DIR "${CMAKE_SOURCE_DIR}/target_deps")
include("${NEKO_DEPS_DIR}/package_info.cmake")

include("${NEKO_DEPS_DIR}/GenChangelog.cmake")
set(NEKO_COPYRIGHT_SRC "${NEKO_DEPS_DIR}/debian/copyright")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${NEKO_DEPS_DIR}/package_info.cmake")

if(NOT DEFINED NEKO_VERSION)
  set(NEKO_VERSION "0.0.0")
  if(EXISTS "${NEKO_CHANGELOG_SRC}")
    file(STRINGS "${NEKO_CHANGELOG_SRC}" NEKO_CHANGELOG_FIRST LIMIT_COUNT 1)
    if(NEKO_CHANGELOG_FIRST MATCHES "\\(([^)]+)\\)")
      set(NEKO_VERSION "${CMAKE_MATCH_1}")
    endif()
  endif()
endif()

set(NEKO_LINUXDEPLOY "" CACHE FILEPATH "Path to a linuxdeploy AppImage; empty means download one")
set(NEKO_APPIMAGE_ASSET_SUBDIR "usr/share/nekodrome" CACHE STRING "Directory inside the AppDir that AppImageUtils::getAssetPath resolves relative asset paths against")

set(NEKO_DIST_DIR "${CMAKE_BINARY_DIR}/dist")
set(NEKO_BIN_ASSETS "${CMAKE_BINARY_DIR}/bin/data")
set(NEKO_PKG_NAME "nekodrome-${NEKO_VERSION}-linux-${CMAKE_SYSTEM_PROCESSOR}")

file(MAKE_DIRECTORY "${NEKO_DIST_DIR}")

install(TARGETS nekodrome RUNTIME DESTINATION bin COMPONENT nekodrome)
install(DIRECTORY "${NEKOUI_DIR}/assets/" DESTINATION /opt/nekodrome COMPONENT nekodrome)
install(FILES "${CMAKE_SOURCE_DIR}/assets/logo.png" DESTINATION /opt/nekodrome COMPONENT nekodrome)
install(FILES "${CMAKE_SOURCE_DIR}/assets/app.png" RENAME nekodrome.png DESTINATION share/pixmaps COMPONENT nekodrome)
install(FILES "${NEKO_DEPS_DIR}/nekodrome.desktop" DESTINATION share/applications COMPONENT nekodrome)

if(EXISTS "${NEKO_CHANGELOG_SRC}")
  set(NEKO_CHANGELOG_GZ "${CMAKE_BINARY_DIR}/changelog.Debian.gz")
  execute_process(COMMAND gzip -9n -c "${NEKO_CHANGELOG_SRC}" OUTPUT_FILE "${NEKO_CHANGELOG_GZ}")
  install(FILES "${NEKO_CHANGELOG_GZ}" DESTINATION share/doc/nekodrome COMPONENT nekodrome)
endif()

if(EXISTS "${NEKO_COPYRIGHT_SRC}")
  install(FILES "${NEKO_COPYRIGHT_SRC}" DESTINATION share/doc/nekodrome COMPONENT nekodrome)
endif()

set(NEKO_DEB_SCRIPTS "")
foreach(script preinst postinst prerm postrm)
  if(EXISTS "${NEKO_DEPS_DIR}/debian/${script}")
    list(APPEND NEKO_DEB_SCRIPTS "${NEKO_DEPS_DIR}/debian/${script}")
  endif()
endforeach()

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_NAME nekodrome)
set(CPACK_PACKAGE_VERSION "${NEKO_VERSION}")
set(CPACK_PACKAGE_CONTACT "${NEKO_MAINTAINER}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${NEKO_SUMMARY}")
set(CPACK_PACKAGE_DESCRIPTION "${NEKO_DESCRIPTION}")
set(CPACK_PACKAGE_DIRECTORY "${NEKO_DIST_DIR}")
set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
set(CPACK_SET_DESTDIR ON)
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL nekodrome)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "${NEKO_DEB_SECTION}")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "${NEKO_DEB_PRIORITY}")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ${NEKO_DEB_AUTO_DEPENDS})
set(CPACK_DEBIAN_PACKAGE_DEPENDS "${NEKO_DEB_DEPENDS}")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "${NEKO_DEB_RECOMMENDS}")
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "${NEKO_DEB_SUGGESTS}")
if(NEKO_HOMEPAGE)
  set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${NEKO_HOMEPAGE}")
endif()
if(NEKO_DEB_SCRIPTS)
  set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${NEKO_DEB_SCRIPTS}")
endif()
include(CPack)

add_custom_target(tarball
  COMMAND ${CMAKE_COMMAND} -E remove_directory "${NEKO_DIST_DIR}/${NEKO_PKG_NAME}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${NEKO_DIST_DIR}/${NEKO_PKG_NAME}"
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:nekodrome> "${NEKO_DIST_DIR}/${NEKO_PKG_NAME}/"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${NEKO_BIN_ASSETS}" "${NEKO_DIST_DIR}/${NEKO_PKG_NAME}/data"
  COMMAND ${CMAKE_COMMAND} -E tar czf "${NEKO_PKG_NAME}.tar.gz" "${NEKO_PKG_NAME}"
  WORKING_DIRECTORY "${NEKO_DIST_DIR}"
  VERBATIM)
add_dependencies(tarball nekodrome nekodrome_assets)

add_custom_target(deb
  COMMAND ${CMAKE_CPACK_COMMAND} -G DEB --config "${CMAKE_BINARY_DIR}/CPackConfig.cmake"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  VERBATIM)
add_dependencies(deb nekodrome nekodrome_assets)

add_custom_target(appimage
  COMMAND ${CMAKE_COMMAND} -E env "LINUXDEPLOY=${NEKO_LINUXDEPLOY}" sh "${NEKO_DEPS_DIR}/build_appimage.sh"
          $<TARGET_FILE:nekodrome>
          "${NEKO_BIN_ASSETS}"
          "${NEKO_DEPS_DIR}/nekodrome.desktop"
          "${CMAKE_SOURCE_DIR}/assets/app.png"
          "${NEKO_DIST_DIR}"
          "${NEKO_APPIMAGE_ASSET_SUBDIR}"
          "${NEKO_VERSION}"
  VERBATIM)
add_dependencies(appimage nekodrome nekodrome_assets)

add_custom_target(dist)
add_dependencies(dist tarball deb appimage)
