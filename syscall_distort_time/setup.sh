#!/usr/bin/env bash

cd / && test -f /usr/src/lib/libc/misc/distort_time.c || patch -p1 </syscall.patch
cd /usr/src/minix/servers/pm
make
make install
cd /usr/src/lib/libc
make
make install
cd /usr/src/releasetools
make hdboot
reboot
