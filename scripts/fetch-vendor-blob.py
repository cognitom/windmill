#!/usr/bin/env python3
"""geonix41 のベンダーブロブを Google Drive の配布 zip から取り出す。

配布物は QMK ソース一式の zip で **約1GB** あるが、必要なのは 2 ファイル (計 97KB) だけ。
zip は末尾の中央ディレクトリから各メンバの位置を引ける形式なので、HTTP の Range
リクエストで「中央ディレクトリ → 目的のメンバ」とピンポイントに取れば
**転送量 4.5MB・10秒程度**で済む。1GB を落とす必要はない。

出力先の 2 ファイルが既に正しい sha256 で存在していれば何もしない (冪等)。
"""

import hashlib
import pathlib
import struct
import sys
import urllib.request
import zlib

# ベンダー配布物 (公開リンク)。差し替えたら EXPECTED も更新すること
DRIVE_FILE_ID = "1Xv3MLWRJ2NYUFQUfYOxqfMQ_e2bGl3Jv"
ZIP_NAME = "qmk_firmware_Rev.2.5-1U.zip"
URL = (
    "https://drive.usercontent.google.com/download"
    f"?id={DRIVE_FILE_ID}&export=download&confirm=t"
)

# QMK ツリーの lib/rdr_lib/ に相当する。keyboards/ の下に置くと qmk lint が
# 「あるはずのないファイル」として弾くので、外に出してある
OUT_DIR = pathlib.Path(__file__).resolve().parent.parent / "vendor"

# zip 内のパス末尾 → (出力名, 改行をLFに正規化するか, 出力後の sha256)
#
# rdr_common.h は zip 内では CRLF。QMK ツリーに合わせて LF に正規化して置く
# (zip 内の生バイトの sha256 は f0a1c6ec97020d8fc2d7143277d38b6181d1306d347ea2e32b588b71f8bbbf39)。
MEMBERS = {
    "lib/rdr_lib/librdrcommon.a": (
        "librdrcommon.a",
        False,
        "7a2dba53e3b564ed412bc1648ae570fb134bbe15ee505f72c7c94d7c227e560d",
    ),
    "lib/rdr_lib/rdr_common.h": (
        "rdr_common.h",
        True,
        "61385d2308b855bc1580c880a6a7407e35daf7884824fc24eb5c99943b16a588",
    ),
}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fetch(start: int, end: int) -> bytes:
    """[start, end] を Range で取る (end は含む)。"""
    req = urllib.request.Request(URL, headers={"Range": f"bytes={start}-{end}"})
    with urllib.request.urlopen(req) as res:
        if res.status != 206:
            sys.exit(
                f"🚨 Range リクエストが効いていない (HTTP {res.status})。\n"
                f"   配布物が非公開になったか、Drive の仕様が変わった可能性がある。\n"
                f"   {ZIP_NAME} を手で落として lib/rdr_lib/ の中身を vendor/ に置いてほしい。"
            )
        return res.read()


def total_size() -> int:
    req = urllib.request.Request(URL, headers={"Range": "bytes=0-0"})
    with urllib.request.urlopen(req) as res:
        return int(res.headers["content-range"].split("/")[1])


def already_ok() -> bool:
    for out_name, _, want in MEMBERS.values():
        path = OUT_DIR / out_name
        if not path.is_file() or sha256(path.read_bytes()) != want:
            return False
    return True


def locate_members() -> dict:
    """中央ディレクトリを読んで、必要なメンバの位置情報を集める。"""
    size = total_size()

    # EOCD (End Of Central Directory) は末尾にある。コメント最大 65535 + ヘッダ 22
    tail = fetch(max(0, size - 65558), size - 1)
    pos = tail.rfind(b"PK\x05\x06")
    if pos < 0:
        sys.exit("🚨 zip の EOCD が見つからない。配布物が壊れているか zip ではない")
    cd_size, cd_offset = struct.unpack("<II", tail[pos + 12 : pos + 20])

    cd = fetch(cd_offset, cd_offset + cd_size - 1)

    found, p = {}, 0
    while p < len(cd) and cd[p : p + 4] == b"PK\x01\x02":
        method, = struct.unpack("<H", cd[p + 10 : p + 12])
        comp_size, = struct.unpack("<I", cd[p + 20 : p + 24])
        name_len, extra_len, comment_len = struct.unpack("<HHH", cd[p + 28 : p + 34])
        local_offset, = struct.unpack("<I", cd[p + 42 : p + 46])
        name = cd[p + 46 : p + 46 + name_len].decode("utf-8", "replace")
        for want in MEMBERS:
            if name.endswith(want):
                found[want] = (local_offset, comp_size, method, name)
        p += 46 + name_len + extra_len + comment_len

    missing = set(MEMBERS) - set(found)
    if missing:
        sys.exit(f"🚨 zip 内に見つからない: {', '.join(sorted(missing))}")
    return found


def extract(local_offset: int, comp_size: int, method: int) -> bytes:
    # ローカルヘッダは 30 バイト + ファイル名 + extra。実データはその後ろ
    header = fetch(local_offset, local_offset + 29)
    name_len, extra_len = struct.unpack("<HH", header[26:30])
    start = local_offset + 30 + name_len + extra_len
    data = fetch(start, start + comp_size - 1)
    if method == 0:
        return data
    if method == 8:
        return zlib.decompress(data, -15)
    sys.exit(f"🚨 未対応の圧縮方式: {method}")


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    if already_ok():
        print(f"✅ ベンダーブロブは取得済み ({OUT_DIR})")
        return

    print(f"📦 {ZIP_NAME} から 2 ファイルだけ取り出す (Range リクエスト)")
    found = locate_members()

    for want, (out_name, to_lf, expect) in MEMBERS.items():
        local_offset, comp_size, method, name = found[want]
        raw = extract(local_offset, comp_size, method)
        if to_lf:
            raw = raw.replace(b"\r\n", b"\n")

        got = sha256(raw)
        if got != expect:
            sys.exit(
                f"🚨 {out_name} の sha256 が想定と違う\n"
                f"   期待: {expect}\n"
                f"   実際: {got}\n"
                f"   配布物が更新された可能性がある。patches/ のパッチが当たるか"
                f" 確認したうえで、このスクリプトの EXPECTED を更新すること"
            )

        (OUT_DIR / out_name).write_bytes(raw)
        print(f"   {name} → {out_name} ({len(raw):,} バイト)")

    print(f"✅ 取得完了 ({OUT_DIR})")


if __name__ == "__main__":
    main()
