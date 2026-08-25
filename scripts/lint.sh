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

# lint は keyboard.json とレイアウト定義しか見ないので、
# geonix41 のベンダーブロブもコアへのパッチも要らない
docker run \
  --interactive --rm \
  --mount type=bind,source="$dirpath/firmware",target="/qmk_firmware/keyboards/$project",readonly \
  --mount type=bind,source="$dirpath/scripts/lint-entrypoint.sh",target="/lint-entrypoint.sh",readonly \
  --entrypoint /bin/bash \
  "$image" /lint-entrypoint.sh $project
