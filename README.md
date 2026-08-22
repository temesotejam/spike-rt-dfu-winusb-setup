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

## 安全方針

ツールはfail-closedで動作させます。

1. `VID=0694 / PID=0008`の接続中デバイスを列挙する
2. 0台なら何も変更せず、DFUモードへの入り方を案内する
3. 1台なら製品情報と現在のドライバ状態を確認する
4. 2台以上なら何も変更せず停止する
5. 対象が期待するDFU Hubと確認できた場合だけWinUSBセットアップを許可する
6. SPIKE-RT実行時のCOMポートには変更を加えない

## 開発段階

### v0.1 - Detector

Windows上で対象DFU Hubを安全に検出し、現在のデバイス情報とドライバ情報を表示します。ドライバ変更は行いません。

EXEをダブルクリックしても結果を読めるよう、終了前に `Press Enter to close this window...` と表示してEnter入力を待ちます。コマンドラインや自動実行で待機させたくない場合は `--no-pause` を付けて実行できます。

### v0.2 - WinUSB Setup

v0.1の検出条件を満たした対象だけにWinUSBをセットアップします。

### v0.3 - Distribution

GUI、わかりやすいエラー表示、GitHub ReleasesでのEXE配布、署名方法を整備します。

将来的には `spike-rt-web-toolkit` の書き込みページから、このツールのリリース版をダウンロードできるようにする想定です。
