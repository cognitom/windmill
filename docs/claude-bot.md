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

## 誰の名義で書き込まれるか

既定では **`claude[bot]`** (Claude GitHub App) 名義になる。コミットの author も
PRの作成者も issue へのコメントも全部これで、`cognitom` 名義にはならない。
ワークフローに `id-token: write` があるのは、この App のトークンを
OIDC で受け取るため。

ボット用アカウント名義にしたい場合は、そのアカウントのPATを渡す。

1. ボット用アカウントに、このリポジトリへの **Write** 権限を与える
   (アサインされるだけなら Read で足りるが、書き込ませるには Write が要る)
2. そのアカウントでファイングレインドPATを発行する。
   対象はこのリポジトリのみ、権限は Contents / Issues / Pull requests を Read & write。
   ワークフローファイル自体を触らせたいなら Workflows も足す
3. Secrets に `CLAUDE_BOT_TOKEN` としてPATを登録する
4. Variables に `CLAUDE_BOT_ID` としてそのアカウントの数値ID
   (`https://api.github.com/users/<名前>` の `id`) を入れる。
   コミットの author に使う `<id>+<名前>@users.noreply.github.com` を組むのに要る

`CLAUDE_BOT_TOKEN` があればそちらを使い、無ければ App 名義に戻る。切り替えは
これだけで、ワークフローを書き換える必要はない。

PATを使うときの注意。

- **有効期限がある。** 切れるとある日突然動かなくなる。App 名義ならこれは無い
- **静的なトークンなので、Appの短命トークンより漏れたときの影響が大きい。**
  このワークフローはコラボレーターの操作でしか起動しないので実害は小さいが、
  Anthropic 側も「PATよりAppを推奨」としている
- ボットが普通のユーザー扱いになるので、ボット自身のコメントが `@claude` を
  含むと呼び出しが循環しうる。ワークフロー側で自分の書き込みは弾いてある

なお、名義だけ揃えたくてPATの管理を避けたいなら、**自分用の GitHub App を作る**
という手もある (`actions/create-github-app-token` でトークンを発行し `github_token`
に渡す)。この場合は `windmill-bot[bot]` のような名義になる。ただし App は
issue のアサイン先にはできないので、アサイン用のアカウントは別途要る。

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
- **ワークフローはデフォルトブランチに入るまで動かない。** App のトークンは
  「そのワークフローがデフォルトブランチに存在するか」を見て発行されるので、
  ブランチに置いただけの状態では起動しない (`workflow_not_found_on_default_branch`)
- ユニットテストはワークフローの中で走らせるが、`scripts/build.sh` は走らせない。
  ビルドの確認はPR側の `Build` ワークフローに任せている
