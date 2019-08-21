#/bin/sh
cd /
source="$1"
patch -t -p1 < $source || echo patching failed
cp /usr/src/minix/include/minix/com.h /usr/include/minix/com.h
cp /usr/src/minix/include/minix/config.h /usr/include/minix/config.h
cp /usr/src/minix/include/minix/syslib.h /usr/include/minix/syslib.h
cd /usr/src/minix/kernel; make && make install \
    && cd /usr/src/minix/lib/libsys; make && make install \
    && cd /usr/src/minix/servers/pm; make && make install \
    && cd /usr/src/minix/servers/sched; make && make install \
    && cd /usr/src/releasetools; make hdboot



 
