#!/usr/bin/env bash

FBLUE='\e[38;5;225m'
FYELLOW='\e[38;5;229m'
RST='\e[0;m'
pipe="/tmp/${RANDOM}.pipe"
# ** end config **
off=0

if [ -z "$1" ] || [ -z "$2" ]; then
	echo "Syntax: $0 <start> <end>" >&2
	echo -e "\tPositions refer to output line #" >&2
	exit 255
fi

START="$1"
END="$2"

define(){ IFS='\n' read -r -d '' ${1} || true; }

die() {
	trap - EXIT ERR INT TERM KILL
	rm -f "$pipe"
	kill -9 %% &>/dev/null #Clean up loose threads
	echo Signal caught >&2
	exit 0
}
trap 'die' EXIT ERR INT TERM KILL

#offstr() {
#	printf "${FBLUE}@0x%X${RST}" $off
#}

opcode() {
	printf "<${FYELLOW}%s${RST}>${FBLUE}@0x%X${RST}" $1 $off
}

#Meant for forked shells
stage1() {
	off=0

	exec 3<"/tmp/1000/pmem.db"
	exec 1>"$pipe"
	while read -N 1 -u 3 c; do
		case $c in
			$'\x01')			
				printf '%s\n' $(opcode SD)
				;;
			$'\x02')
				printf '%s ' $(opcode SC)
				;;
			$'\x03')
				printf ' %s\n' $(opcode EC)
				;;
			$'\x04')
				printf '%s\n' $(opcode ED)
				;;
			$'\x17')
				printf '%s\n' $(opcode ER)
				;;
			*)
				echo -n $c
				;;
		esac
		(( off += 1 ))
	done
	exec 1<&-
	exec 3<&-
	trap - EXIT
}

define _trisplit <<"** EOF **"
	xC="${line##* }"
	xP="${line% *}"
	xB="${xP#* }"
	xA="${xP% *}"
** EOF **

shopt -s expand_aliases
unalias -a

alias trisplit='eval $_trisplit'

mkfifo "$pipe"
echo @@ stage1 >&2
stage1 &

echo @@ opening pipe for reading >&2
exec 4<"$pipe"

echo @@ reading into buffer in 4k chunks >&2
#msg=()
msg=""
chunk=0 
buf=1

trap - ERR #Relying on read to return 1
while [ ! -z "$buf" ]; do 
	read -r -N 4096 -u 4 buf
	ret=$?
	if [ $ret -gt 1 ]; then 
		echo "@@ -- read returned error code $ret"   >&2
		echo "@@ -- partial data retrieved (if any)" >&2
		echo "$buf"                                  >&2
		echo "@@ ----------------------------------" >&2
		exit
	fi
#	msg[chunk]="$buf"
	msg="$msg$buf"
	echo @@ -- $chunk >&2
	(( chunk += 1 ))
	if [ $ret -eq 1 ]; then
		break
	fi
done
trap 'die' ERR
unset buf

echo @@ data retrieved >&2
exec 4<&-

i=0 #DEBUG

#echo -e "${msg[*]}" > /tmp/out

while read line; do
	xC="${line##* }"
	xP="${line% *}"
	xB="${xP#* }"
	xA="${xP% *}"

	if [ "$i" -lt "$START" ]; then
		<<<nop
	elif [ "$i" -ge "$END" ]; then
		<<<nop
	elif [ "$xA" = "$line" ]; then
		printf '%s\n' "$line"
	else
#		echo "@@ line: <$line>"
		printf '%-48s %-48s %s\n' "$xA" "$(base64 -d <<< "${xB// /}")" "$xC"
	fi

	(( i += 1 ))           
done <<< "$msg"
#done <<< "${msg[*]}"

echo @@ debug exit >&2
exit
