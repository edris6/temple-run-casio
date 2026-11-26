CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

GAME = src/session.c src/course.c src/recovered.c
GAME_DEPS = $(GAME) src/session.h src/course.h src/recovered.h
RASTER = src/raster.c src/rows.c
RASTER_DEPS = $(RASTER) src/raster.h src/rows.h src/reciprocal.h
ASSET_HEADERS = $(addprefix build/,$(addsuffix .h, player path path_h \
	left_corner right_corner corner_c corner_d hole hole_h narrow narrow_h \
	jump jump_h slide slide_h trip trip_h coin player_tex path_tex tree_tex))

.PHONY: test clean
test: build/test_recovered build/test_renderer build/test_raster build/test_course build/test_session
	./build/test_recovered
	./build/test_renderer
	./build/test_raster
	./build/test_course
	./build/test_session

build/test_recovered: src/recovered.c src/recovered.h tests/test_recovered.c
	mkdir -p build
	$(CC) $(CFLAGS) -Isrc src/recovered.c tests/test_recovered.c -lm -o $@

# test_renderer includes renderer.c to reach its private functions.
build/test_renderer: src/renderer.c src/renderer.h src/course.h $(RASTER_DEPS) tests/test_renderer.c
	mkdir -p build
	$(CC) $(CFLAGS) -Isrc $(RASTER) tests/test_renderer.c -lm -o $@

build/test_raster: $(RASTER_DEPS) src/present.c src/present.h tests/test_raster.c
	mkdir -p build
	$(CC) $(CFLAGS) -Isrc $(RASTER) src/present.c tests/test_raster.c -lm -o $@

build/test_course: src/course.c src/course.h src/recovered.c src/recovered.h tests/test_course.c
	mkdir -p build
	$(CC) $(CFLAGS) -Isrc src/course.c src/recovered.c tests/test_course.c -lm -o $@

build/test_session: $(GAME_DEPS) tests/test_session.c
	mkdir -p build
	$(CC) $(CFLAGS) -Isrc $(GAME) tests/test_session.c -lm -o $@

# Generate the asset headers from the user's Unity checkout first.
build/preview: src/preview.c src/assets.c src/assets.h src/renderer.c src/renderer.h \
		$(RASTER_DEPS) $(GAME_DEPS) $(ASSET_HEADERS)
	$(CC) $(CFLAGS) -Isrc -Ibuild src/preview.c src/assets.c src/renderer.c \
		$(RASTER) $(GAME) -lm -o $@

clean:
	rm -f build/test_recovered build/test_renderer build/test_raster build/test_course build/test_session build/preview
