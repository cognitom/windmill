# Geonix41

Windmill の4機種目。ES32 FS026 (Cortex-M0) を積んだ、USB / BLE×3 / 2.4G の三モード機。

* Keyboard Maintainer: [cognitom](https://github.com/cognitom)
* Hardware Supported: Geonix REV.2.5 1U (RDMCTMZT)

```sh
bash scripts/build.sh   # 4機種まとめてビルドされる
```

書き込みは MSC (USBドライブ) 型ブートローダー。Esc を押しながら USB 接続すると
FAT ドライブが現れるので、`output/windmill_geonix41.bin` をコピーするだけ。
`make :flash` や QMK Toolbox は使えない (`bootloader: custom`)。

## 他の3機種と違うところ

Windmill の他の機種は素の QMK でビルドできるが、この機種だけは違う。

### ベンダーブロブが要る

無線 (BLE/2.4G)・電源管理・LEDドライバ・キースキャン後段の `Key_Value_Dispose()` は
`librdrcommon.a` というクローズドソースのライブラリが持っている。再配布しないので
リポジトリには置かず、`scripts/fetch-vendor-blob.py` がベンダー配布の zip から
取り出して `vendor/` に置く (`build.sh` が自動で呼ぶ)。

配布 zip は約1GB あるが、必要なのは 2 ファイル・計 97KB だけなので、
HTTP の Range リクエストで該当部分だけ抜いている (転送量 4.5MB / 10秒程度)。

### QMK コアにパッチが要る

ブロブは HID レポートの送出経路を QMK 標準から丸ごと差し替える。
そのため `patches/qmk-core-rdr-lib.patch` を当てないとビルドできない。
また ES32 のクロックと USB まわりに `patches/es32-fs026-geonix41.patch` が要る。

ブロブは単一オブジェクトなので、リンクすると `del_key_from_report()` のような
コア関数の実装まで一緒に入ってくる。パッチ側では同名の定義を落としてあり、
**この状態で他の機種をビルドすると未定義参照で落ちる**。
`scripts/entrypoint.sh` が他の3機種を先にビルドし、最後に geonix41 をやるのはこのため。

### rules.mk と config.h を持つ

ES32 は QMK が知らない MCU なので、`keyboard.json` だけではボード設定を書けない。
Windmill でこの2ファイルを持つのはこの機種だけ。

### qmk lint を `--strict` で走らせていない

`Option "debounce" duplicates default value of "5"` が出るが、これは誤検知。
`quantum/debounce/asym_eager_defer_pk.c` は `DEBOUNCE` 未定義でも 5 に
フォールバックする一方、`quantum/bootmagic/bootmagic.c` は `BOOTMAGIC_DEBOUNCE` を
`DEBOUNCE * 2` で作り、`DEBOUNCE` が見えないときだけ 30 に落ちる。
消すと起動時 (Esc押し) の待ちが 10ms → 30ms に変わるので、`config.h` から
消してはいけない。

## キーコードの番号

`rdr_lib` が `QK_KB_0` から30個、自前のキーコード (無線モード切替など) を並べている。
Windmill の `MY_*` はぶつからないよう `QK_KB_30` から始める
(`config.h` の `WINDMILL_KEYCODE_BASE`)。ズレたら `geonix41.c` の
`_Static_assert` が検知する。

## LED

キー下 48個 (LED 0〜47) とアンダーグロー 29個 (48〜76)。
Windmill が配色を塗るのはキー下だけで、アンダーグローと電池残量・ペアリング表示は
ベンダー実装 (`User_Led_Show()`) に任せている。順序はベンダー → Windmill なので、
配色が上に乗る。
