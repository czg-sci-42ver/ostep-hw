Get process states from test output

```
$ ./test-scheduler.sh
$ %run plot.py
```

![ticks of processes over time](ticks.png)

---

- here `usys.S,syscall.h,syscall.c` are to define syscalls.
  and `user.h,defs.h` is similar to declare self-defined functions.
  `sysproc.c` is to define functions.
  `proc.h,proc.c` are to define the scheduler.
  `Makefile` needs add `ps.c`.
