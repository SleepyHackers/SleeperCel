#!/usr/bin/env bash
######################################################################
<< "**END DOCS**"
/------ pmem packet --------\
| 0x01 [SD] Start of record |
| 0x02 [SC] Start of cell   |
| 0x03 [EC] End of cell     |
| ...       [more cells]    |
| 0x17 [ER] End of row      |
| ...       [more rows]     |
| 0x04 [ED] End of record   |
\---------------------------/

Each pmem.sh run is associated with one unique record in the database.
No "start of row" CC, row position is implied by the presence of "end of row"
CCs.

Contents of all cells are to be base64 encoded.
First row of every record must be of format:
  - seconds since epoch when this run of pwmem started
  - Total number of PIDs in this record 
All further rows in the same record must be:
  - PID 
  - Total memory used by PID
  - exdata cells...

**END DOCS**
######################################################################

declare pid
declare mem

nexdata=10                 #Number of extra data entries (-o flags passed to ps) to parse
pmemdb="/tmp/1000/pmem.db" #Path to database file

die() {
	trap - ERR
	echo
	echo ERR caught >&2
	exit 1
}
trap 'die' ERR #it looks ugly up here, but it's always best to call trap as early as possible 

#Clear db - **** make this an option ****
#>pmemdb

exec 3>>"$pmemdb"

#Count the number of pids we'll be working with to push in the record header row
pids="$(ps hax -o pid)"
for pid in $pids; do
	(( pcount += 1 ))
done

#Push start of record and initial header row
#printf $'\001\002%s\003\002%s\003\027' "$(base64 <<< $(date +%s))" "$(base64 <<< $pcount)" >&3
row=$'\x01\x02'"$(base64 <<< $(date +%s))"'\x03\x02'"$(base64 <<< $pcount)"$'\x03\x17' 
printf "$row" >&3

for pid in $pids; do
	if [ -z "$pid" ]; then
		break
	fi

	echo -n "."
	# Unfortunately this DOES seem to be the fastest and cleanest solution
	# Bash arithmetic and string mangling would be very extensive and ugly
	# for very little gain
	mem="$(pmap -x -p "$pid" | tail -n 1 | awk '{ print $2 + $3 + $4 }')"
	if [[ "$mem" > "0" ]]; then
		# Retrieve and store extra information for processes
		# Change nexdata to reflect new number of -o flags if modified
		exdata="$(ps h -o lxc,netns,mntns,uid,gid,nlwp,ni,wchan,ignored,comm $pid)"
		exdata="${exdata//  /}"
		# We use this hack to have consistent date formats in the database (Epoch time)
		# ps -o lstart has inflexible human-readable formatting full of spaces
		lstart="$(stat -c %Y /proc/$$)"
		
		exdata_pkt=("$lstart")
		
		# This loop makes sense as it cuts out MANY pipes to sed 
		for ((i=1; i<nexdata; i++)); do
#			while [ -z "${exdata##* }" ]; do
#				exdata="${exdata% *}"
#			done
			exdata_pkt[$i]="${exdata##* }"
			exdata="${exdata% *}"
		done
		
		#Build the row, starting with the two mandatory fields - then, incrementally tack on the exdata elements
		row=$'\x02'"$(base64 <<< ${pid})"$'\x03\x02'"$(base64 <<< ${mem})"$'\x03' 
		for ((i=0; i<nexdata; i++)); do
			row="${row}"$'\x02'"$(base64 <<< ${exdata_pkt[i]})"$'\x03'
		done

		#Push end of row
		row="${row}"$'\x17'
		
		printf "$row" >&3
	fi
done

#Push end of record and close DB FD
printf $'\x04' >&3
#printf "\004" >&3
exec 3<&-

echo
echo "EXIT" success
