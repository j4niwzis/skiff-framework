cmake_minimum_required(VERSION 4.3.4 FATAL_ERROR)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/import-std.cmake)
# C++23 for every library this build makes, not only for its own targets:
# splice takes the build's standard, and below 23 it is not itself.
set(CMAKE_CXX_STANDARD 23)
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
set_target_properties(skiff_framework PROPERTIES EXPORT_NAME framework CXX_STANDARD 23)
target_compile_features(skiff_framework PUBLIC cxx_std_23)
target_link_libraries(skiff_framework PUBLIC skiff::ui)

# skiff_app(target [SHELL page.html]): a program of skiff.app. Under
# Emscripten it is the page itself (index.html, .js, .wasm): what the page's
# JavaScript calls exported, the page Emscripten makes from SHELL -- which
# says @@SKIFF-PRERENDER@@ where the copy written natively goes. Natively,
# the program that writes it out.
function(skiff_app target)
  cmake_parse_arguments(APP "" "SHELL" "" ${ARGN})
  target_link_libraries(${target} PRIVATE skiff::framework)
  set_target_properties(${target} PROPERTIES CXX_STANDARD 23 CXX_SCAN_FOR_MODULES ON)
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

option(SKIFF_FRAMEWORK_SITE "Build skiff's site, a program of skiff.app" ${PROJECT_IS_TOP_LEVEL})
if(SKIFF_FRAMEWORK_SITE)
  add_subdirectory(site)
endif()
