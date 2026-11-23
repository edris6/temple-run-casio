CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

.PHONY: test clean
test: build/test_recovered build/test_renderer build/test_course build/test_session
	./build/test_recovered
	./build/test_renderer
	./build/test_course
	./build/test_session

build/test_recovered: src/recovered.c src/recovered.h tests/test_recovered.c
	mkdir -p build
	$(CC) $(CFLAGS) -Isrc src/recovered.c tests/test_recovered.c -lm -o $@

build/test_renderer: src/renderer.c src/renderer.h tests/test_renderer.c
	mkdir -p build
	$(CC) $(CFLAGS) -Isrc src/renderer.c tests/test_renderer.c -lm -o $@

build/test_course: src/course.c src/course.h src/recovered.c src/recovered.h tests/test_course.c
	mkdir -p build
	$(CC) $(CFLAGS) -Isrc src/course.c src/recovered.c tests/test_course.c -lm -o $@

build/test_session: src/session.c src/session.h src/course.c src/course.h src/recovered.c src/recovered.h tests/test_session.c
	mkdir -p build
	$(CC) $(CFLAGS) -Isrc src/session.c src/course.c src/recovered.c tests/test_session.c -lm -o $@

# Generate build/player.h and build/path.h from the user's Unity checkout first.
build/preview: src/preview.c src/renderer.c src/renderer.h src/course.c src/course.h src/recovered.c src/recovered.h build/player.h build/path.h build/player_tex.h build/path_tex.h build/jump.h build/slide.h build/trip.h build/coin.h build/tree_tex.h build/left_corner.h build/right_corner.h build/corner_c.h build/corner_d.h build/path_h.h build/jump_h.h build/slide_h.h build/trip_h.h
	$(CC) $(CFLAGS) -Isrc -Ibuild src/preview.c src/renderer.c src/course.c src/recovered.c -lm -o $@

clean:
	rm -f build/test_recovered build/test_renderer build/test_course build/test_session build/preview
