# SPIKE-RT DFU WinUSB Setup

WindowsでLEGO SPIKE Prime HubのDFUモードをSPIKE-RT WebUSB書き込みに使えるようにするための、専用WinUSBセットアップツールです。

汎用USBドライバ変更ツールにはせず、対象をSPIKE Prime / Technic Large HubのDFUモードに限定します。SPIKE-RT実行時のUSBシリアル / COMポートや、他のUSB機器には触れません。

## 対象

- LEGO SPIKE Prime / Technic Large Hub のDFUモード
- USB VID: `0x0694`
- USB PID: `0x0008`
- 目的: DFUインターフェースをWindowsのWinUSBで利用できる状態にする

## 通常の使い方 - v0.3 GUI

配布ZIPをフォルダへ展開し、フォルダ構成を変更せずに次のEXEをダブルクリックします。

```text
SPIKE-RT-DFU-WinUSB-Setup-v0.3.exe
```

GUIは起動すると自動的にDFU Hubを確認します。

```text
HubをDFUモードでUSB接続
        ↓
GUI EXEをダブルクリック
        ↓
0694:0008 を1台だけ確認
        ↓
LEGO Technic Large Hub in DFU Mode であることを確認
        ↓
┌─ Service=WinUSB ─→ 「準備完了」
│
└─ 未設定 ─────────→ 「WinUSBを設定」ボタン
                         ↓
                    確認ダイアログ
                         ↓
                    WinUSBセットアップ
                         ↓
                    Service=WinUSB を再確認
```

GUIに汎用USBデバイス選択画面はありません。操作は基本的に `再確認`、必要な場合だけ `WinUSBを設定`、`閉じる` の3つです。

## v0.3 の内部構成

ユーザーが起動するのはルートのGUI EXEだけです。実際のUSB検出・安全判定・WinUSB処理は、これまで実機確認してきたCLIを `runtime` 配下のworkerとしてそのまま利用します。

```text
SPIKE-RT-DFU-WinUSB-Setup-v0.3.exe
runtime/
  spike-rt-dfu-winusb-worker.exe
  libwdi.dll
libwdi-COPYING-LGPL.txt
THIRD_PARTY_NOTICES.md
README.md
SHA256SUMS.txt
```

GUI側でドライバ変更ロジックを複製しないため、CLIで確認してきたfail-closed判定がそのまま最終権限を持ちます。

## 安全方針

workerは次の条件を満たさない限りドライバ変更へ進みません。

1. `VID=0694 / PID=0008` の接続中デバイスだけを対象にする
2. 0台なら何も変更せず停止する
3. 2台以上なら何も変更せず停止する
4. 名称が `LEGO Technic Large Hub in DFU Mode` と一致しなければ停止する
5. WindowsがManufacturerを返した場合はLEGOであることを確認する
6. libwdi側でも独立に `0694:0008` が1件だけであることを確認する
7. WinUSB以外の変更先を実装しない
8. GUIではインストール前に確認ダイアログを表示し、worker側でも既存の `INSTALL` 確認を通す
9. インストール後はSetupAPIで `Service=WinUSB` を再確認する
10. SPIKE-RT実行時のCOMポートには変更を加えない

詳細は `docs/SAFETY.md` を参照してください。

## 開発者向け非破壊テスト

通常利用ではコマンドライン操作は不要です。以下は検証用です。

### `--self-test`

worker単体で、Hubを接続せず安全判定ロジックをテストできます。GitHub Actionsでも毎回実行します。

確認項目には、正しいDFUメタデータの受理、誤PID・誤名称・誤メーカーの拒否、WinUSB状態判定、libwdiのWinUSB対応、UTF-16LE+BOMのINFデコードが含まれます。

### `--prepare-only`

DFU Hubを1台接続した状態で、ドライバを変更せずlibwdiのWinUSB INF生成経路まで検証します。

- `disable_cat=TRUE`
- `disable_signing=TRUE`
- `wdi_install_driver()` は呼ばない
- 生成INFの `VID_0694&PID_0008` とWinUSB参照を確認
- 一時ファイルは検証後に削除

libwdiはINFをUTF-16LE+BOMで生成するため、検証側もUTF-16LEとして明示的にデコードします。

## libwdi

WinUSBのINF生成、カタログ署名、ドライバ導入には `pbatard/libwdi` v1.5.1を使用します。GitHub Actionsではrelease commit `9b23b82a2dd1cbffc16d46c212f92c6bf8c0c602` に固定してビルドします。

libwdiはLGPL-3.0-or-laterです。Artifactには `libwdi-COPYING-LGPL.txt` と `THIRD_PARTY_NOTICES.md` を同梱します。

## 検証状況

実機で確認済み:

- `0694:0008` の検出
- `LEGO Technic Large Hub in DFU Mode` / `Lego Group` の取得
- `Service=WinUSB` の判定
- WinUSB設定済みPCでは何も変更せず終了すること
- `--prepare-only` でlibwdiが生成した実INFを正しく検証できること
- `--prepare-only` 実行後も既存ドライバを変更しない経路

GitHub Actionsで確認済み:

- Windows x64 workerビルド
- libwdi v1.5.1ビルド
- worker `--self-test`
- v0.3 Win32 GUIビルド
- GUI + `runtime` worker + `libwdi.dll` の配布パッケージ生成

まだ実機未確認:

- v0.3 GUIが設定済みPCで自動的に「準備完了」を表示する経路
- `Service!=WinUSB` の実PCで、実際にWinUSBを新規設定して `Service=WinUSB` まで再確認する経路

## 開発段階

### v0.1 - Detector

対象DFU Hubと現在のWindowsドライバ状態を安全に読み取る検出器。

### v0.2 - WinUSB Setup

fail-closed検出を維持したまま、未設定の場合だけlibwdiでWinUSBを設定するCLI。設定済み経路と非破壊prepare-onlyは実機確認済みです。

### v0.3 - GUI / Distribution

CLIを安全なworkerとして残し、通常利用をWindows GUIへ移行します。次にGUI実機確認、GitHub Releases配布、コード署名、`spike-rt-web-toolkit` からのダウンロード導線を整備します。
