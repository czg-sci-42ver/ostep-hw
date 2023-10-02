#!/usr/bin/env awk -f

BEGIN	{
    not_find=1
}
# https://unix.stackexchange.com/a/206523/568529
{
    if ($5 == "C3" && not_find)
        for(i=1;i<=NF;i++) {
            if ($i=="off") {
                not_find=0
                printf("off num at %d\n", i+1)
            }
            if ($i=="count") {
                printf("count num at %d\n", i+1)
                exit 0
            }
        }
}