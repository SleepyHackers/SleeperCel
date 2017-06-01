#!/usr/bin/env bash

declare dfn

syntax() {
	echo "Syntax: $0 <path to ioctl_out.dat>"
	exit 1
}

compile() {
	gcc -c -x c -o /tmp/.ioctl2c.o - <<< "$@"
	objcopy -j .data -I elf64-x86-64 -O binary /tmp/.ioctl2c.o ioctl_out.bin
	rm -f /tmp/.ioctl2c.o
}

getdata() {
	i=0
	dsz="$(stat -c %s "$dfn")"
	echo "#include <stdint.h>"
	echo -n 'uint8_t data[] = {';
	for byte in $(od -An -tx1 "$dfn"); do
		if [ "$i" -eq "$((dsz-1))" ]; then
			echo "0x$byte};"
		else
			echo -n "0x$byte, "
		fi
		(( i += 1 ))
	done
}

dfn="$1"
if [ -z "$dfn" -o ! -s "$dfn" ]; then
	syntax
fi

out="$(fold -w 60 <<< "$(getdata)")"

#compile "$out"

echo "$out"
