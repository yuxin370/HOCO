#!/bin/bash



sudo ./configure \
            --enable-debug \
            ICU_CFLAGS='-I/usr/include/unicode' \
            ICU_LIBS='-L/usr/lib/x86_64-linux-gnu -licui18n -licuuc -licudata'

make -j 4 world

su

make install-world

rm -r /usr/local/pgsql/data
mkdir -p /usr/local/pgsql/data
chown postgres /usr/local/pgsql/data
su - postgres
/usr/local/pgsql/bin/initdb -D /usr/local/pgsql/data
/usr/local/pgsql/bin/pg_ctl -D /usr/local/pgsql/data -l logfile start
/usr/local/pgsql/bin/createdb test



echo 'Run Sucssful.'