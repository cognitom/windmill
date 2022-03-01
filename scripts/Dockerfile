FROM qmkfm/qmk_cli
ARG QMK_VERSION=0.14.25

RUN git clone --recurse-submodules --depth 1 --branch $QMK_VERSION https://github.com/qmk/qmk_firmware.git
WORKDIR /qmk_firmware
