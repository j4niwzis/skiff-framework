# skiff's site, written with skiff: a program of skiff.app. Its tree, its
# model's parts and what it adds are page.cc; main.cc is the program,
# SKIFF_APP. Under Emscripten the page itself (index.html, .js, .wasm);
# natively, the program that writes it out (the page whole, and the markup
# put into the page Emscripten made).
add_library(site_ui STATIC)
target_sources(site_ui PUBLIC FILE_SET CXX_MODULES FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/page.cc)
set_target_properties(site_ui PROPERTIES CXX_STANDARD 23)
target_compile_features(site_ui PUBLIC cxx_std_23)
target_link_libraries(site_ui PUBLIC skiff::framework)

add_executable(site ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cc)
target_link_libraries(site PRIVATE site_ui)
skiff_app(site SHELL ${CMAKE_CURRENT_SOURCE_DIR}/web/index.html)
