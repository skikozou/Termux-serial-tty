# Termux-serial-tty (CDC ACM fork)

[MarkWllms/Termux-serial-tty](https://github.com/MarkWllms/Termux-serial-tty) のforkです。
USB CDC ACMデバイス（iRobot Roomba 980など）への対応を追加しています。

---

## 元リポジトリからの変更点

### 1. `src/cdc_acm.cpp`（新規追加）

USB CDC ACMクラスのドライバ。元リポジトリはFTDI/CH34x/PL2303のみ対応しており、
CDC ACMデバイスは `LIBUSB_ERROR_BUSY (error 6)` で接続できなかった。

対応デバイス：
- iRobot Roomba 980（VID `27a6:0002`）

主な実装内容：
- `SET_LINE_CODING`（ボーレート・データビット・パリティ・ストップビット設定）
- `SET_CONTROL_LINE_STATE`（DTR制御）
- ifc=0（CDC Control）・ifc=1（CDC Data）の両インターフェースをclaimする前にカーネルドライバをdetach

### 2. `examples/ptyserial.cpp`（修正）

PTY作成後、slaveを自分で開いてからUSBにattachするよう修正。

**修正前の問題：** PTYのslaveが誰も開いていない状態でBulk INコールバックが発生すると
`i/o error 5 (EIO)` になりすぐ終了していた。

**修正内容：**
- PTY作成直後に `open(ptyname, O_RDWR | O_NOCTTY)` でslaveを開く
- メインループ終了まで開きっぱなしにする（早期closeするとEIOが再発する）

### 3. `Makefile`（修正）

- `OBJS` に `cdc_acm.o` を追加
- `ptyserial` ターゲットのリンクフラグに `-L./bin` を追加（`-lusbuart` が見つからない問題の修正）

---

## ビルド方法

```bash
pkg install clang libusb pkg-config make git

git clone https://github.com/skikozou/Termux-serial-tty
cd Termux-serial-tty

make
make ptyserial
```

---

## 使い方

### 1. USBパーミッションの取得

```bash
USB_DEV=$(termux-usb -l | tr -d '[]" \n' | tr ',' '\n' | grep -v '^$' | head -1)
termux-usb -r "$USB_DEV"
```

### 2. ptyserialの起動

```bash
TTYDIR="$(pwd)"
USB_DEV=$(termux-usb -l | tr -d '[]" \n' | tr ',' '\n' | grep -v '^$' | head -1)
termux-usb -e "env LD_LIBRARY_PATH=$TTYDIR/bin $TTYDIR/ptyserial 115200" "$USB_DEV"
```

成功すると `/dev/pts/XX` のようなパスが表示される。これを通常のシリアルポートとして使用できる。

---

## 動作確認環境

- デバイス：Sony Xperia 5 III SOG05（Android、非root）
- Termux 0.118 (F-Droid版)
- 接続対象：iRobot Roomba 980

---

## トラブルシューティング

| 症状 | 原因 | 対処 |
|---|---|---|
| `termux-usb -l` が空 | Termux:APIなし／デバイス未認識 | F-Droid版Termux:APIをインストール |
| `error 6 (BUSY)` | CDCドライバ非対応 | このforkを使う |
| `i/o error 5 (EIO)` | PTY slave未オープン | `ptyserial.cpp` のslave_fd修正が入っているか確認 |
| `Status=6` ループ | slave_fdの早期close | ループ終了後にcloseされているか確認 |
| `-lusbuart not found` | `-L./bin` が抜けている | `Makefile` の修正が入っているか確認 |
