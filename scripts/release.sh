#! /usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.." # move to the current dir
branch=main

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

usage() {
  echo "使い方: $0 [-p|--patch] [-m|--minor] [vX.Y.Z]" 1>&2
}

mode=patch
new_version=""

case "${1:-}" in
  -p | --patch)
    mode=patch
    ;;
  -m | --minor)
    mode=minor
    ;;
  "")
    mode=patch
    ;;
  v[0-9]*)
    new_version=$1
    ;;
  *)
    echo "🚨  引数が不正です: $1" 1>&2
    usage
    exit 1
    ;;
esac

if [ -z "$new_version" ]; then
  latest_tag=$(git tag -l 'v*' --sort=-v:refname | head -n1)
  latest_tag=${latest_tag:-v0.0.0}
  version=${latest_tag#v}
  IFS='.' read -r major minor patch <<< "$version"

  if [ "$mode" = minor ]; then
    new_version="v${major}.$((minor + 1)).0"
  else
    new_version="v${major}.${minor}.$((patch + 1))"
  fi
fi

echo "🏷  ${new_version} をタグ付けして origin へ push します"
git tag "$new_version"
git push origin "$new_version"
