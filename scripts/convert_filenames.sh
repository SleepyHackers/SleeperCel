#!/usr/bin/env bash

declare TRIAL=n
declare APPLE=n
declare ABLETON=n
declare SKIP=n
declare LOG="convert_filenames.log"

die() {
	echo ERR caught >&2
	trap ERR
	exit 1
}

syntax() {
	echo "Syntax: $0 [options] <path to sed rules file>" >&2
	printf "\t%s\t%s\n" "-l" "Logfile (path to debug logfile - *must* be specified before -d)"
	printf "\t%s\t%s\n" "-d" "Debug (logs extra output to logfile [default convert_filenames.log])"
	printf "\t%s\t%s\n" "-t" "Trial mode (doesn't actually perform any renames, most useful with -d)"
	printf "\t%s\t%s\n" "-a" "Apple mode (don't delete any of the HFS+ metadata entries)"
	printf "\t%s\t%s\n" "-A" "Ableton mode (don't delete any of the Ableton Sound Analysis [.asd] files)"
	printf "\t%s\t%s\n" "-s" "Skip path conversion (only perform Apple/Ableton deletions)"
	return 1
}

convpath() {
	trap 'die' ERR
	files="$(find . -mindepth 1 -maxdepth 1 -printf "%f\n")"
	echo -e "FILES in "$(pwd)":\n${files}" >&3
	while read path; do
		if [ ! -z "$path" -a -e "$path" ]; then
#			conv="${path##*/}"
			conv="$(sed "$srules" <<< "$path")"
			if [ -z "$conv" ]; then
				echo "Aborting on failed conversion of \"$path\"" >&2
				false
			fi
			if [ "$path" != "$conv" ]; then
				declare -i i=1
				convA="${conv%.*}"
				convB="${conv##*.}"
				while [ -e "$conv" -a -s "$conv" ]; do
					conv="${convA}_${i}.${convB}"
					((i++))
				done
				echo "*** \"$path\" -> \"$conv\" ***" >&3
				if [ "$TRIAL" = "n" ]; then
					mv -n -- "$path" "$conv"
					if [ "$?" -ne "0" ]; then
						echo "Aborting on failed mv invocation on \"$path\"" >&2
						false
					fi
				fi
			fi
		fi
	done <<< "$files"
}

trap 'die' ERR

exec 3<>/dev/null

while [ -z "${1%%-*}" ]; do
	case "$1" in
		"-d")
			>convert_filenames.log
			exec 3<>"$LOG"
			;;
		"-l")
			shift
			if [ -z "$1" ]; then
				syntax
			fi
			LOG="$1"
			;;
		"-t")
			TRIAL=y
			;;
		"-a")
			APPLE=y
			;;
		"-A")
			ABLETON=y
			;;
		"-s")
			SKIP=y
			;;
		*)
			syntax
			;;
	esac
	shift
done

if [ -z "$1" ]; then
	syntax
fi

srules="$(cat "$1")"

#The Great Apple Cleansing
if [ "$APPLE" = "n" ]; then
	echo "Removing undesireables" >&3
	find . -type f -regextype egrep -iregex "^\./[.]?.*DS_STORE$" -printf "*** rm %p ***\n" -delete >&3
	find . -regextype egrep -iregex "^\./.*__MACOSX.*" -printf "*** rm %p ***\n" -delete >&3
	find . -type f -regex "^\./.*\._.*$" -printf "*** rm %p ***\n" -delete >&3
fi

if [ "$ABLETON" = "n" ]; then
	find . -type f -regextype egrep -iregex ".*\.(asd|adg)$" -printf "*** rm %p ***\n" -delete >&3
fi

if [ "$SKIP" = "y" ]; then
	echo EXIT success
	exit 0
fi

#First filecount performed after the Apple Cleansing to not trigger a false mismatch alert
count="$(find . | wc -l)"
i=0

while [ ! -z "$(find . -mindepth $i -type d)" ]; do
	(( i+=1 ))
done
(( i-- ))

echo "Maximum of $i levels detected" >&3
echo "Beginning conversion" >&3

cwd="$(pwd)"
for ((n=i; n>0; n--)); do
	#	files="$(find . -mindepth $n -maxdepth $n -type d -print0 | xargs -0 -I {} echo "{}")"
	files="$(find . -mindepth $n -maxdepth $n -type d)"
	echo -e "Level ${n}:\n${files}\n" >&3
	while IFS= read "dir"; do
		cd "$dir"
		if [ "$?" -ne "0" ]; then
			echo "Aborting on failure to change directory to "$dir"" >&2
			false
		fi
		convpath
		cd "$cwd"
	done <<< "$files"
done

convpath

check="$(find . | wc -l)"

if [ "$check" -ne "$count" ]; then
	echo "Error: Filecount mismatch - $count entries before conversion attempt, $check entries after" >&2
	echo EXIT failure
else
	echo EXIT success
fi

trap ERR


