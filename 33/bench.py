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


def bench(lib):
    total_time = 0
    for _ in range(args.trials):
        sp = subprocess.Popen(
            [f'./server_{lib}.out', str(args.num_reqs)],
            stderr=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        # os.system("echo $(sudo netstat -tnlp | grep :8080)")
        cp = subprocess.Popen(
            ['./client.out', args.file_name, str(args.num_reqs)],
            stderr=subprocess.PIPE, stdout=subprocess.DEVNULL, text=True)
        time.sleep(0.100)
        os.system("echo $(sudo netstat -tnlp | grep :8080)")
        """
        TODO sometimes "client error: connect: Connection refused"
        https://serverfault.com/a/725263/1032032

        TODO why must call cp before sp, communicate implies blocking? https://stackoverflow.com/a/2408670/21294350
        """
        _, c_err = cp.communicate()
        if c_err:
            print(f'client error: {c_err}')
            sp.terminate()
            exit(1)
        s_out, s_err = sp.communicate()
        if s_err:
            print(f'server error: {s_err}')
            exit(1)
        total_time += float(s_out)

    print(f'{lib}: {total_time / (args.trials * args.num_reqs)} '
          'nanoseconds/request')


if platform.system() == 'Linux':
    bench('io_uring')  # async
    bench('epoll')     # sync
bench('libevent')      # nonblock
