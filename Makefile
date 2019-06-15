CC	?= cc
CFLAGS	 = `pkg-config --cflags fuse x11`
LDFLAGS	 = `pkg-config --libs   fuse x11`

.PHONY: all clean

all: xfuse xclip_test

xfuse: xfuse.c
	${CC} ${CFLAGS} xfuse.c -o xfuse ${LDFLAGS}

xclip_test: xclip_test.c
	${CC} `pkg-config --libs --cflags x11` xclip_test.c -o xclip_test

clean:
	rm -f xfuse xclip_test