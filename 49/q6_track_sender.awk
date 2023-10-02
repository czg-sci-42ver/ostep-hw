#!/usr/bin/env awk -f

{
    if ($5 == "C3"){
        startTime[$2][$6] = $1
    }
    for (sender in startTime){
        # 1. here assumer receiver always corresponds to the latest sender data, 
        # so not reset the sender data after calculation.
        # 2. here only ensure the sender correspondance and not care about the retry.
        if ($5 == "R3" && $3 == sender) {
            latency = ($1 - startTime[sender][$6]) * 1000
            if (latency <= 1)
                print latency
            #print "ID", $6, "latency", ($1 - startTime[$6]) * 1000, "milliseconds"
            #sum = sum + $1 - startTime[$6]
        }
    }
}
#END {
#    print "Average latency", sum / length(startTime) * 1000, "milliseconds"
#}
