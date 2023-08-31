#!/bin/bash
POLICIES=("FIFO" "LRU" "OPT" "UNOPT" "RAND" "CLOCK")

# original file too long; `wc -l vpn.txt` -> `844509 vpn.txt`
TARGET_LINE=10
sed -e "1,${TARGET_LINE}p" ./vpn.txt > ./vpn_minimal.txt
for policy in "${POLICIES[@]}"
do
    for i in 1 2 3 4
    do
    # here -f read the page numbers (the VPN->PFN is one-to-one map, so using the vpn.txt is enough).
        echo "$i ${POLICIES}"
        ./paging-policy.py -c -f ./vpn_minimal.txt -p "$policy" -C "$i" | grep hitrate
    done
    echo ""
done
