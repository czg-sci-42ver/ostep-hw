#!/usr/bin/env awk -f

function update_contents(fh,uid,whether_write){
    # https://www.gnu.org/software/gawk/manual/html_node/Arrays-of-Arrays.html
    # print "arg: " fh " " uid
    file_user_off[whether_write][fh][uid]=strtonum("0x"$12)
    # printf("assign off: 0x%x\n",file_user_off[whether_write][fh][uid])
    file_user_cnt[whether_write][fh][uid]=strtonum("0x"$14)
    if (file_user_next_off[whether_write][fh][uid]!=file_user_off[whether_write][fh][uid]) {
        # printf("(%d %d) random plus\n",fh,uid)
        not_equal[whether_write][fh][uid]++
    }else {
        # printf("(%s %s) sequential plus\n",fh,uid)
        equal[whether_write][fh][uid]++
    }
    file_user_old_off[whether_write][fh][uid]=file_user_off[whether_write][fh][uid]
    file_user_next_off[whether_write][fh][uid]=file_user_off[whether_write][fh][uid]+file_user_cnt[whether_write][fh][uid]
    # printf("(off+cnt): %d+%d==%d\n",file_user_off[whether_write][fh][uid],file_user_cnt[whether_write][fh][uid],file_user_next_off[whether_write][fh][uid])
}

{ 
    # if (($8 == "read" || $8 == "write") && $10 == "01122f009ead0d0120000000003669c1928f10016486000001122f009ead0d00" && $2 == "32.03fe") 
    # https://www.gnu.org/software/gawk/manual/html_node/Multidimensional.html
    if ($8 == "read" && $5 == "C3"){
        # print $0
        update_contents($10,$2,0)
        ### not use function
        # print "arg: " $10 " " $2
        # file_user_off[0][$10][$2]=strtonum("0x"$12)
        # printf("assign off: 0x%x\n",file_user_off[0][$10][$2])
        # file_user_cnt[0][$10][$2]=strtonum("0x"$14)
        # if (file_user_next_off[0][$10][$2]!=file_user_off[0][$10][$2]) {
        #     printf("(%s %s) random plus before %d\n",$10,$2,not_equal[0][$10][$2])
        #     not_equal[0][$10][$2]++
        #     printf("after %d\n",not_equal[0][$10][$2])
        # }else{
        #     printf("(%d %d) sequential plus\n",$10,$2)
        #     equal[0][$10][$2]++
        # }
        # file_user_next_off[0][$10][$2]=file_user_off[0][$10][$2]+file_user_cnt[0][$10][$2]
        ###
    }
    if ($8 == "write" && $5 == "C3"){
        update_contents($10,$2,1)
    }
}
# here not use seq_usr_cnt,rnd_usr_cnt to calculate the average
function show_log(multi_arr,whether_write,equal_arr,seq_usr_cnt,rnd_usr_cnt) {
    rnd_cnt=0
    seq_cnt=0
    seq_rnd_diff=0
    seq_rnd_min_diff=0
    seq_usr_check_cnt=0
    log_str=whether_write?"write":"read"
    for (fh in multi_arr[whether_write]) {
        # print fh " " multi_arr[whether_write][fh]
        if (isarray(multi_arr[whether_write][fh])) {
            # printf("check multi_arr[whether_write] %d\n",fh)
            for (uid in multi_arr[whether_write][fh]) {
                # printf("check multi_arr[whether_write] %d %d\n",fh,uid)

                # printf("%s (%s %s) random: %d\n",log_str,fh,uid,multi_arr[whether_write][fh][uid])
                rnd_cnt+=multi_arr[whether_write][fh][uid]
                if (isarray(equal_arr[whether_write][fh])) {
                    # TODO weird 0 output because `equal[whether_write][fh][uid]++` init and increment at least 1
                    ####
                    # $ awk -f ./q4.awk sample
                    # write (01122f009ead0d0120000000003669d1c8a510016486000001122f009ead0d00 32.03fa) sequential: 0
                    ####
                    # printf("%s (%s %s) sequential: %d\n",log_str,fh,uid,equal_arr[whether_write][fh][uid])
                    seq_cnt+=equal_arr[whether_write][fh][uid]
                    diff=equal_arr[whether_write][fh][uid]-multi_arr[whether_write][fh][uid]
                    if (seq_rnd_diff<diff) {
                        seq_rnd_diff=diff
                        max_pair[0]=equal_arr[whether_write][fh][uid]
                        max_pair[1]=multi_arr[whether_write][fh][uid]
                    }
                    if (seq_rnd_min_diff>diff) {
                        seq_rnd_min_diff=diff
                        min_pair[0]=equal_arr[whether_write][fh][uid]
                        min_pair[1]=multi_arr[whether_write][fh][uid]
                    }
                    seq_usr_check_cnt++
                }
            }
        }
    }
    if (seq_usr_check_cnt<seq_usr_cnt[whether_write]) {
        printf("%s %d seq skipped\n",log_str,seq_usr_cnt[whether_write]-seq_usr_check_cnt)
        printf("actual seq_cnt:%d\n",check_seq(equal[whether_write]))
    }
    printf("%s rnd/seq:%d/%d with\nseq_rnd_max_diff(seq-rnd):%d by (%d-%d)\n",log_str,rnd_cnt,seq_cnt,seq_rnd_diff,max_pair[0],max_pair[1])
    printf("seq_rnd_min_diff(seq-rnd):%d by (%d-%d)\n",seq_rnd_min_diff,min_pair[0],min_pair[1])
}

function check_seq(two_d_arr){
    cnt=0
    for (fh in two_d_arr) {
        if (isarray(two_d_arr[fh]))
            for (uid in two_d_arr[fh]) {
                cnt+=two_d_arr[fh][uid]
            }
    }
    return cnt
}

# 2D array
function check_sub_arr_size(arr){
    cnt=0
    for (fh in arr) {
        if (isarray(arr[fh]))
            cnt+=length(arr[fh])
    }
    return cnt
}

function summary_log(Whether_Write){
    whether_write=Whether_Write
    seq_usr_cnt[whether_write]=check_sub_arr_size(equal[whether_write])
    rnd_usr_cnt[whether_write]=check_sub_arr_size(not_equal[whether_write])
    log_str=whether_write?"\nwrite_seq_usr_cnt ":"\nread_seq_usr_cnt "
    print log_str seq_usr_cnt[whether_write]
    show_log(not_equal,whether_write,equal,seq_usr_cnt,rnd_usr_cnt);
}
function drop_first_not_equal(not_equal) {
    for (whether_write in not_equal){
        # can't directly assign to one variable https://stackoverflow.com/a/62179751/21294350
        # arr=not_equal[whether_write]
        if (isarray(not_equal[whether_write]))
            for (fh in not_equal[whether_write]) {
                if (isarray(arr[fh]))
                    for (uid in not_equal[whether_write][fh]) {
                        not_equal[whether_write][fh][uid]--
                    }
            }
    }
}
END	{
    # https://stackoverflow.com/a/3060790/21294350
    drop_first_not_equal(not_equal)
    summary_log(0)
    summary_log(1)
}