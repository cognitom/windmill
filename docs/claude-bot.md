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

## 準備

1. **Claude GitHub App を入れる。**
   https://github.com/apps/claude からこのリポジトリへインストールする。
   Contents / Issues / Pull requests の read & write を求められる
2. **APIキーを置く。** Settings → Secrets and variables → Actions → Secrets で
   `ANTHROPIC_API_KEY` を追加する。キーは https://console.anthropic.com で発行する
3. **ラベルを作る。** Issues → Labels → New label で `claude` を作る

以上。ボット用のアカウントは要らない。

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

## なぜアサインではなくラベルなのか

**GitHubのAppはissueのアサイン先にできない。** アサインで起動する形にするには、
アサインできる実体 — つまりボット役の普通のGitHubアカウント — を作って
コラボレーターに招く必要がある。サイドバーからワンクリックという操作は
ラベルでも変わらないので、アカウントを増やさないほうを採った。

後からアサインで起動したくなったら、ボット用アカウントを用意したうえで
`claude.yml` の `on:` に `assigned` を足し、`labeled` ジョブの条件を

```yaml
if: >-
  (github.event.action == 'labeled' && github.event.label.name == 'claude') ||
  (github.event.action == 'assigned' && github.event.assignee.login == 'ボットの名前')
```

とすればよい。書き込みの名義は `claude[bot]` のままで、そこは変わらない。

## 注意

- **費用がかかる。** Anthropic のAPIトークンと、GitHub Actions の実行時間の両方。
  ジョブは60分で打ち切る。モデルとターン数は `claude.yml` の `claude_args` で変える
- **ラベルはコラボレーターしか付けられない**ので、外部から勝手には起動しない。
  `@claude` のほうも、書き込み権限のある人のコメントにしか反応しない
- **PRは自動ではマージされない。** 中身は必ず読んでからマージする
- **ワークフローはデフォルトブランチに入るまで動かない。** App のトークンは
  「そのワークフローがデフォルトブランチに存在するか」を見て発行されるので、
  ブランチに置いただけの状態では起動しない (`workflow_not_found_on_default_branch`)
- ユニットテストはワークフローの中で走らせるが、`scripts/build.sh` は走らせない。
  ビルドの確認はPR側の `Build` ワークフローに任せている
