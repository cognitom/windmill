#! /usr/bin/env bash

if [ ! -f /.dockerenv ]; then
    echo '🚨  Do not run it outside a Docker container.' 1>&2
    exit 1
fi

host_uid=$(ls -n $0 | awk '{print $3}') # study who my owner is
host_gid=$(ls -n $0 | awk '{print $4}') # study what I belong to

make $@:default

mv $@* /output
chown $host_uid:$host_gid /output/*
