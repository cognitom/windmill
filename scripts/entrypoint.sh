#! /usr/bin/env bash

if [ ! -f /.dockerenv ]; then
    echo '🚨  Do not run it outside a Docker container.' 1>&2
    exit 1
fi

host_uid=$(ls -n $0 | awk '{print $3}') # study who my owner is
host_gid=$(ls -n $0 | awk '{print $4}') # study what I belong to

make windmill/technik:default
mv windmill_technik_default.hex /output/windmill_technik.hex

make windmill/ymd40:default
mv windmill_ymd40_default.hex /output/windmill_ymd40.hex
chown $host_uid:$host_gid /output/*
