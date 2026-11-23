cmake_minimum_required(VERSION 3.15)
project(temple_native C)

include(GenerateG3A)
find_package(Gint 2.1 REQUIRED)

add_executable(temple src/cg_main.c src/recovered.c src/course.c src/session.c src/renderer.c)
target_include_directories(temple PRIVATE src build)
target_compile_options(temple PRIVATE -Wall -Wextra -Os)
target_link_libraries(temple Gint::Gint)
generate_g3a(TARGET temple OUTPUT "TRUN3D.g3a" NAME "TRunner"
    ICONS assets/icon-uns.png assets/icon-sel.png)
