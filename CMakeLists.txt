cmake_minimum_required(VERSION 4.3.4 FATAL_ERROR)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/import-std.cmake)
# C++26 for every library this build makes, not only for its own targets:
# splice takes the build's standard, and knot (the state a page carries)
# is C++26's.
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_EXTENSIONS OFF)

# skiff-framework: skiff, skiff-widgets and what nearly every program
# written with them does, as one library (skiff.app) -- the tree mounted on
# the page in a browser, over the copy written when the program was built;
# drawn on a canvas by skiff's scene where the program says so; written out
# as HTML natively. A program is its tree, its model's parts and what it
# adds: SKIFF_APP(page, Hooks) (include/skiff/app.h). The site (site/) is
# one such program.
#
# skiff-widgets (and skiff through it) comes from cmake-everywhere: from this
# build where it is already in it, else from the revision named below.
if(NOT COMMAND cme_declare_port AND NOT CMAKE_PROJECT_TOP_LEVEL_INCLUDES)
  set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/cmake/get_cme.cmake)
endif()

project(skiff_framework VERSION 0.1.0 LANGUAGES CXX)

# What is built: skiff.app (the page, or the page written out) and the
# window (skiff.platform.window, below). A program that only wants the
# window -- mux -- says SKIFF_FRAMEWORK_APP OFF, and nothing of skiff.app's
# own (skiff.ui, the page) is asked for.
option(SKIFF_FRAMEWORK_APP "skiff.app: the page, and the page written out" ON)

