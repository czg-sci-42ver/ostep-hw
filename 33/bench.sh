#! /bin/bash
file_name=$1
# not bigger than LISTEN_BACKLOG
num_reqs=$2
#####
# not use too many trials, which may make sometimes
# client err connect: Connection refused connect: Connection refused
#####
trials=$3
debug=$4

recreate_tmp=0
debug_server_out=0
# not use this to make loop waiting for job end
all_background=1

###############
# https://stackoverflow.com/a/26827443/21294350 
# { berr=$({ bout=$(banana); } 2>&1; echo $bout >&2); echo $berr; } 2>&1
# 2. pass ret across function calls https://unix.stackexchange.com/a/408588/568529
###############
get_stderr_stdout(){
    cmd=$1
    var_prefix=$2
    background=$3
    if [[ ${recreate_tmp} == 1 ]];then
        echo recreate
        touch /tmp/${var_prefix}_out /tmp/${var_prefix}_err
    fi
    ## https://stackoverflow.com/a/18124325/21294350 this can't let ${var_prefix}_out store
    # declare ${var_prefix}_err=$({ ${var_prefix}_out=$(${cmd}); } 2>&1)
    ##
    ## https://unix.stackexchange.com/a/444940/568529
    # ${background} with "&" no use
    # echo "${cmd} 1>/tmp/${var_prefix}_out 2>/tmp/${var_prefix}_err ${background}"
    # ${cmd} 1>/tmp/${var_prefix}_out 2>/tmp/${var_prefix}_err ${background}
    ##
    ## https://stackoverflow.com/a/963857/21294350 and https://stackoverflow.com/a/26827443/2129and 
    ## too complex to pass the stderr and stdout
    if [[ ${all_background} == 0 ]];then
        if [[ -n ${background} ]];then
            # echo "${cmd} 1>/tmp/${var_prefix}_out 2>/tmp/${var_prefix}_err &"
            ${cmd} 1>/tmp/${var_prefix}_out 2>/tmp/${var_prefix}_err &
        else
            ${cmd} 1>/tmp/${var_prefix}_out 2>/tmp/${var_prefix}_err
        fi
    else
        # echo "all at the background"
        ${cmd} 1>/tmp/${var_prefix}_out 2>/tmp/${var_prefix}_err &
    fi
}

check_err(){
    var_prefix=$1
    err_str=$(cat /tmp/${var_prefix}_err)
    if [[ -n ${err_str} ]];then 
       echo ${var_prefix} err ${err_str}
       exit -1
    fi
}
clean_file(){
    rm -f /tmp/${1}_err /tmp/${1}_out
}

clear_all_servers(){
    processes=$(pgrep -f "server_.*")
    if [[ -n ${processes} ]];then
        kill -9 ${processes}
    fi
}

bench(){
    lib=$1
    echo file ${lib}
    time=0
    for i in $(seq ${trials}); do
        ## the overhead is a little high than python.
        get_stderr_stdout "./server_${lib}.out ${num_reqs}" server "&"
        server_pid=$(jobs -p | awk '{print $1}')
        get_stderr_stdout "./client.out ${file_name} ${num_reqs}" client ""
        ## weird jobs always show even after the process ends.
        ## https://stackoverflow.com/a/3643961/21294350
        # set -m
        # cat /tmp/client_out
        # job_list=$(jobs -p)
        # while [[ -n ${job_list} ]];do
        #     jobs -p
        #     echo ps $(ps aux | grep server_pid | grep -v g)
        #     job_list=$(jobs -p)
        #     echo "wait for all jobs to end" ${job_list}
        #     sleep 0.1s
        # done
        ##
        ## this waits for server, otherwise /tmp/server_out will have nothing.
        while [[ -n $(ps aux | grep server_pid | grep -v g) ]];do
            echo "wait for the server"
        done
        check_err server
        check_err client
        if [[ ${debug_server_out} == 1 ]];then
            cat /tmp/server_out
        fi
        server_out=$(cat /tmp/server_out)
        time=$(python -c "print(${time}+${server_out})")
        if [[ ${debug} == 1 ]];then
            echo "${i}th" ${server_out} "ns; total:" ${time} "ns"
        fi
        # to avoid many weird "connect: Connection refused", we need clean and recreate the tmp files
        if [[ ${recreate_tmp} == 1 ]];then
            clean_file server
            clean_file client
        fi
        # ensure no listening
        if [[ -n $(ps aux | grep ${server_pid} | grep -v g) ]];then
            kill -9 ${server_pid};
        fi
    done
    python -c "print(len(str(${time}/(${trials}*${num_reqs})).split('.')[0]))"
    average_time=$(python -c "print(${time}/(${trials}*${num_reqs}))")
    echo "${lib} average_time: ${average_time}ns/request"
}

####
# async is better
# $ ./bench.sh test2.txt 1 1 0
# file epoll
# 8
# epoll average_time: 10363465.0ns/request
# file io_uring
# 7
# io_uring average_time: 8563156.0ns/request
# file libevent
# 8
# libevent average_time: 12727329.0ns/request
# and cache helps
# $ ./bench.sh test2.txt 100 10 0
# file epoll
# 6
# epoll average_time: 145765.263ns/request
# file io_uring
# 6
# io_uring average_time: 128801.484ns/request
# file libevent
# 6
# libevent average_time: 126758.332ns/request
####

# ensures client send to expected servers.
clear_all_servers
# same order with ./bench.py
bench io_uring
bench epoll
bench libevent