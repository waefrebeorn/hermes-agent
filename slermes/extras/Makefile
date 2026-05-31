CC       = cc
CFLAGS   = -O2 -Wall -Wextra -Wpedantic -Werror -Wno-unused-function
LDLIBS   = -lm
TARGETS  = img2svg palette asciimg morph imggrid
.PHONY: all clean test

all: $(TARGETS)

img2svg: img2svg.c common.h stb_image.h
	$(CC) $(CFLAGS) img2svg.c $(LDLIBS) -o img2svg

palette: palette.c common.h stb_image.h
	$(CC) $(CFLAGS) palette.c $(LDLIBS) -o palette

asciimg: asciimg.c common.h stb_image.h
	$(CC) $(CFLAGS) asciimg.c $(LDLIBS) -o asciimg

morph: morph.c common.h stb_image.h
	$(CC) $(CFLAGS) morph.c $(LDLIBS) -o morph

imggrid: imggrid.c stb_image.h
	$(CC) $(CFLAGS) imggrid.c $(LDLIBS) -o imggrid

test: all
	@echo "--- img2svg test ---"
	./img2svg ../assets/slermes.jpg /tmp/slermes-test.svg --eps 1.5 --crop 2>/dev/null
	wc -c /tmp/slermes-test.svg
	@echo "--- palette test ---"
	./palette ../assets/slermes.jpg 8 2>/dev/null
	@echo "--- asciimg test ---"
	./asciimg ../assets/slermes.jpg 60 std 2>/dev/null | head -20
	@echo "--- morph test ---"
	./morph ../assets/slermes.jpg edges /tmp/slermes-edges.pgm 2>/dev/null
	wc -c /tmp/slermes-edges.pgm
	@echo "--- imggrid test ---"
	./imggrid ../assets/slermes.jpg /tmp/slermes-grid-test.svg 16 20 2>/dev/null
	wc -c /tmp/slermes-grid-test.svg
	@echo "ALL EXTRAS TESTS PASSED"

clean:
	rm -f $(TARGETS) *.o
