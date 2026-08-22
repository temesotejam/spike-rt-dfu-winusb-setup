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
→ spike-rt-dfu-winusb-setup-v0.2.exe を起動
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

## 非破壊テスト

WinUSB未設定PCが手元になくても、インストール処理以外の経路を検証できるように2つの非破壊モードを用意します。

### `--self-test`

Hubを接続せずに実行できます。

```text
spike-rt-dfu-winusb-setup-v0.2.exe --self-test
```

次を確認します。

- 正しい `0694:0008` + Large Hub DFUメタデータを受理できる
- 誤PIDを拒否できる
- 誤デバイス名を拒否できる
- Windowsがメーカー名を返した場合に誤メーカーを拒否できる
- driverless時の空メーカー値を許容する
- WinUSB状態判定
- 同梱libwdiがWinUSB機能を持つこと
- libwdiが生成する形式と同じUTF-16LE+BOMのINFを正しく読み、`0694:0008` とWinUSBを検出できること

このモードはUSB列挙、INF生成、証明書変更、ドライバ変更を行いません。GitHub Actionsでも毎回実行します。

### `--prepare-only`

DFU Hubを1台接続して実行します。

```text
spike-rt-dfu-winusb-setup-v0.2.exe --prepare-only
```

SetupAPIとlibwdiの両方で対象を再確認したうえで、一時フォルダにWinUSB用INFを生成し、INFに `VID_0694&PID_0008` とWinUSB参照が含まれることを確認します。その後、一時ファイルを削除します。

libwdiの生成INFはUTF-16LE+BOMなので、検証側もUTF-16LEを明示的にデコードしてから内容を確認します。

このモードではlibwdiの `disable_cat=TRUE` / `disable_signing=TRUE` を使用し、`wdi_install_driver()` は呼びません。したがって、ドライバ割り当て、カタログ署名、自己署名証明書の追加は行いません。現在すでに `Service=WinUSB` のPCでも実行できます。

`--detect-only` は従来どおり、検出と現在のドライバ状態の表示だけを行います。

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

- `spike-rt-dfu-winusb-setup-v0.2.exe`
- `libwdi.dll`
- `libwdi-COPYING-LGPL.txt`
- `THIRD_PARTY_NOTICES.md`
- `SHA256SUMS.txt`

`spike-rt-dfu-winusb-setup-v0.2.exe` と `libwdi.dll` は同じフォルダに置いて実行してください。v0.3で配布方法をさらに簡略化する予定です。

## libwdi

WinUSBのINF生成、カタログ署名、ドライバ導入には `pbatard/libwdi` v1.5.1を使用します。GitHub Actionsではrelease commit `9b23b82a2dd1cbffc16d46c212f92c6bf8c0c602` に固定してビルドします。

libwdiはLGPL-3.0-or-laterです。詳細は `THIRD_PARTY_NOTICES.md` とArtifactに同梱する `libwdi-COPYING-LGPL.txt` を参照してください。

## 検証状況

実機で以下を確認済みです。

- `0694:0008` の検出
- `LEGO Technic Large Hub in DFU Mode` / `Lego Group` の取得
- `Service=WinUSB` の判定
- WinUSB設定済みPCで再実行しても何も変更せず終了すること

GitHub Actionsでは、Windows x64ビルドと `--self-test --no-pause` を毎回実行します。self-testにはUTF-16LE+BOMのINFデコード検証も含めます。

**未確認:** `Service!=WinUSB` の実PCに対して、実際に `INSTALL` → WinUSB割り当て → `Service=WinUSB` 再確認まで完了する経路。この検証は未設定PCを利用できるときに行います。

## 開発段階

### v0.1 - Detector

実機で `0694:0008`、`LEGO Technic Large Hub in DFU Mode`、`Lego Group`、`Service=WinUSB` を取得できることを確認済みです。

### v0.2 - WinUSB Setup

v0.1のfail-closed検出を維持したまま、未設定の場合だけlibwdiでWinUSBをセットアップします。設定済み経路は実機確認済みで、新規インストール経路のみ実機未検証です。非破壊のself-test / prepare-onlyを追加して検証範囲を広げています。

### v0.3 - Distribution

GUI、より分かりやすいエラー表示、GitHub Releasesでの配布、コード署名、Web Toolkitからのダウンロード導線を整備します。

将来的には `spike-rt-web-toolkit` の書き込みページから、このツールのリリース版をダウンロードできるようにする想定です。
