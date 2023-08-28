#!/bin/bash
POLICIES=("FIFO" "LRU" "OPT" "UNOPT" "RAND" "CLOCK")

for policy in "${POLICIES[@]}"
do
    for i in 1 2 3 4
    do
    # here -f read the page numbers (the VPN->PFN is one-to-one map, so using the vpn.txt is enough).
        ./paging-policy.py -c -f ./vpn.txt -p "$policy" -C "$i"
    done
    echo ""
done
