# issue を Claude に任せる

issue に `claude` ラベルを付けると、GitHub Actions 上で Claude が動き、
調査 → 実装 → テスト → PR まで通しでやる。`.github/workflows/claude.yml` がその中身。

## できること

| きっかけ | 動き |
|--|--|
| issue に `claude` ラベルを付ける | issue に進捗コメントを立てて実装し、PRを出す |
| issue / PR のコメントで `@claude` と書く | その場で返事をする。追加指示やレビュー対応はこちら |

コミットもPRもコメントも **`claude[bot]`** (Claude GitHub App) 名義になる。
`cognitom` 名義では書き込まれない。

PRができると `Build` ワークフローが走り、4機種ぶんのコンパイルが確かめられる。

## 起動できる人

**リポジトリの持ち主だけ。** それ以外の人がラベルを付けても `@claude` と書いても、
ワークフローは起動しない。使う枠が自分の操作ぶんに収まるようにしてある。

判定は `github.actor`、つまりそのイベントを起こしたアカウントで見る。
別の人にも許すなら Variables に `CLAUDE_USER` を置いてそのログイン名を入れる
(複数人に開くなら `claude.yml` の条件を `contains()` に書き換える)。

## 準備

1. **Claude GitHub App を入れる。**
   https://github.com/apps/claude からこのリポジトリへインストールする。
   Contents / Issues / Pull requests の read & write を求められる
2. **トークンを置く。** 手元で `claude setup-token` を実行し、出てきたトークンを
   Settings → Secrets and variables → Actions → Secrets に
   `CLAUDE_CODE_OAUTH_TOKEN` として登録する
3. **ラベルを作る。** Issues → Labels → New label で `claude` を作る

ボット用のアカウントは要らない。トークンは
**Claude Code のサブスクリプション (Pro / Max) の枠**で動く。

## 使い方

issue を書いて `claude` ラベルを付ける。あとは進捗コメントとPRを待つ。
やることが曖昧だったり、キー配列の方針など好みが割れる判断が要るときは、
Claude は勝手に決めずに issue へ質問をコメントして止まる。
そこに答えるか、PRに `@claude` で追加指示を出せば続きをやる。

書き方は普通の issue でよいが、次があると精度が上がる。

- どの機種の話か (technik / ymd40 / minipeg48 / geonix41)
- どのレイヤー・どのキーか
- 期待する挙動と実際の挙動

リポジトリ側のお作法 (テストの二重管理、コメントは日本語、など) は
ルートの `CLAUDE.md` に書いてあり、Claude はこれを読んでから動く。
やり方を変えたいときはワークフローではなく `CLAUDE.md` を直すのが早い。

## 他のリポジトリで使う

`claude.yml` はリポジトリ名を持たないので、そのままコピーして使える。
持ち主の判定も `github.repository_owner` を見ているだけなので設定は要らない。
移すときに直すのはこの2つ。

- **`claude_args` の `--allowedTools`。** windmill では
  `Bash(bash scripts/test.sh)` しか許していない。移った先のテストコマンドに変える
- **`prompt` の中身。** ビルドとテストの手順が windmill 固有

移した先ごとに、App のインストール・Secrets・`claude` ラベルの3つは要る
(Secrets は Organization レベルにまとめてもよい)。ルートの `CLAUDE.md` も
そのリポジトリ用に書く。

**非公開リポジトリでは GitHub Actions の実行時間が課金対象**になる点に注意。
windmill は public なので無料枠だが、非公開だと月あたりの無料分を食う。
このワークフローはユニットテストで Docker イメージを作るぶん1回が長めなので、
効いてくるようなら `timeout-minutes` と `--max-turns` を絞る。

## 第三者からの安全性

**PRでワークフローを書き換えられても、持ち主限定は外れない。**
`issues` / `issue_comment` / `pull_request_review_comment` の3イベントは、
GitHubの仕様で**デフォルトブランチにあるワークフローファイルしか実行されない**。
fork のPRが `claude.yml` をどう書き換えても、マージするまで効かない。
このワークフローは `pull_request` をトリガーに持っていないので、
PRのコードでワークフローが走る経路もない。

さらに Claude GitHub App のトークンは、そのワークフローがデフォルトブランチに
あるかを確認してから発行される。ワークフローを変更したPRではそもそもトークンが
下りない。**ワークフローを触るPRを、中身を読まずにマージしないこと。**
守りの本体はここで、GitHubのrulesetで足せることは特に無い
(`.github/workflows/**` を守る push ruleset は非公開リポジトリ向けの機能)。

残るのは**こちらから第三者のコードに対して Claude を呼んでしまう**経路。
fork のPRで `@claude` と書くと、Claude はそのPRのコードをチェックアウトし、
許可してある `bash scripts/test.sh` も fork 側の中身が走る。
実行時の環境にはトークンがあるので、任意コード実行と同じ扱いになる。
fork の `CLAUDE.md` を読むことによるプロンプトインジェクションも同じ経路。

- **インラインのレビューコメント (Files changed) からは、fork のPRでは起動しない。**
  ワークフロー側で弾いてある
- **PRの会話タブのコメントは、イベントに fork かどうかが載らないので弾けない。**
  第三者のPRでは `@claude` と書かないこと。レビューは自分の目でやる
- リポジトリの Settings → Actions → General で
  **Fork pull request workflows from outside collaborators** を
  「Require approval for all outside collaborators」にしておくと、
  `Build` ワークフロー側も第三者のPRでは承認するまで走らなくなる

## 注意

- **枠は対話セッションと共有になる。** CIでの実行が、手元で Claude Code を
  使うときと同じ枠を消費する。ジョブは60分で打ち切る。
  モデルとターン数は `claude_args` で変える
- **トークンには期限がある** (発行から約1年)。切れると静かに動かなくなるので、
  動かなくなったらまず `claude setup-token` を取り直す
- **PRは自動ではマージされない。** 中身は必ず読んでからマージする
- **ワークフローはデフォルトブランチに入るまで動かない。** App のトークンは
  「そのワークフローがデフォルトブランチに存在するか」を見て発行されるので、
  ブランチに置いただけの状態では起動しない (`workflow_not_found_on_default_branch`)
- ユニットテストはワークフローの中で走らせるが、`scripts/build.sh` は走らせない。
  ビルドの確認はPR側の `Build` ワークフローに任せている
