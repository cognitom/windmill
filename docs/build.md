# ファームウェアの作成

[コンパイルしたもの](https://github.com/cognitom/windmill/releases) も用意していますが、自分で作成することも可能です。

必要な環境は以下の通り。

- Windows(WSL2)、または Mac
- Docker

上記整っていれば、QMKのドキュメントにある[セットアップ手順](https://docs.qmk.fm/#/newbs_getting_started)は不要。このリポジトリをクローンして`build.sh`を実行すればOKです。

```bash
$ git clone https://github.com/cognitom/windmill.git
$ cd windmill
$ bash scripts/build.sh
```

`output`ディレクトリ内にファームウェアが作成されます。

## lintとテスト

キー処理のユニットテストがあります。同じくDockerだけあれば動きます。

```bash
$ bash scripts/lint.sh   # keyboard.json の静的チェック。数秒で終わる
$ bash scripts/test.sh   # ユニットテスト
```

`lint.sh` は `build.sh` の中でも走っているものと同じで、ファームウェアの
コンパイルまではしません。手早く確かめたいときに使えます。

詳細は [tests/readme.md](../tests/readme.md) を参照してください。
