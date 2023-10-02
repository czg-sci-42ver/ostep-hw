#!/usr/bin/env awk -f

{
    if ($5 == "C3" && $8 == "getattr")
        users[$2]++

    # size   ($21): size of the file in bytes
    # fileid ($31): number which uniquely identifies the file within its
    #               file system (on UNIX this would be the inumber)

    if ($5 == "R3" && $8 == "getattr")
        fileSize[$31] = $21
}
END {
    for (f in fileSize)
        # https://www.gnu.org/software/gawk/manual/html_node/Concatenation.html#:~:text=6.2.,2%20String%20Concatenation&text=It%20does%20not%20have%20a,%2D%7C%20Field%20number%20one%3A%20Anthony%20%E2%80%A6
        sum = sum + strtonum("0x"fileSize[f])

    avg_req=0
    for (user in users){
        print "Client", user, "requests", users[user]
        # Average
        avg_req += users[user]
    }
    avg_req/=length(users)

    print length(users), "clients"

    printf("Average file size: %d bytes\n", sum / length(fileSize))

    # variance and standard deviation
    avg_dev=0
    for (user in users) {
        # https://unix.stackexchange.com/a/140045/568529
        user_dev[user]=(users[user]-avg_req)^2
        avg_dev+=user_dev[user]
    }
    var=avg_dev/length(users)
    printf("variance of user requests:%d\n", var)
    std_dev=sqrt(var)
    printf("standard deviation of user requests:%d\n", std_dev)
}