if(SKIFF_FRAMEWORK_APP)
  if(NOT TARGET skiff::ui)
    # Both backends, for skiff.ui to mount on: the page, and the scene --
    # without Skia, by default: in a browser its stand-in draws on a canvas.
    set(SKIFF_UI ON CACHE BOOL "" FORCE)
    set(SKIFF_WEB ON CACHE BOOL "" FORCE)
    # Skia where a program asks for it (-DSKIFF_SKIA=ON): writing a page out
    # needs none.
    if(NOT DEFINED SKIFF_SKIA)
      set(SKIFF_SKIA OFF CACHE BOOL "")
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
      set(SKIFF_SKIA OFF CACHE BOOL "" FORCE)
      # No threads: a page served as files (GitHub Pages) cannot say the
      # headers shared memory needs.
      set(SKIFF_EMSCRIPTEN_THREADS OFF CACHE BOOL "" FORCE)
    endif()
  endif()
  if(COMMAND cme_declare_port AND NOT TARGET skiff::ui)
    cme_declare_port(
      NAME skiff-widgets
      PROVIDES skiff-widgets skiff_widgets
      GITHUB_REPOSITORY j4niwzis/skiff-widgets
      GIT_TAG 97a7613bff8c023c55f84d369dbe83fc95412ad8
      VERSION 0.1.0
    )
  endif()
  if(NOT TARGET skiff::ui)
    find_package(skiff-widgets 0.1.0 REQUIRED)
  endif()
  # knot: the model written into the page as JSON, and read back.
  if(COMMAND cme_declare_port AND NOT TARGET knot)
    cme_declare_port(NAME knot PROVIDES knot
      GITHUB_REPOSITORY j4niwzis/knot
      GIT_TAG 56858a4b579584a85b488db99ef10687a97702d5
      LICENSE AGPL-3.0-only)
  endif()
  if(NOT TARGET knot)
    find_package(knot REQUIRED)
  endif()

  # Under Emscripten every unit -- the std module CMake makes among them -- is
  # compiled with exceptions, or the std module is not one they can load: said
  # where the build is configured (CMAKE_CXX_FLAGS), not after.
  if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten" AND NOT CMAKE_CXX_FLAGS MATCHES "-fexceptions")
    message(FATAL_ERROR "skiff-framework under Emscripten: configure with -DCMAKE_CXX_FLAGS=\"-fexceptions -mmutable-globals\"")
  endif()

  add_library(skiff_framework STATIC)
  add_library(skiff::framework ALIAS skiff_framework)
  target_sources(skiff_framework PUBLIC FILE_SET CXX_MODULES FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/app.cc)
  target_include_directories(skiff_framework PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
  set_target_properties(skiff_framework PROPERTIES EXPORT_NAME framework CXX_STANDARD 26)
  target_compile_features(skiff_framework PUBLIC cxx_std_26)
  target_link_libraries(skiff_framework PUBLIC skiff::ui knot)
endif()

# The native window: skiff's scene in a window of the system's (SDL3), drawn
# by Skia -- on the GPU through GL where there is a GL, in software into the
# window's own pixels where there is not -- with the system's input, dialogs
# and clipboard. skiff.platform.window (src/platform/), for the platforms that
# have one.
#
# Each platform is a cmake-everywhere platform feature: on for the platform a
# build is for, never asked for and never refused, and naming what that
# platform needs and nothing else -- a build for Android brings the window's
# Android pieces and SDL, one for the browser none of it. A platform not
# among them gets no window: skiff.app alone (the page, or the page written
# out).
set(skiff_framework_window_platforms linux windows macos android)
if(COMMAND cme_port_feature)
  foreach(platform IN LISTS skiff_framework_window_platforms)
    cme_port_feature(skiff-framework ${platform} SUMMARY "skiff.platform.window: a window of the system's, through SDL3" DEPENDS sdl3)
  endforeach()
  cme_port_feature(skiff-framework wasm SUMMARY "skiff.app on the page, and on a canvas")
endif()
if(COMMAND cme_platform_features)
  cme_platform_features(skiff_framework_platform)
elseif(ANDROID)
  set(skiff_framework_platform android)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set(skiff_framework_platform wasm)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(skiff_framework_platform linux)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(skiff_framework_platform macos)
elseif(WIN32)
  set(skiff_framework_platform windows)
else()
  string(TOLOWER "${CMAKE_SYSTEM_NAME}" skiff_framework_platform)
endif()
# On for a program that takes the framework (mux); off for the framework's own
# build (the site), which writes a page and needs no window -- nor SDL, nor Skia.
set(skiff_framework_window_default ON)
if(PROJECT_IS_TOP_LEVEL)
  set(skiff_framework_window_default OFF)
endif()
option(SKIFF_FRAMEWORK_WINDOW "skiff.platform.window, where this build's platform has one" ${skiff_framework_window_default})
if(SKIFF_FRAMEWORK_WINDOW AND skiff_framework_platform IN_LIST skiff_framework_window_platforms)
  find_package(SDL3 REQUIRED)
  # skiff's scene with Skia, and skiff-widgets: the program's where it has
  # them (mux), else asked for here.
  if(NOT TARGET skiff::widgets)
    set(SKIFF_SKIA ON CACHE BOOL "" FORCE)
    if(COMMAND cme_declare_port)
      cme_declare_port(
        NAME skiff-widgets
        PROVIDES skiff-widgets skiff_widgets
        GITHUB_REPOSITORY j4niwzis/skiff-widgets
        GIT_TAG 97a7613bff8c023c55f84d369dbe83fc95412ad8
        VERSION 0.1.0
      )
    endif()
    find_package(skiff-widgets 0.1.0 REQUIRED)
  endif()
  # Source overlays, by platform: for foo.cc, a foo_<platform>.cc replaces it
  # on that platform (one may name several: foo_linux_windows_macos.cc), and
  # only this platform's enter the build -- no source says #if about one.
  set(skiff_framework_overlay "_(${skiff_framework_window_platforms})(_(${skiff_framework_window_platforms}))*\\.cc$")
  file(GLOB skiff_framework_platform_all CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/*.cc")
  set(skiff_framework_platform_sources)
  foreach(source IN LISTS skiff_framework_platform_all)
    get_filename_component(name "${source}" NAME)
    if(name MATCHES "${skiff_framework_overlay}")
      if(name MATCHES "_${skiff_framework_platform}(_|\\.cc$)")
        list(APPEND skiff_framework_platform_sources "${source}")
      endif()
      continue()
    endif()
    get_filename_component(stem "${source}" NAME_WE)
    file(GLOB overlays "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/${stem}_*.cc")
    set(replaced FALSE)
    foreach(overlay IN LISTS overlays)
      get_filename_component(overlay_name "${overlay}" NAME)
      string(REGEX REPLACE "(_(${skiff_framework_window_platforms}))+\\.cc$" ".cc" common "${overlay_name}")
      if(common STREQUAL name AND overlay_name MATCHES "_${skiff_framework_platform}(_|\\.cc$)")
        set(replaced TRUE)
      endif()
    endforeach()
    if(NOT replaced)
      list(APPEND skiff_framework_platform_sources "${source}")
    endif()
  endforeach()
  add_library(skiff_framework_window STATIC)
  add_library(skiff::window ALIAS skiff_framework_window)
  target_sources(skiff_framework_window PUBLIC FILE_SET CXX_MODULES FILES ${skiff_framework_platform_sources})
  set_target_properties(skiff_framework_window PROPERTIES EXPORT_NAME window CXX_STANDARD 26)
  target_compile_features(skiff_framework_window PUBLIC cxx_std_26)
  target_link_libraries(skiff_framework_window PUBLIC SDL3::SDL3 skiff::skiff skiff::widgets splice)
  message(STATUS "skiff-framework: the window, for ${skiff_framework_platform}")
endif()

# skiff_app(target [SHELL page.html]): a program of skiff.app. Under
# Emscripten it is the page itself (index.html, .js, .wasm): what the page's
# JavaScript calls exported, the page Emscripten makes from SHELL -- which
# says @@SKIFF-PRERENDER@@ where the copy written natively goes. Natively,
# the program that writes it out.
function(skiff_app target)
  cmake_parse_arguments(APP "" "SHELL" "" ${ARGN})
  target_link_libraries(${target} PRIVATE skiff::framework)
  set_target_properties(${target} PROPERTIES CXX_STANDARD 26 CXX_SCAN_FOR_MODULES ON)
  if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set_target_properties(${target} PROPERTIES OUTPUT_NAME index SUFFIX .html)
    target_link_options(${target} PRIVATE -fexceptions
      -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_RUNTIME_METHODS=ccall
      -sEXPORTED_FUNCTIONS=_main,_skiff_web_event,_skiff_canvas_pointer,_skiff_canvas_key,_skiff_canvas_text
      -sSTACK_SIZE=4194304)
    if(APP_SHELL)
      target_link_options(${target} PRIVATE --shell-file ${APP_SHELL})
      set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS ${APP_SHELL})
    endif()
  endif()
endfunction()

# The framework's own checks, natively: the state a page carries.
if(PROJECT_IS_TOP_LEVEL AND SKIFF_FRAMEWORK_APP AND NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  enable_testing()
  add_executable(skiff_framework_state_test test/state.cc)
  set_target_properties(skiff_framework_state_test PROPERTIES CXX_STANDARD 26 CXX_SCAN_FOR_MODULES ON)
  target_link_libraries(skiff_framework_state_test PRIVATE skiff::framework)
  add_test(NAME state COMMAND skiff_framework_state_test)
endif()

option(SKIFF_FRAMEWORK_SITE "Build skiff's site, a program of skiff.app" ${PROJECT_IS_TOP_LEVEL})
if(NOT SKIFF_FRAMEWORK_APP)
  set(SKIFF_FRAMEWORK_SITE OFF)
endif()
if(SKIFF_FRAMEWORK_SITE)
  add_subdirectory(site)
endif()
