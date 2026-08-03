#! /usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.." # move to the current dir
branch=main
changes=CHANGES.md

usage() {
  cat <<'EOS' 1>&2
使い方: scripts/release.sh [-p|--patch] [-m|--minor] [-n|--dry-run] [-y|--yes] [vX.Y.Z]

  -p, --patch    パッチ版を上げる (既定)
  -m, --minor    マイナー版を上げる
  -n, --dry-run  CHANGES.md へ書く内容を表示するだけで、コミットもタグも作らない
  -y, --yes      確認を省く
  vX.Y.Z         バージョンを直接指定する
EOS
}

mode=patch
new_version=""
dry_run=false
assume_yes=false

while [ $# -gt 0 ]; do
  case "$1" in
    -p | --patch) mode=patch ;;
    -m | --minor) mode=minor ;;
    -n | --dry-run) dry_run=true ;;
    -y | --yes) assume_yes=true ;;
    -h | --help)
      usage
      exit 0
      ;;
    v[0-9]*.[0-9]*.[0-9]*) new_version=$1 ;;
    *)
      echo "🚨  引数が不正です: $1" 1>&2
      usage
      exit 1
      ;;
  esac
  shift
done

# タグ付けの起点を勘違いしないよう、mainブランチ以外では実行させない
current_branch=$(git rev-parse --abbrev-ref HEAD)
if [ "$current_branch" != "$branch" ]; then
  echo "🚨  ${branch} ブランチで実行してください (現在のブランチ: ${current_branch})" 1>&2
  exit 1
fi

# コミットされていない変更が残ったままタグを切ると、タグの指す内容と
# 手元の作業ツリーがずれて事故るので先に弾く
if [ -n "$(git status --porcelain)" ]; then
  echo "🚨  作業ツリーに変更が残っています。コミットしてから実行してください" 1>&2
  exit 1
fi

# 手元のmainがoriginより古いままタグを打つと、古いコミットがリリースされてしまう
git fetch origin "$branch" --tags
if [ "$(git rev-parse "$branch")" != "$(git rev-parse "origin/$branch")" ]; then
  echo "🚨  ローカルの ${branch} が origin/${branch} とずれています。pull または push して揃えてください" 1>&2
  exit 1
fi

latest_tag=$(git tag -l 'v*' --sort=-v:refname | head -n1)

if [ -z "$new_version" ]; then
  version=${latest_tag:-v0.0.0}
  version=${version#v}
  IFS='.' read -r major minor patch <<< "$version"

  if [ "$mode" = minor ]; then
    new_version="v${major}.$((minor + 1)).0"
  else
    new_version="v${major}.${minor}.$((patch + 1))"
  fi
fi

if git rev-parse -q --verify "refs/tags/${new_version}" > /dev/null; then
  echo "🚨  タグ ${new_version} は既にあります" 1>&2
  exit 1
fi

# 変更履歴はPRのリンクを並べるだけなので、mainの first-parent を辿って
# マージ済みPRを拾う。GitHubのマージコミットは1行目が
# "Merge pull request #12 from ..." で本文の1行目がPRのタイトル、
# squashマージなら1行目が "タイトル (#12)" になるので、どちらの形も見る
repo_url=$(git remote get-url origin)
repo_url=${repo_url%.git}
repo_url=${repo_url/git@github.com:/https://github.com/}

if [ -n "$latest_tag" ]; then
  range="${latest_tag}..HEAD"
else
  range=HEAD
fi

entry_file=$(mktemp)
trap 'rm -f "$entry_file"' EXIT

{
  echo "## ${new_version} ($(date +%Y-%m-%d))"
  echo
} > "$entry_file"

found=false
while IFS= read -r sha; do
  subject=$(git log -1 --format=%s "$sha")
  case "$subject" in
    'Merge pull request #'[0-9]*)
      number=${subject#'Merge pull request #'}
      number=${number%% *}
      title=$(git log -1 --format=%b "$sha" | sed -n '1p')
      ;;
    *'(#'[0-9]*')')
      number=${subject##*'(#'}
      number=${number%')'}
      title=${subject%" (#${number})"}
      ;;
    *) continue ;;
  esac

  # 本文が空のマージコミットもあるので、その場合は番号だけ並べる
  if [ -n "$title" ]; then
    echo "- ${title} ([#${number}](${repo_url}/pull/${number}))" >> "$entry_file"
  else
    echo "- [#${number}](${repo_url}/pull/${number})" >> "$entry_file"
  fi
  found=true
done < <(git rev-list --first-parent --reverse "$range")

if [ "$found" = false ]; then
  echo "🚨  ${latest_tag:-最初のコミット} 以降にマージされたPRが見つかりません" 1>&2
  exit 1
fi

{
  echo
  echo "---"
  echo
} >> "$entry_file"

echo "📝  ${changes} に書き足す内容:"
echo
cat "$entry_file"

if [ "$dry_run" = true ]; then
  echo "🔎  --dry-run のため、ここで終わります"
  exit 0
fi

if [ "$assume_yes" = false ]; then
  if [ -t 0 ]; then
    read -r -p "この内容で ${new_version} をリリースします。よろしいですか? [y/N] " reply
    case "$reply" in
      [yY]*) ;;
      *)
        echo "🛑  中止しました"
        exit 1
        ;;
    esac
  else
    echo "🚨  対話端末ではありません。内容を確かめたうえで -y を付けて実行してください" 1>&2
    exit 1
  fi
fi

# 新しいセクションは最新版が上に来るよう、最初の "## " の直前へ差し込む
updated=$(mktemp)
trap 'rm -f "$entry_file" "$updated"' EXIT
insert_at=$(grep -n '^## ' "$changes" | head -n1 | cut -d: -f1 || true)
if [ -n "$insert_at" ]; then
  head -n "$((insert_at - 1))" "$changes" > "$updated"
  cat "$entry_file" >> "$updated"
  tail -n "+${insert_at}" "$changes" >> "$updated"
else
  cat "$changes" "$entry_file" > "$updated"
fi
mv "$updated" "$changes"

git add "$changes"
git commit -m "リリース ${new_version}"
git push origin "$branch"

echo "🏷  ${new_version} をタグ付けして origin へ push します"
git tag "$new_version"
git push origin "$new_version"
