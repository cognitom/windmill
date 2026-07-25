#! /usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.." # move to the current dir
dirpath="$( pwd -P )" # study the dir path
project=windmill

# Dockerfile の ARG と揃えること。
# QMKのバージョンをイメージのタグに含めて、バージョンを上げたら作り直されるようにする
qmk_version=$(sed -n 's/^ARG QMK_VERSION=//p' ./scripts/Dockerfile)
image="$project-qmk:$qmk_version"

if [ -z "$(docker image ls -q "$image")" ]; then
  docker build -t "$image" -f ./scripts/Dockerfile .
fi

# Run container and build firmware
docker run \
  --interactive --rm \
  --mount type=bind,source="$dirpath/firmware",target="/qmk_firmware/keyboards/$project" \
  --mount type=bind,source="$dirpath/output",target="/output" \
  --mount type=bind,source="$dirpath/scripts/entrypoint.sh",target="/entrypoint.sh" \
  --entrypoint /bin/bash \
  "$image" /entrypoint.sh $project
