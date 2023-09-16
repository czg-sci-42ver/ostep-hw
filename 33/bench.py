#!/usr/bin/env python3

import argparse
import platform
import subprocess
import time
import os

parser = argparse.ArgumentParser()
parser.add_argument("file_name")
parser.add_argument("num_reqs", type=int)
parser.add_argument("trials", type=int)
args = parser.parse_args()

watch_process=1
sp_first=1

def bench(lib):
    total_time = 0
    for _ in range(args.trials):
        """
        not use shell=True https://stackoverflow.com/a/2408670/21294350
        """
        sp = subprocess.Popen(
            [f'./server_{lib}.out', str(args.num_reqs)],
            stderr=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        # os.system("echo $(sudo netstat -tnlp | grep :8080)")
        cp = subprocess.Popen(
            ['./client.out', args.file_name, str(args.num_reqs)],
            stderr=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        """
        no need sleep if client
        """
        # time.sleep(0.100)
        netcmd_str = "sudo netstat -tnlp | grep :8080"
        netcmd_echo_str = "echo $("+netcmd_str+")"
        """
        use echo to ensure return 0 by https://stackoverflow.com/a/27921146/21294350
        use shell=True to be compatible with os.system https://stackoverflow.com/a/18739828/21294350
        """
        if subprocess.check_output(netcmd_echo_str,shell=True) != b'\n':
            print("server still running",repr(subprocess.check_output(netcmd_echo_str,shell=True)))
        """
        sometimes "client error: connect: Connection refused", see connection.h AVOID_CONNECT_ERROR
        https://serverfault.com/a/725263/1032032

        why must call cp before sp, communicate implies blocking? https://stackoverflow.com/a/2408670/21294350
        if cp with error, sp will be into loop, so the blocking state.
        """
        if sp_first:
            s_out, s_err = sp.communicate()
        c_out, c_err = cp.communicate()
        if watch_process:
            print(c_out)
        if c_err:
            print(f'client error: {c_err}')
            sp.terminate()
            exit(1)
        if not sp_first:
            s_out, s_err = sp.communicate()
        if s_err:
            print(f'server error: {s_err}')
            exit(1)
        if watch_process:
            print("plus",s_out)
        total_time += float(s_out)

    print(f'{lib}: {total_time / (args.trials * args.num_reqs)} '
          'nanoseconds/request')


if platform.system() == 'Linux':
    bench('io_uring')  # async
    bench('epoll')     # sync
bench('libevent')      # nonblock
