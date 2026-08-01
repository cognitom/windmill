#! /usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.." # move to the current dir
dirpath="$( pwd -P )" # study the dir path
project=windmill

# build.sh と同じイメージを使う。QMKのバージョンはDockerfileのARGが正
qmk_version=$(sed -n 's/^ARG QMK_VERSION=//p' ./scripts/Dockerfile)
image="$project-qmk:$qmk_version"

if [ -z "$(docker image ls -q "$image")" ]; then
  docker build -t "$image" -f ./scripts/Dockerfile .
fi

# ユニットテストなので geonix41 のベンダーブロブは要らない (fetch-vendor-blob.py は走らせない)
docker run \
  --interactive --rm \
  --mount type=bind,source="$dirpath/firmware",target="/qmk_firmware/keyboards/$project" \
  --mount type=bind,source="$dirpath/tests",target="/qmk_firmware/tests/$project",readonly \
  --mount type=bind,source="$dirpath/patches",target="/patches",readonly \
  --mount type=bind,source="$dirpath/scripts/test-entrypoint.sh",target="/test-entrypoint.sh" \
  --entrypoint /bin/bash \
  "$image" /test-entrypoint.sh $project
