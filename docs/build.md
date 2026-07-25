# ファームウェアの作成

[コンパイルしたもの](https://github.com/cognitom/windmill/releases) も用意していますが、
自分で作成することも可能です。

必要な環境は以下の通り。

- Windows(WSL2)、または Mac
- [QMK Toolbox](https://github.com/qmk/qmk_toolbox/releases)
- Docker

上記整っていれば、QMKのドキュメントにある[セットアップ手順](https://docs.qmk.fm/newbs_getting_started)は不要。
このリポジトリをクローンして`build.sh`を実行すればOKです。

```bash
$ git clone https://github.com/cognitom/windmill.git
$ cd windmill
$ bash scripts/build.sh
```

`output`ディレクトリ内に `windmill_technik.hex` と `windmill_ymd40.hex` が作成されます。

## 仕組み

`scripts/build.sh` は、`scripts/Dockerfile` からQMKのビルド用イメージを作り
(`qmkfm/qmk_cli` に指定バージョンのqmk_firmwareをクローンしたもの)、このリポジトリの
`firmware` ディレクトリをコンテナ内の `/qmk_firmware/keyboards/windmill` に
バインドマウントして `make` を実行します。

QMKのバージョンは `scripts/Dockerfile` の `ARG QMK_VERSION` で固定しています。
ここを書き換えると、次回の `build.sh` でイメージが作り直されます
(イメージのタグにバージョンを含めているため)。

## Dockerを使わない場合

QMKの環境が既にあるなら、`firmware` を `qmk_firmware/keyboards/windmill` に
シンボリックリンクして直接ビルドできます。

```bash
$ ln -s /path/to/windmill/firmware /path/to/qmk_firmware/keyboards/windmill
$ cd /path/to/qmk_firmware
$ make windmill/technik:default
$ make windmill/ymd40:default
```
