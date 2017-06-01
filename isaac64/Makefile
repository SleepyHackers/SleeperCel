CC := x86_64-linux-musl-gcc
CFLAGS := -Ofast -pipe -march=native -static -g0 -ffreestanding -fomit-frame-pointer -ffast-math
LDFLAGS := -static,-s,-nostdlib
OUT := i64

all:
	${CC} ${CFLAGS} isaac64_out.c isaac64/isaac64.c -o ${OUT} -Wl,${LDFLAGS}
	@file ${OUT}
	@du -sb ${OUT}
