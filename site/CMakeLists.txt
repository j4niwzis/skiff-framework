cmake_minimum_required(VERSION 4.3.4 FATAL_ERROR)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/import-std.cmake)
set(CMAKE_CXX_EXTENSIONS OFF)

# skiff's site, written with skiff: one tree against skiff.ui on skiff.web.
# Under Emscripten, the page itself (index.html, .js, .wasm); natively, the
# program that writes it out when the site is built (static.html, and the
# markup put into the page Emscripten made). Nothing of Skia in either.
#
# skiff-widgets (and skiff through it) comes from cmake-everywhere: from this
# build where it is already in it, else from the revision named below.
if(NOT COMMAND cme_declare_port AND NOT CMAKE_PROJECT_TOP_LEVEL_INCLUDES)
  set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/cmake/get_cme.cmake)
endif()

project(skiff_site VERSION 0.1.0 LANGUAGES CXX)

if(NOT TARGET skiff::ui)
  # The page's backend, and no scene: skiff.ui is skiff.web's names.
  set(SKIFF_UI OFF CACHE BOOL "" FORCE)
  set(SKIFF_WEB ON CACHE BOOL "" FORCE)
  set(SKIFF_UI_BACKEND web CACHE STRING "" FORCE)
  # No threads: a page served as files (GitHub Pages) cannot say the headers
  # shared memory needs.
  set(SKIFF_EMSCRIPTEN_THREADS OFF CACHE BOOL "" FORCE)
endif()
if(COMMAND cme_declare_port AND NOT TARGET skiff::ui)
  cme_declare_port(
    NAME skiff-widgets
    PROVIDES skiff-widgets skiff_widgets
    GITHUB_REPOSITORY j4niwzis/skiff-widgets
    GIT_TAG c836c3a5cb7478ec46aa97ba5140cf96c35152da
    VERSION 0.1.0
  )
endif()
if(NOT TARGET skiff::ui)
  find_package(skiff-widgets 0.1.0 REQUIRED)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set(site_emscripten_flags -fexceptions)
  string(JOIN " " site_emscripten_joined ${site_emscripten_flags})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${site_emscripten_joined}")
endif()

add_library(site_ui STATIC)
target_sources(site_ui PUBLIC FILE_SET CXX_MODULES FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/model.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/src/page.cc)
set_target_properties(site_ui PROPERTIES CXX_STANDARD 23)
target_compile_features(site_ui PUBLIC cxx_std_23)
target_link_libraries(site_ui PUBLIC skiff::ui)

add_executable(site ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cc)
target_link_libraries(site PRIVATE site_ui)
set_target_properties(site PROPERTIES CXX_STANDARD 23 CXX_SCAN_FOR_MODULES ON)
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  foreach(target site_ui site)
    target_compile_options(${target} PRIVATE ${site_emscripten_flags})
  endforeach()
  set_target_properties(site PROPERTIES OUTPUT_NAME index SUFFIX .html)
  target_link_options(site PRIVATE ${site_emscripten_flags}
    -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_RUNTIME_METHODS=ccall
    -sEXPORTED_FUNCTIONS=_main,_skiff_web_event
    -sSTACK_SIZE=4194304
    --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/index.html)
  set_property(TARGET site APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/index.html)
endif()
