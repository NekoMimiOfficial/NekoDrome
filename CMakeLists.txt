cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(nekodrome)
include("${VITASDK}/share/vita.cmake" REQUIRED)

find_package(SDL2 REQUIRED)

set(VITA_APP_NAME "NekoDrome")
set(VITA_TITLEID  "NEMIKO001")
set(VITA_VERSION  "01.01")

set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d PARENTAL_LEVEL=1")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

include_directories(
  ../common
)

add_executable(${PROJECT_NAME}
  ./src/main.cpp
)

target_link_libraries(${PROJECT_NAME}
  SDL2_mixer
  SDL2_image
  SDL2_ttf
  SDL2
  curl
  jansson
  modplug
  opusfile
  mpg123
  xmp
  ssl
  crypto
  jpeg
  vorbisfile
  vorbis
  ogg
  opus
  freetype
  webpdemux
  webp
  png
  bz2
  SceAudio_stub
  SceDisplay_stub
  SceCtrl_stub
  ScePower_stub
  SceNet_stub
  SceNetCtl_stub
  SceIme_stub
  SceCommonDialog_stub
  SceSysmodule_stub
  SceHttp_stub
  SceSsl_stub
  pthread
  ${SDL2_LIBRARIES}
  z
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})
vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE sce_sys/icon0.png sce_sys/icon0.png
  FILE sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
  FILE sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
  FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
  FILE data/font.ttf data/font.ttf
)
