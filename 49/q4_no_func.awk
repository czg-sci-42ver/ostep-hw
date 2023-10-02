#!/usr/bin/env awk -f

function update_contents(fh,uid,whether_write){
    # https://www.gnu.org/software/gawk/manual/html_node/Arrays-of-Arrays.html
    print "arg: " fh " " uid
    file_user_off[whether_write][fh][uid]=strtonum("0x"$12)
    printf("assign off: 0x%x\n",file_user_off[whether_write][fh][uid])
    file_user_cnt[whether_write][fh][uid]=strtonum("0x"$14)
    if (file_user_next_off[whether_write][fh][uid]!=file_user_off[whether_write][fh][uid]) {
        printf("(%d %d) random plus\n",fh,uid)
        not_equal[whether_write][fh][uid]++
    }else{
        printf("(%d %d) sequential plus\n",fh,uid)
        equal[whether_write][fh][uid]++
    }
    file_user_next_off[whether_write][fh][uid]=file_user_off[whether_write][fh][uid]+file_user_cnt[whether_write][fh][uid]
}

{ 
    # if (($8 == "read" || $8 == "write") && $10 == "01122f009ead0d0120000000003669c1928f10016486000001122f009ead0d00" && $2 == "32.03fe") 
    # https://www.gnu.org/software/gawk/manual/html_node/Multidimensional.html
    if ($8 == "read" && $5 == "C3"){
        # print $0
        # update_contents($10,$2,0)
        ### not use function
        print "arg: " $10 " " $2
        file_user_off[0][$10][$2]=strtonum("0x"$12)
        printf("assign off: 0x%x\n",file_user_off[0][$10][$2])
        file_user_cnt[0][$10][$2]=strtonum("0x"$14)
        if (file_user_next_off[0][$10][$2]!=file_user_off[0][$10][$2]) {
            printf("(%s %s) random plus before %d\n",$10,$2,not_equal[0][$10][$2])
            not_equal[0][$10][$2]++
            printf("after %d\n",not_equal[0][$10][$2])
        }else{
            printf("(%d %d) sequential plus\n",$10,$2)
            equal[0][$10][$2]++
        }
        file_user_next_off[0][$10][$2]=file_user_off[0][$10][$2]+file_user_cnt[0][$10][$2]
        ###
    }
}
END	{
    # https://stackoverflow.com/a/3060790/21294350
    read_seq_cnt=0
    for (fh in equal[0]) {
        if (isarray(equal[0][fh]))
            read_seq_cnt+=length(equal[0][fh])
    }
    print "\nread_seq_cnt " read_seq_cnt
    print "not equal:"
    for (fh in not_equal[0]) {
        # print fh " " not_equal[0][fh]
        if (isarray(not_equal[0][fh])) {
            # printf("check not_equal[0] %d\n",fh)
            for (uid in not_equal[0][fh]) {
                # printf("check not_equal[0] %d %d\n",fh,uid)
                printf("read (%s %s) random: %d\n",fh,uid,not_equal[0][fh][uid])
                if (isarray(equal[0][fh])) {
                    printf("read (%s %s) sequential: %d\n",fh,uid,equal[0][fh][uid])
                }
            }
        }
    }
}