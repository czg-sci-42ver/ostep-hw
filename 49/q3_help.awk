#!/usr/bin/env awk -f

BEGIN	{
    not_find=1
}
# https://unix.stackexchange.com/a/206523/568529
{
    if ($5 == "R3" && $8 == "getattr" && not_find)
        for(i=1;i<=NF;i++) {
            if ($i=="size") {
                not_find=0
                printf("size_num at %d\n", i+1)
            }
            if ($i=="fileid") {
                printf("fileid num at %d\n", i+1)
                exit 0
            }
        }
}