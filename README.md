# SPIKE-RT DFU WinUSB Setup

WindowsでLEGO SPIKE Prime HubのDFUモードをSPIKE-RT WebUSB書き込みに使えるようにするための、専用WinUSBセットアップツールです。

このツールは汎用USBドライバ変更ツールにはしません。対象をSPIKE Prime HubのDFUモードに限定し、未知のUSB機器やSPIKE-RT実行時のUSBシリアルには触れない設計にします。

## 対象

- LEGO SPIKE Prime / Technic Large Hub のDFUモード
- USB VID: `0x0694`
- USB PID: `0x0008`
- 目的: DFUインターフェースをWindowsのWinUSBで利用できる状態にする

## 対象外

- SPIKE-RT実行中のUSBシリアル / COMポート
- 通常起動中のLEGO Hub
- `0694:0008`以外のUSB機器
- 複数候補が同時に存在して対象を一意に決められない状態

## v0.2 の動作

通常はEXEをダブルクリックして使います。

```text
HubをDFUモードでUSB接続
→ spike-rt-dfu-winusb-setup.exe を起動
→ 0694:0008 を1台だけ検出
→ LEGO Technic Large Hub in DFU Mode であることを確認
→ Service=WinUSB なら「設定済み」と表示して終了
→ 未設定なら現在のドライバ情報を表示
→ ユーザーが INSTALL と入力
→ libwdiでWinUSBを準備・インストール
→ WindowsのUAC/ドライバ確認が必要なら表示
→ 再検出して Service=WinUSB を確認
→ 完了
```

結果を読めるよう、終了前に `Press Enter to close this window...` と表示してEnter入力を待ちます。コマンドラインや自動実行で待機させたくない場合は `--no-pause` を付けます。

`--detect-only` を付けると、検出と現在のドライバ状態の表示だけを行い、WinUSBが未設定でも変更しません。

## 安全方針

ツールはfail-closedで動作させます。

1. `VID=0694 / PID=0008`の接続中デバイスだけを列挙する
2. 0台なら何も変更せず、DFUモードへの入り方を案内する
3. 2台以上なら何も変更せず停止する
4. 1台でも、名称が `LEGO Technic Large Hub in DFU Mode` と一致しなければ停止する
5. WindowsがManufacturerを返した場合はLEGOであることも確認する
6. libwdi側でも独立に `0694:0008` が1件だけであることを再確認する
7. WinUSB以外の変更先は実装しない
8. 実際の変更直前に `INSTALL` の明示入力を要求する
9. インストール後はSetupAPIで `Service=WinUSB` を再確認する
10. SPIKE-RT実行時のCOMポートには変更を加えない

詳細は `docs/SAFETY.md` を参照してください。

## ビルドと配布物

GitHub ActionsはWindows x64用に次を生成します。

- `spike-rt-dfu-winusb-setup.exe`
- `libwdi.dll`
- `libwdi-COPYING-LGPL.txt`
- `THIRD_PARTY_NOTICES.md`
- `SHA256SUMS.txt`

現段階では `spike-rt-dfu-winusb-setup.exe` と `libwdi.dll` を同じフォルダに置いて実行してください。v0.3で配布方法をさらに簡略化する予定です。

## libwdi

WinUSBのINF生成、カタログ署名、ドライバ導入には `pbatard/libwdi` v1.5.1を使用します。GitHub Actionsではrelease commit `9b23b82a2dd1cbffc16d46c212f92c6bf8c0c602` に固定してビルドします。

libwdiはLGPL-3.0-or-laterです。詳細は `THIRD_PARTY_NOTICES.md` とArtifactに同梱する `libwdi-COPYING-LGPL.txt` を参照してください。

## 開発段階

### v0.1 - Detector

実機で `0694:0008`、`LEGO Technic Large Hub in DFU Mode`、`Lego Group`、`Service=WinUSB` を取得できることを確認済みです。

### v0.2 - WinUSB Setup

v0.1のfail-closed検出を維持したまま、未設定の場合だけlibwdiでWinUSBをセットアップします。現在この段階です。

### v0.3 - Distribution

GUI、より分かりやすいエラー表示、GitHub Releasesでの配布、コード署名、Web Toolkitからのダウンロード導線を整備します。

将来的には `spike-rt-web-toolkit` の書き込みページから、このツールのリリース版をダウンロードできるようにする想定です。
