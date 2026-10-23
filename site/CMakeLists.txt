cmake_minimum_required(VERSION 4.3.4 FATAL_ERROR)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/import-std.cmake)
# C++23 for every library this build makes, not only for the site's own
# targets: splice takes the build's standard, and below 23 it is not itself.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)

# skiff's site, written with skiff: one tree in skiff.ui, mounted on
# skiff.web (the page, laid out by the browser) and on skiff's scene (the
# same page drawn on a canvas), in one program.
# Under Emscripten, the page itself (index.html, .js, .wasm); natively, the
# program that writes it out when the site is built (static.html, and the
# markup put into the page Emscripten made). Nothing of Skia in either: the
# scene is built on skiff's stand-in for its types (SKIFF_SKIA off), which in
# a browser draws by the canvas's 2D context.
#
# skiff-widgets (and skiff through it) comes from cmake-everywhere: from this
# build where it is already in it, else from the revision named below.
if(NOT COMMAND cme_declare_port AND NOT CMAKE_PROJECT_TOP_LEVEL_INCLUDES)
  set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/cmake/get_cme.cmake)
endif()

project(skiff_site VERSION 0.1.0 LANGUAGES CXX)

if(NOT TARGET skiff::ui)
  # Both backends, for skiff.ui to mount on: the page, and the scene
  # without Skia.
  set(SKIFF_UI ON CACHE BOOL "" FORCE)
  set(SKIFF_SKIA OFF CACHE BOOL "" FORCE)
  set(SKIFF_WEB ON CACHE BOOL "" FORCE)
  # No threads: a page served as files (GitHub Pages) cannot say the headers
  # shared memory needs.
  set(SKIFF_EMSCRIPTEN_THREADS OFF CACHE BOOL "" FORCE)
endif()
if(COMMAND cme_declare_port AND NOT TARGET skiff::ui)
  cme_declare_port(
    NAME skiff-widgets
    PROVIDES skiff-widgets skiff_widgets
    GITHUB_REPOSITORY j4niwzis/skiff-widgets
    GIT_TAG 05ebdcbf43a34c41252009298729ef96d6656d4b
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
  message(FATAL_ERROR "skiff's site under Emscripten: configure with -DCMAKE_CXX_FLAGS=\"-fexceptions -mmutable-globals\"")
endif()

add_library(site_ui STATIC)
target_sources(site_ui PUBLIC FILE_SET CXX_MODULES FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/page.cc)
set_target_properties(site_ui PROPERTIES CXX_STANDARD 23)
target_compile_features(site_ui PUBLIC cxx_std_23)
target_link_libraries(site_ui PUBLIC skiff::ui)

add_executable(site ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cc)
target_link_libraries(site PRIVATE site_ui)
set_target_properties(site PROPERTIES CXX_STANDARD 23 CXX_SCAN_FOR_MODULES ON)
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set_target_properties(site PROPERTIES OUTPUT_NAME index SUFFIX .html)
  target_link_options(site PRIVATE -fexceptions
    -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_RUNTIME_METHODS=ccall
    -sEXPORTED_FUNCTIONS=_main,_skiff_web_event,_skiff_canvas_pointer,_skiff_canvas_key,_skiff_canvas_text
    -sSTACK_SIZE=4194304
    --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/index.html)
  set_property(TARGET site APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/index.html)
endif()
