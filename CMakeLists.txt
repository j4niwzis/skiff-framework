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
set(skiff_framework_has_window OFF)
if(SKIFF_FRAMEWORK_WINDOW AND skiff_framework_platform IN_LIST skiff_framework_window_platforms)
  set(skiff_framework_has_window ON)
endif()

option(SKIFF_FRAMEWORK_APP "skiff.app: the page, and the page written out" ON)

if(SKIFF_FRAMEWORK_APP)
  # The canvas -- the whole tree drawn by skiff's scene instead of the page's
  # elements (skiff.app.canvas) -- is a feature: a frontend has no need of
  # one, and without it skiff's scene is not built at all (but where the
  # window needs it). Asked for as the feature (COMPONENTS canvas), or by
  # hand (SKIFF_FRAMEWORK_CANVAS); the framework's own build (the site) has it.
  if(COMMAND cme_port_feature)
    cme_port_feature(skiff-framework canvas SUMMARY "skiff.app.canvas: the tree drawn by skiff's scene on a canvas, instead of the page")
  endif()
  set(skiff_framework_canvas_default ${PROJECT_IS_TOP_LEVEL})
  if(DEFINED CME_FEATURES_skiff-framework AND "canvas" IN_LIST CME_FEATURES_skiff-framework)
    set(skiff_framework_canvas_default ON)
  endif()
  option(SKIFF_FRAMEWORK_CANVAS "skiff.app with the canvas: the tree drawn by skiff's scene, where the program says so" ${skiff_framework_canvas_default})
  if(SKIFF_FRAMEWORK_CANVAS)
    set(skiff_framework_canvas ${CMAKE_CURRENT_SOURCE_DIR}/src/canvas.cc)
  else()
    set(skiff_framework_canvas ${CMAKE_CURRENT_SOURCE_DIR}/src/canvas_off.cc)
  endif()
  if(NOT TARGET skiff::ui)
    # The backends skiff.ui mounts on: the page, and the scene where there is
    # a canvas or a window -- without Skia, by default: in a browser its
    # stand-in draws on the canvas.
    if(SKIFF_FRAMEWORK_CANVAS OR skiff_framework_has_window)
      set(SKIFF_UI ON CACHE BOOL "" FORCE)
    else()
      set(SKIFF_UI OFF CACHE BOOL "" FORCE)
    endif()
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
  # The network -- what a program asks of it, declared in its model and run
  # by the framework, in the browser and natively (skiff.app.net) -- is a
  # feature: it reads answers with knot, and natively (served) needs OpenSSL.
  # Asked for as the feature (COMPONENTS net), or by hand
  # (SKIFF_FRAMEWORK_NET); the framework's own build has it.
  if(COMMAND cme_port_feature)
    cme_port_feature(skiff-framework net SUMMARY "skiff.app.net: requests declared in the model, run with retries, in the browser and natively" DEPENDS knot)
  endif()
  set(skiff_framework_net_default ${PROJECT_IS_TOP_LEVEL})
  if(DEFINED CME_FEATURES_skiff-framework AND "net" IN_LIST CME_FEATURES_skiff-framework)
    set(skiff_framework_net_default ON)
  endif()
  option(SKIFF_FRAMEWORK_NET "skiff.app.net: requests declared in the model, run with retries" ${skiff_framework_net_default})
  if(SKIFF_FRAMEWORK_NET)
    set(skiff_framework_net ${CMAKE_CURRENT_SOURCE_DIR}/src/net.cc)
  else()
    set(skiff_framework_net ${CMAKE_CURRENT_SOURCE_DIR}/src/net_off.cc)
  endif()

  # Storage -- parts of the model kept in the browser (skiff.app.store),
  # through knot -- is a feature as well; the framework's own build has it.
  if(COMMAND cme_port_feature)
    cme_port_feature(skiff-framework store SUMMARY "skiff.app.store: parts of the model kept in the browser's storage" DEPENDS knot)
  endif()
  set(skiff_framework_store_default ${PROJECT_IS_TOP_LEVEL})
  if(DEFINED CME_FEATURES_skiff-framework AND "store" IN_LIST CME_FEATURES_skiff-framework)
    set(skiff_framework_store_default ON)
  endif()
  option(SKIFF_FRAMEWORK_STORE "skiff.app.store: parts of the model kept in the browser's storage" ${skiff_framework_store_default})
  if(SKIFF_FRAMEWORK_STORE)
    set(skiff_framework_store ${CMAKE_CURRENT_SOURCE_DIR}/src/store.cc)
  else()
    set(skiff_framework_store ${CMAKE_CURRENT_SOURCE_DIR}/src/store_off.cc)
  endif()

  # Live -- the page served by the program itself, the tree and a model for
  # each browser kept on the server and the page a stream appended to as it
  # moves (skiff.app.serve) -- is a feature too: it needs Boost (Asio, Beast)
  # and a program that is only written out or only a page asks for none of
  # it. Asked for as the feature (COMPONENTS live), or by hand
  # (SKIFF_FRAMEWORK_LIVE); the framework's own build has it. Natively only.
  if(COMMAND cme_port_feature)
    cme_port_feature(skiff-framework live SUMMARY "skiff.app.serve: the page served live, its tree kept on the server, with no script")
  endif()
  set(skiff_framework_live_default ${PROJECT_IS_TOP_LEVEL})
  if(DEFINED CME_FEATURES_skiff-framework AND "live" IN_LIST CME_FEATURES_skiff-framework)
    set(skiff_framework_live_default ON)
  endif()
  option(SKIFF_FRAMEWORK_LIVE "skiff.app served live (program serve [port]), through Boost.Beast" ${skiff_framework_live_default})
  set(skiff_framework_serve ${CMAKE_CURRENT_SOURCE_DIR}/src/serve_off.cc)
  if(SKIFF_FRAMEWORK_LIVE AND NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    if(NOT TARGET Boost::beast)
      if(COMMAND cme_find_package)
        # Asked before skiff is (Boost's PFR, as a module): one ask of
        # Boost with all of it, or cmake-everywhere asks for a second run.
        cme_find_package(Boost COMPONENTS asio beast pfr[modules] REQUIRED)
      else()
        find_package(Boost REQUIRED COMPONENTS asio beast)
      endif()
    endif()
    set(skiff_framework_serve ${CMAKE_CURRENT_SOURCE_DIR}/src/serve.cc)
    set(skiff_framework_serves ON)
    # The network asked from the server: Beast, secured by OpenSSL.
    if(SKIFF_FRAMEWORK_NET)
      if(NOT TARGET OpenSSL::SSL)
        if(COMMAND cme_find_package)
          cme_find_package(OpenSSL REQUIRED)
        else()
          find_package(OpenSSL REQUIRED)
        endif()
      endif()
      set(skiff_framework_serve ${skiff_framework_serve} ${CMAKE_CURRENT_SOURCE_DIR}/src/net_native.cc)
      set(skiff_framework_net_secured ON)
    else()
      set(skiff_framework_serve ${skiff_framework_serve} ${CMAKE_CURRENT_SOURCE_DIR}/src/net_native_off.cc)
    endif()
  endif()

  if(COMMAND cme_declare_port AND NOT TARGET skiff::ui)
    cme_declare_port(
      NAME skiff-widgets
      PROVIDES skiff-widgets skiff_widgets
      GITHUB_REPOSITORY j4niwzis/skiff-widgets
      GIT_TAG bfc219e13a8af3fd6bb49e359a4840876e1c8e0d
      VERSION 0.1.0
    )
  endif()
  if(NOT TARGET skiff::ui)
    find_package(skiff-widgets 0.1.0 REQUIRED)
  endif()
  # SSR -- the page written out with the model it was written with, read
  # back in the browser (skiff.app.carry), and what is to come of it -- is a
  # feature: a program that is a page in the browser alone (a frontend, no
  # server, nothing written out) asks for none of it, and needs nothing of
  # knot. Asked for as the feature (COMPONENTS ssr), or by hand
  # (SKIFF_FRAMEWORK_SSR); the framework's own build (the site) has it.
  if(COMMAND cme_port_feature)
    cme_port_feature(skiff-framework ssr SUMMARY "server-side rendering: the model travels with the page, through knot" DEPENDS knot)
  endif()
  set(skiff_framework_ssr_default ${PROJECT_IS_TOP_LEVEL})
  if(DEFINED CME_FEATURES_skiff-framework AND "ssr" IN_LIST CME_FEATURES_skiff-framework)
    set(skiff_framework_ssr_default ON)
  endif()
  option(SKIFF_FRAMEWORK_SSR "skiff.app with server-side rendering: the model written into the page, through knot" ${skiff_framework_ssr_default})
  if(SKIFF_FRAMEWORK_SSR OR SKIFF_FRAMEWORK_NET OR SKIFF_FRAMEWORK_STORE)
    if(COMMAND cme_declare_port AND NOT TARGET knot)
      cme_declare_port(NAME knot PROVIDES knot
        GITHUB_REPOSITORY j4niwzis/knot
        GIT_TAG 56858a4b579584a85b488db99ef10687a97702d5
        LICENSE AGPL-3.0-only)
    endif()
    if(NOT TARGET knot)
      find_package(knot REQUIRED)
    endif()
  endif()
  if(SKIFF_FRAMEWORK_SSR)
    set(skiff_framework_carry ${CMAKE_CURRENT_SOURCE_DIR}/src/carry_ssr.cc)
  else()
    set(skiff_framework_carry ${CMAKE_CURRENT_SOURCE_DIR}/src/carry.cc)
  endif()

  # Under Emscripten every unit -- the std module CMake makes among them -- is
  # compiled with exceptions, or the std module is not one they can load: said
  # where the build is configured (CMAKE_CXX_FLAGS), not after.
  if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten" AND NOT CMAKE_CXX_FLAGS MATCHES "-fexceptions")
    message(FATAL_ERROR "skiff-framework under Emscripten: configure with -DCMAKE_CXX_FLAGS=\"-fexceptions -mmutable-globals\"")
  endif()

  add_library(skiff_framework STATIC)
  add_library(skiff::framework ALIAS skiff_framework)
  target_sources(skiff_framework PUBLIC FILE_SET CXX_MODULES FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/app.cc ${CMAKE_CURRENT_SOURCE_DIR}/src/route.cc ${CMAKE_CURRENT_SOURCE_DIR}/src/timer.cc ${skiff_framework_store} ${skiff_framework_carry} ${skiff_framework_canvas} ${skiff_framework_net} ${skiff_framework_serve})
  target_include_directories(skiff_framework PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
  set_target_properties(skiff_framework PROPERTIES EXPORT_NAME framework CXX_STANDARD 26)
  target_compile_features(skiff_framework PUBLIC cxx_std_26)
  target_link_libraries(skiff_framework PUBLIC skiff::ui)
  if(SKIFF_FRAMEWORK_SSR OR SKIFF_FRAMEWORK_NET OR SKIFF_FRAMEWORK_STORE)
    target_link_libraries(skiff_framework PUBLIC knot)
  endif()
  if(skiff_framework_net_secured)
    target_link_libraries(skiff_framework PUBLIC OpenSSL::SSL OpenSSL::Crypto)
  endif()
  if(skiff_framework_serves)
    target_link_libraries(skiff_framework PUBLIC Boost::asio Boost::beast)
  endif()
endif()

if(skiff_framework_has_window)
  find_package(SDL3 REQUIRED)
  # chevron: the clipboard writes what it offers as HTML with its escaping
  # (chevron.escape). The program's where it has it (mux), else asked for.
  if(NOT TARGET chevron)
    if(COMMAND cme_declare_port)
      cme_declare_port(NAME chevron PROVIDES chevron
        GITHUB_REPOSITORY j4niwzis/chevron
        GIT_TAG 8f40c545f0120747b3bf2ad6a61b420505e1249b
        VERSION 0.2
        LICENSE AGPL-3.0-only
        TARGETS chevron)
      cme_options(chevron "CHEVRON_TESTS OFF")
    endif()
    find_package(chevron 0.2 REQUIRED)
  endif()
  # skiff's scene with Skia, and skiff-widgets: the program's where it has
  # them (mux), else asked for here.
  if(NOT TARGET skiff::widgets)
    set(SKIFF_SKIA ON CACHE BOOL "" FORCE)
    if(COMMAND cme_declare_port)
      cme_declare_port(
        NAME skiff-widgets
        PROVIDES skiff-widgets skiff_widgets
        GITHUB_REPOSITORY j4niwzis/skiff-widgets
        GIT_TAG bfc219e13a8af3fd6bb49e359a4840876e1c8e0d
        VERSION 0.1.0
      )
    endif()
    find_package(skiff-widgets 0.1.0 REQUIRED)
  endif()
  # Source overlays, by platform: for foo.cc, a foo_<platform>.cc replaces it
  # on that platform (one may name several: foo_linux_windows_macos.cc), and
  # only this platform's enter the build -- no source says #if about one.
  list(JOIN skiff_framework_window_platforms "|" skiff_framework_platform_names)
  set(skiff_framework_overlay "_(${skiff_framework_platform_names})(_(${skiff_framework_platform_names}))*\\.cc$")
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
      string(REGEX REPLACE "(_(${skiff_framework_platform_names}))+\\.cc$" ".cc" common "${overlay_name}")
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
  target_link_libraries(skiff_framework_window PUBLIC SDL3::SDL3 skiff::skiff skiff::widgets splice chevron)
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
    # What the page's JavaScript calls: its events, and the canvas's where
    # there is one.
    set(exported _main,_malloc,_free,_skiff_web_event,_skiff_loopback_open,_skiff_loopback_begin,_skiff_loopback_next,_skiff_loopback_post,_skiff_loopback_close,_skiff_app_wake,_skiff_app_route,_skiff_app_route_known)
    if(SKIFF_FRAMEWORK_CANVAS)
      string(APPEND exported ,_skiff_canvas_pointer,_skiff_canvas_key,_skiff_canvas_text)
    endif()
    target_link_options(${target} PRIVATE -fexceptions
      -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_RUNTIME_METHODS=ccall,UTF8ToString
      -sEXPORTED_FUNCTIONS=${exported}
      -sSTACK_SIZE=4194304)
    if(APP_SHELL)
      target_link_options(${target} PRIVATE --shell-file ${APP_SHELL})
      set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS ${APP_SHELL})
    endif()
  endif()
endfunction()

# skiff_app_client(target): the program built for a page served live -- its
# parts that are the browser's own alone (SKIFF_APP_CLIENT): client.js and
# client.wasm under Emscripten; nothing natively.
function(skiff_app_client target)
  target_link_libraries(${target} PRIVATE skiff::framework)
  set_target_properties(${target} PROPERTIES CXX_STANDARD 26 CXX_SCAN_FOR_MODULES ON)
  if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set_target_properties(${target} PROPERTIES OUTPUT_NAME client SUFFIX .js)
    target_link_options(${target} PRIVATE -fexceptions
      -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_RUNTIME_METHODS=ccall,UTF8ToString
      -sEXPORTED_FUNCTIONS=_main,_malloc,_free,_skiff_web_event,_skiff_app_wake
      -sSTACK_SIZE=1048576)
  endif()
endfunction()

# The framework's own checks, natively: the state a page carries.
if(PROJECT_IS_TOP_LEVEL AND SKIFF_FRAMEWORK_APP AND SKIFF_FRAMEWORK_SSR AND NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  enable_testing()
  add_executable(skiff_framework_state_test test/state.cc)
  set_target_properties(skiff_framework_state_test PROPERTIES CXX_STANDARD 26 CXX_SCAN_FOR_MODULES ON)
  target_link_libraries(skiff_framework_state_test PRIVATE skiff::framework)
  add_test(NAME state COMMAND skiff_framework_state_test)
endif()

# Routes read from paths and kept with the model (test/route.cc).
if(PROJECT_IS_TOP_LEVEL AND SKIFF_FRAMEWORK_APP AND NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  enable_testing()
  add_executable(skiff_framework_route_test test/route.cc)
  set_target_properties(skiff_framework_route_test PROPERTIES CXX_STANDARD 26 CXX_SCAN_FOR_MODULES ON)
  target_link_libraries(skiff_framework_route_test PRIVATE skiff::framework)
  add_test(NAME route COMMAND skiff_framework_route_test)
  add_executable(skiff_framework_timer_test test/timer.cc)
  set_target_properties(skiff_framework_timer_test PROPERTIES CXX_STANDARD 26 CXX_SCAN_FOR_MODULES ON)
  target_link_libraries(skiff_framework_timer_test PRIVATE skiff::framework)
  add_test(NAME timer COMMAND skiff_framework_timer_test)
  add_executable(skiff_framework_switch_test test/switch.cc)
  set_target_properties(skiff_framework_switch_test PROPERTIES CXX_STANDARD 26 CXX_SCAN_FOR_MODULES ON)
  target_link_libraries(skiff_framework_switch_test PRIVATE skiff::framework)
  add_test(NAME switch COMMAND skiff_framework_switch_test)
  add_executable(skiff_framework_todo_live_test test/todo_live.cc)
  set_target_properties(skiff_framework_todo_live_test PROPERTIES CXX_STANDARD 26 CXX_SCAN_FOR_MODULES ON)
  target_link_libraries(skiff_framework_todo_live_test PRIVATE skiff::framework)
  add_test(NAME todo_live COMMAND skiff_framework_todo_live_test)
endif()

# The network's runner, with a transport of the test's own (test/net.cc).
if(PROJECT_IS_TOP_LEVEL AND SKIFF_FRAMEWORK_APP AND SKIFF_FRAMEWORK_NET AND NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  enable_testing()
  add_executable(skiff_framework_net_test test/net.cc)
  set_target_properties(skiff_framework_net_test PROPERTIES CXX_STANDARD 26 CXX_SCAN_FOR_MODULES ON)
  target_link_libraries(skiff_framework_net_test PRIVATE skiff::framework)
  add_test(NAME net COMMAND skiff_framework_net_test)
endif()
# And natively, through Beast, against a server of the test's own.
if(PROJECT_IS_TOP_LEVEL AND skiff_framework_net_secured)
  add_executable(skiff_framework_net_native_test test/net_native_main.cc)
  target_sources(skiff_framework_net_native_test PRIVATE FILE_SET CXX_MODULES FILES test/net_native.cc)
  set_target_properties(skiff_framework_net_native_test PROPERTIES CXX_STANDARD 26 CXX_SCAN_FOR_MODULES ON)
  target_link_libraries(skiff_framework_net_native_test PRIVATE skiff::framework)
  add_test(NAME net_native COMMAND skiff_framework_net_native_test)
endif()

# A page alone (test/page.cc): words and a canvas of the page's own, with or
# without the canvas feature -- what a frontend builds.
if(PROJECT_IS_TOP_LEVEL AND SKIFF_FRAMEWORK_APP AND NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  enable_testing()
  add_executable(skiff_framework_page_test test/page.cc)
  skiff_app(skiff_framework_page_test)
  add_test(NAME page COMMAND skiff_framework_page_test)
  set_tests_properties(page PROPERTIES PASS_REGULAR_EXPRESSION "A page, and a canvas in it.*<canvas class=\"skiff-canvas\"")
endif()

option(SKIFF_FRAMEWORK_SITE "Build skiff's site, a program of skiff.app" ${PROJECT_IS_TOP_LEVEL})
# The site is a canvas's too (its scene tab): built where there is one.
if(NOT SKIFF_FRAMEWORK_APP OR NOT SKIFF_FRAMEWORK_CANVAS)
  set(SKIFF_FRAMEWORK_SITE OFF)
endif()
if(SKIFF_FRAMEWORK_SITE)
  add_subdirectory(site)
endif()
