#! /usr/bin/env bash
set -euo pipefail

if [ ! -f /.dockerenv ]; then
    echo '🚨  Do not run it outside a Docker container.' 1>&2
    exit 1
fi

host_uid=$(ls -n "$0" | awk '{print $3}') # study who my owner is
host_gid=$(ls -n "$0" | awk '{print $4}') # study what I belong to

for keyboard in technik ymd40; do
  qmk lint -kb "windmill/$keyboard" -km default --strict
  make "windmill/$keyboard:default"
  mv "windmill_${keyboard}_default.hex" "/output/windmill_${keyboard}.hex"
done

chown "$host_uid:$host_gid" /output/*.hex
