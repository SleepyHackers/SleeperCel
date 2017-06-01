AS=i686-elf-as
CC=i686-elf-gcc
STRIP=i686-elf-strip
CFLAGS=-Os -g0 -pipe -Wall -ffreestanding -I. -nostdlib -fno-use-linker-plugin -fno-builtin -Wno-builtin-declaration-mismatch -Werror -Wno-unused-function -Wno-int-conversion -Wno-char-subscripts
SRC := readable.c kernel.c serial.c pci.c
SRC_DIR = .
OBJ_DIR := obj
DEP_DIR := dep

#End config

OBJ := ${foreach src,${SRC},${OBJ_DIR}/${patsubst %.c,%.o,${src}}}
DEP := $(patsubst %.c,%.d, $(SRC))
DEP := ${foreach dep,${DEP},${DEP_DIR}/${notdir ${dep}}}

all: ${DEP} ${OBJ} boot.o
	@echo Linking kernel
	@${CC} ${CFLAGS} -T kernel.ld -o kernel.bin ${OBJ_DIR}/boot.o ${OBJ} -lgcc
#	${CC} ${CFLAGS} -c isaac32/readable.c kernel.c -o kernel.o
#	i686-elf-gcc -O2 -Wall -march=native -I/usr/lib/gcc/i686-pc-linux-gnu/4.9.4/include/ isaac32/readable.c kernel.c -o kernel -shared
	@${STRIP} -sxXD kernel.bin
	@grub-file --is-x86-multiboot kernel.bin && echo Passed multiboot sanity test

-include ${DEP}

clean:
	rm -f ${DEP_DIR}/* ${OBJ_DIR}/* kernel.bin

boot.o: boot.s
	@echo Assembling bootsector
	@${AS} boot.s -o boot.o
	@mv boot.o ${OBJ_DIR}

${DEP_DIR}/%.d: ${SRC_DIR}/%.c
	@echo Building dependencies for $<
	@${CC} ${CFLAGS} -MM -MF $@ -MT ${OBJ_DIR}/${patsubst %.c,%.o,${notdir $<}} -c $<

${OBJ_DIR}/%.o: ${SRC_DIR}/%.c
	@echo Compiling $?
	@${CC} ${CFLAGS} -c $<
	@mv ${notdir $@} ${OBJ_DIR}

syms:
	@readelf -Ss kernel.bin

iso: kernel.bin
	cp kernel.bin iso/boot/
	cp grub.cfg iso/boot/grub/
	grub-mkrescue -o kernel.iso -d /usr/lib/grub/i386-pc iso

virt: kernel.iso
	qemu-system-i386 -machine accel=kvm -display curses -vga virtio -cdrom kernel.iso -m 512 -netdev user,id=e1 -device e1000e,id=e1 -serial telnet:127.0.0.1:41414

backup:
	backup os_attempt_two Makefile boot.s fsmalloc.c fsmalloc.h grub.cfg kernel.c kernel.ld multiboot2.s rand.h readable.c standard.h types.h serial.h serial.c pci.c
