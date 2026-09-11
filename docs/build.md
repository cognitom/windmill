# ファームウェアの作成

[コンパイルしたもの](https://github.com/cognitom/windmill/releases) も用意していますが、自分で作成することも可能です。

必要な環境は以下の通り。

- Windows(WSL2)、または Mac
- Docker (Compose v2)

QMKのドキュメントにある[セットアップ手順](https://docs.qmk.fm/#/newbs_getting_started)は不要です。QMK はコンテナの中で動かします。

## QMK のコンテナを立てる

`compose.agent.yaml` に QMK の環境を用意してあります。立てると `qmk` サービスが常駐します。
初回は QMK の取得とイメージのビルドで数分かかります。

```bash
$ git clone https://github.com/cognitom/windmill.git
$ cd windmill
$ docker compose -f compose.agent.yaml up -d --wait
```

スクリプトはホストの `qmk` コマンドを呼ぶので、それがこのコンテナへ届くようにしておきます。
コマンドをコンテナへ振り向ける仕組み (shim) があればそれを使い、無ければ `scripts/bin` を
PATH に足します。ここの `qmk` は `docker compose exec` で qmk サービスへ振り向けるラッパーで、
CI も同じものを使っています。

```bash
$ export PATH="$PWD/scripts/bin:$PATH"
```

## ビルド

```bash
$ bash scripts/build.sh           # technik / ymd40 / minipeg48
$ bash scripts/geonix41/build.sh  # geonix41
```

`output`ディレクトリ内にファームウェアが作成されます。

geonix41 は QMK コアにパッチを当ててビルドするため、常駐のコンテナとは別に使い捨ての
コンテナで回します (こちらは `docker` を直接使います)。ベンダー配布のライブラリも
自動で取ってきます。詳細は [firmware/geonix41/readme.md](../firmware/geonix41/readme.md) を参照してください。

## lintとテスト

キー処理のユニットテストがあります。どちらも同じコンテナの `qmk` で動きます。

```bash
$ bash scripts/lint.sh   # keyboard.json の静的チェック。4機種ぶん
$ bash scripts/test.sh   # ユニットテスト
```

詳細は [tests/readme.md](../tests/readme.md) を参照してください。
