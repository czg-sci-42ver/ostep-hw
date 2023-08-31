#! /usr/bin/env python3

traceFile = open('./ls-trace.txt', 'r')
vpnFile = open('./vpn.txt', 'w')

for line in traceFile:
    if (not line.startswith('=')) and (' ' in line):
        """
        https://stackoverflow.com/a/12672012/21294350 pagesize -> 4096
        """
        vpnFile.write(str((int("0x" + line[3:11], 16) & 0xfffff000) >> 12) + "\n")

traceFile.close()
vpnFile.close()
