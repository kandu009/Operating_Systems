#!/bin/bash

pages=( 100 ) 
frames=( 100 90 80 70 60 50 40 30 20 10 2 )
algorithms=( "rand" "fifo" "custom" )
programs=( "sort" "scan" "focus" )

echo "sum num_pages num_frames page_faults disk_reads disk_writes evictions algorithm program"
for page in "${pages[@]}"; do
	for frame in "${frames[@]}"; do
		for algo in "${algorithms[@]}"; do
			for prog in "${programs[@]}"; do
				if [ "$frame" -le "$page" ]; then
					echo $(./virtmem $page $frame $algo $prog)", "$algo", "$prog
				fi
			done
		done
	done
done
