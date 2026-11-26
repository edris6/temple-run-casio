cmake_minimum_required(VERSION 3.15)
project(temple_native C ASM)

include(GenerateG3A)
find_package(Gint 2.1 REQUIRED)

add_executable(temple src/cg_main.c src/recovered.c src/course.c src/session.c
  src/renderer.c src/raster.c src/rows.c src/sh4.S src/present.c src/assets.c
  src/menu.c src/save.c)
target_include_directories(temple PRIVATE src build)
target_compile_options(temple PRIVATE -Wall -Wextra -O2)
target_link_libraries(temple Gint::Gint)
generate_g3a(TARGET temple OUTPUT "TRUN3D.g3a" NAME "TRunner"
    ICONS assets/icon-uns.png assets/icon-sel.png)

# Optional: compares the assembly loops with their C references on the machine.
add_executable(temple_check EXCLUDE_FROM_ALL tests/cg_check.c src/rows.c
  src/present.c src/sh4.S)
target_include_directories(temple_check PRIVATE src)
target_compile_options(temple_check PRIVATE -Wall -Wextra -O2)
target_link_libraries(temple_check Gint::Gint)
generate_g3a(TARGET temple_check OUTPUT "TRCHECK.g3a" NAME "TRcheck"
    ICONS assets/icon-uns.png assets/icon-sel.png)
