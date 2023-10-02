#!/usr/bin/env awk -f

{
    if ($5 == "C3")
        requests[$2]++

    if ($5 == "R3")
        replies[$3]++
}

# borrowed from 3
function avg(arr) {
    avg_req=0
    for (arr_index in arr){
        avg_req += arr[arr_index]
    }
    avg_req/=length(arr)
    return avg_req
}

# https://stackoverflow.com/a/15570815/21294350
function dev(arr,dev_array) {
    # variance and standard deviation
    avg_req=avg(arr)
    avg_dev=0
    
    for (user in arr) {
        # https://unix.stackexchange.com/a/140045/568529
        user_dev[user]=(arr[user]-avg_req)^2
        avg_dev+=user_dev[user]
    }
    dev_array[0]=avg_dev/length(user_dev)
    printf("variance of user requests:%d\n", dev_array[0])
    dev_array[1]=sqrt(dev_array[0])
    printf("standard deviation of user requests:%d\n", dev_array[1])
}

END {
    print length(requests), "clients"
    for (c in requests) {
        print "Client", c, "requests", requests[c], "replies", replies[c]
    }
    # explicit init array
    # devs[0]=0
    # devs[1]=0
    dev(requests,devs)
    printf("var: %d std_dev: %d\n",devs[0],devs[1])
}
