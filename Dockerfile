FROM qmkfm/qmk_cli
ARG QMK_VERSION=0.33.11

RUN git clone --recurse-submodules --depth 1 --branch $QMK_VERSION https://github.com/qmk/qmk_firmware.git
WORKDIR /qmk_firmware

# テスト基盤への手当て (tests/readme.md 参照)。tests/test_common しか触らず
# ファームウェアのビルドには効かないので、イメージに焼いておく
COPY patches/qmk-test-harness.patch /tmp/
RUN git apply /tmp/qmk-test-harness.patch && rm /tmp/qmk-test-harness.patch

# rootful の docker で -u を付けて root 以外で入ってきたとき用。QMK はビルドの
# 最後にファームウェアを直下へコピーし、バージョンの埋め込みで git を叩く
RUN chmod a+w /qmk_firmware && git config --system --add safe.directory '*'
