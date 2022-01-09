#! /usr/bin/env bash

cd "$(dirname "$0")/.." # move to the current dir
dirpath="$( pwd -P )" # study the dir path
project=windmill

if [ -z "$(docker image ls -q $project-qmk)" ]; then
  docker build -t $project-qmk -f ./scripts/Dockerfile .
fi

# Run container and build firmware
docker run \
  --interactive --rm \
  --env ALT_GET_KEYBOARDS=true \
  --mount type=bind,source="$dirpath/firmware",target="/qmk_firmware/keyboards/$project" \
  --mount type=bind,source="$dirpath/output",target="/output" \
  --mount type=bind,source="$dirpath/scripts/entrypoint.sh",target="/entrypoint.sh" \
  --entrypoint /bin/bash \
  $project-qmk /entrypoint.sh $project
