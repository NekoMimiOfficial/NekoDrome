if(NOT DEFINED ENV{VITASDK})
  message(FATAL_ERROR "VITASDK is not set")
endif()
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(NEKO_VITA_TITLE_ID "NEMIKO001" CACHE STRING "Vita title ID")
set(NEKO_VITA_VERSION "02.00" CACHE STRING "Vita app ver")

find_package(PkgConfig REQUIRED)
pkg_check_modules(NEKO_VITA_DEPS REQUIRED sdl2 SDL2_ttf SDL2_image SDL2_mixer libcurl)

target_include_directories(nekodrome PRIVATE ${NEKO_VITA_DEPS_STATIC_INCLUDE_DIRS})
target_link_libraries(nekodrome PRIVATE
  ${NEKO_VITA_DEPS_STATIC_LDFLAGS}
  SceIme_stub SceCommonDialog_stub SceSysmodule_stub SceNet_stub SceNetCtl_stub SceRtc_stub)
target_link_options(nekodrome PRIVATE -Wl,-q)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/scripts/deploy.sh
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
     FILE_PERMISSIONS
         OWNER_READ OWNER_WRITE OWNER_EXECUTE
         GROUP_READ GROUP_EXECUTE
         WORLD_READ WORLD_EXECUTE
)

set(NEKO_VITA_SCE_SYS ${CMAKE_SOURCE_DIR}/assets/vita/sce_sys)
file(GLOB_RECURSE NEKO_VITA_SCE_FILES RELATIVE ${NEKO_VITA_SCE_SYS} CONFIGURE_DEPENDS
  ${NEKO_VITA_SCE_SYS}/*.png ${NEKO_VITA_SCE_SYS}/*.xml)
set(NEKO_VITA_SCE_ARGS)
foreach(f ${NEKO_VITA_SCE_FILES})
  list(APPEND NEKO_VITA_SCE_ARGS FILE ${NEKO_VITA_SCE_SYS}/${f} sce_sys/${f})
endforeach()

vita_create_self(nekodrome.self nekodrome)
vita_create_vpk(nekodrome.vpk ${NEKO_VITA_TITLE_ID} nekodrome.self
  VERSION ${NEKO_VITA_VERSION}
  NAME NekoDrome
  ${NEKO_VITA_SCE_ARGS}
  FILE ${CMAKE_SOURCE_DIR}/assets/logo.png assets/logo.png
  FILE ${NEKOUI_DIR}/assets/fonts/Roboto-Regular.ttf font/Roboto-Regular.ttf
  FILE ${NEKOUI_DIR}/assets/fonts/MaterialIcons-Regular.ttf font/MaterialIcons-Regular.ttf)
