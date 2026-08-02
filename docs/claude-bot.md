# issue を Claude に任せる

issue をボット役のアカウントへアサインすると、GitHub Actions 上で Claude が動き、
調査 → 実装 → テスト → PR まで通しでやる。`.github/workflows/claude.yml` がその中身。

## できること

| きっかけ | 動き |
|--|--|
| issue をボットへアサイン | issue に進捗コメントを立てて実装し、PRを出す |
| issue に `claude` ラベルを付ける | 同上 (アサインの代わり。ボット用アカウントを作らずに済む) |
| issue / PR のコメントで `@claude` と書く | その場で返事をする。追加指示やレビュー対応はこちら |

PRができると `Build` ワークフローが走り、4機種ぶんのコンパイルが確かめられる。

## 準備

### 1. Claude GitHub App を入れる

https://github.com/apps/claude からこのリポジトリへインストールする。
Contents / Issues / Pull requests の read & write を求められる。

### 2. APIキーをシークレットに置く

Settings → Secrets and variables → Actions → Secrets で
`ANTHROPIC_API_KEY` を追加する。キーは https://console.anthropic.com で発行する。

### 3. ボット役のアカウントを用意する

**GitHubのAppはissueのアサイン先にできない**ので、アサインで動かすには
アサインできる実体 — つまり普通のGitHubアカウント — が要る。

1. ボット用のアカウントを新しく作る (名前は何でもよい。例: `windmill-bot`)
2. このリポジトリの Settings → Collaborators に招待する (Read 権限で足りる)
3. Settings → Secrets and variables → Actions → **Variables** で
   `CLAUDE_ASSIGNEE` にそのアカウント名を入れる

`CLAUDE_ASSIGNEE` を置かない場合は `windmill-bot` という名前とみなす。

アカウントを増やしたくなければ、この手順を飛ばしてラベルのほうを使う。
その場合は Issues → Labels → New label で `claude` という名前のラベルを作っておく
(まだ無い)。以降、issue にそのラベルを付けるとアサインと同じように動く。

## 使い方

issue を書いて、ボットにアサインする。あとは進捗コメントとPRを待つ。
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

## 注意

- **費用がかかる。** Anthropic のAPIトークンと、GitHub Actions の実行時間の両方。
  ジョブは60分で打ち切る。モデルとターン数は `claude.yml` の `claude_args` で変える
- **アサインとラベルはコラボレーターしか触れない**ので、外部から勝手には起動しない。
  `@claude` のほうも、書き込み権限のある人のコメントにしか反応しない
- **PRは自動ではマージされない。** 中身は必ず読んでからマージする
- ユニットテストはワークフローの中で走らせるが、`scripts/build.sh` は走らせない。
  ビルドの確認はPR側の `Build` ワークフローに任せている
