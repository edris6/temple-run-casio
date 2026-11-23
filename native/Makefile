CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

.PHONY: test clean
test: build/test_recovered build/test_renderer
	./build/test_recovered
	./build/test_renderer

build/test_recovered: src/recovered.c src/recovered.h tests/test_recovered.c
	mkdir -p build
	$(CC) $(CFLAGS) -Isrc src/recovered.c tests/test_recovered.c -lm -o $@

build/test_renderer: src/renderer.c src/renderer.h tests/test_renderer.c
	mkdir -p build
	$(CC) $(CFLAGS) -Isrc src/renderer.c tests/test_renderer.c -lm -o $@

# Generate build/player.h and build/path.h from the user's Unity checkout first.
build/preview: src/preview.c src/renderer.c src/renderer.h build/player.h build/path.h build/player_tex.h build/path_tex.h
	$(CC) $(CFLAGS) -Isrc -Ibuild src/preview.c src/renderer.c -lm -o $@

clean:
	rm -f build/test_recovered build/test_renderer build/preview
