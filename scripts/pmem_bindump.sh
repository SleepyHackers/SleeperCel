#!/usr/bin/env bash

if [ -z "$1" ]; then
	echo "Syntax: $0 <path to pmem.db>"
	exit 1
fi

od -Ax -t x1 -w 16 /tmp/1000/pmem.db | grep --color=always -E "(01|02|03|04|17)" | less -R
