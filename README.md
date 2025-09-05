> タカヤ TR3 シリーズ（TR3XM 等）を **LAN（TCP）** で操作する **C++ サンプル**です。  
> **Windows / MSVC** 向け。日本語プロンプトで実行できます。

**動作環境**
- Windows 10/11 (x64)
- Build Tools for Visual Studio 2022（C++ ツール）
- TR3 シリーズ RFID リーダー（LAN 接続）

**使い方（3ステップ）**
1. **ダウンロード**：  
   - 推奨 → [Releases](https://github.com/TamaruNorio/TR3_LAN_CPP/releases)  
   - もしくは [ZIP（main）](https://github.com/TamaruNorio/TR3_LAN_CPP/archive/refs/heads/main.zip)
2. **ビルド**：`build_msvc.bat release`（Debugなら `build_msvc.bat`）  
   成果物は `build\tr3xm_lan.exe`
3. **実行**：`build\tr3xm_lan.exe` を起動 → 画面の日本語プロンプトに従い IP/PORT・読取回数・アンテナ数を入力

**問い合わせ**：不具合・質問は [Issues](https://github.com/TamaruNorio/TR3_LAN_CPP/issues) へお願いします。



# TR3_LAN_CPP – タカヤ TR3 シリーズ RFID リーダー LAN サンプル（C++）

タカヤ製 TR3 シリーズ（例：TR3XM）を **LAN（TCP）経由**で操作する最小サンプルです。  
エンドユーザー（C++ の基本知識あり）向けに、ビルド～実行までの手順と、使い方・トラブル対応をまとめています。

## 特長

- Windows（MSVC）向けの**単純で読みやすい構成**
- `protocol.hpp / protocol.cpp` による **STX…CR 形式の厳密な構文解析**
- `main.cpp` は **日本語プロンプト**で操作、**読取回数は引数でも指定可**
- **ビルド成果物は build フォルダに集約**（.exe / .obj / .pdb）

---

## フォルダ構成

```
TR3_LAN_CPP/
├─ include/
│   └─ tr3/
│       └─ protocol.hpp        … 通信プロトコル定義（STX/ETX/SUM/CR）
├─ src/
│   ├─ main.cpp                … 実行エントリ（日本語プロンプト）
│   ├─ client.cpp              … Winsock クライアント（送受信ラッパ）
│   └─ protocol.cpp            … プロトコル実装（構文解析）
├─ build/                      … ビルド成果物（exe / obj / pdb）
├─ .vscode/                    … VSCode 用タスク等（任意）
├─ doc/                        … 各種ドキュメント（最新版はWebからダウンロードのこと）
├─ build_msvc.bat              … ビルド用バッチ（MSVC）
├─ config.txt                  … 前回使用の IP/PORT を保存
└─ read.md（このファイル）
```

> **注意**：`protocol.hpp / protocol.cpp` は他ファイルから利用する前提で完成済みです。基本的に**変更しません**。

---

## 必要環境

- Windows 10/11（64bit）
- **Build Tools for Visual Studio 2022** または Visual Studio 2022（C++ ツール入り）
- VSCode（任意。コマンドプロンプトのみでも可）
- TR3 シリーズ リーダー（LAN モジュール搭載）、IP/PORT が分かること

---

## セットアップ

1. **ビルドツールの確認**  
   「スタート」→「Visual Studio 2022」→  
   **x64 Native Tools Command Prompt for VS** があることを確認。

2. **バッチの VS パスを確認**  
   `build_msvc.bat` 冒頭にある `VSDEV_DIR` が環境と一致しているか確認してください。  
   例（Build Tools 既定インストール）：  
   `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools`  
   異なる場合は **そのフォルダに合わせて書き換え**てください。

3. **ネットワーク**  
   - リーダーの **IP アドレス** と **ポート番号**（例：9004）を把握。  
   - PC と同一ネットワーク上、ファイアウォールでポートがブロックされていないことを確認。

---

## ビルド方法

### 1) コマンドライン

```
> build_msvc.bat          ← Debug ビルド（既定）
> build_msvc.bat release  ← Release ビルド
> build_msvc.bat clean    ← 生成物の削除
```

- 成果物：`build\tr3xm_lan.exe`
- `.obj/.pdb` も `build\` に出力されます。

### 2) VSCode（任意）

- `Ctrl+Shift+B` で `tasks.json` の「build (batch)」が実行されます。  
- 必要なら Release/Clean タスクを `tasks.json` に追加してください（任意）。

---

## 実行方法

### 基本

```
> build\tr3xm_lan.exe
```

起動後、**日本語のプロンプト**に従って IP / PORT を入力します。  
入力が無ければ `config.txt` の前回値（初回は既定値）を使用します。

### 読取回数を引数で与える（任意）

```
> build\tr3xm_lan.exe 5    ← 読取回数の既定値を 5 回に
```

> その後のプロンプトで Enter を押せば、その既定回数が使われます。  
> 指定が無ければ 1 回が既定です。

---

## 使い方の流れ

1. 起動 → IP / PORT を入力（Enter で前回値）  
2. 接続成功後、以下を順に実行します：
   - **ROM バージョン確認**
   - **コマンドモード設定**
   - **アンテナ切替（指定本数分）**
   - **Inventory2 実行** → UID 数受信 → 各タグの DSFID / UID を表示  
     （UID は表示時に MSB→LSB へ整形）
   - 1 読取ごとに **ブザー ON**（演出）

3. 完了後、Enter で終了。

### 画面例（短縮）

```
==== TR3XM LAN ツール C++ ====
接続先IPアドレスを入力してください（Enterで前回値: 192.168.0.1）：
ポート番号を入力してください（Enterで前回値: 9004）：
[接続中] 192.168.0.1:9004
[LOG] 接続成功
... ROMバージョンの読み取り ...
読取回数を入力してください（Enterで 1 ）：
接続アンテナ数を入力してください（最大3）：
-- 読取 1/1 --
[アンテナ切替] ANT#0
... Inventory2 ...
UID数 : 2
DSFID : 26
UID   : E0 04 01 50 61 4E 42 2D
...
[終了] 接続を閉じました
Enterキーを押すと終了します...
```

---

## ログの見方

- `[send]`：リーダーへ送信したフレーム（STX..CR を HEX で表示）
- `[recv]`：受信した生フレーム（HEX）
- `[cmt]`：操作の区切りメッセージ（ROM 取得、Inventory2 など）
- `UID/DSFID`：Inventory 応答の要約表示

---

## 実装メモ（拡張したい方向け）

- **プロトコル層**（`protocol.hpp / protocol.cpp`）：  
  STX/ADDR/CMD/LEN/DATA/ETX/SUM/CR の厳密解析。**変更不要**です。
- **クライアント層**（`client.cpp`）：  
  Winsock で 1 バイトずつ受信 → `Parser.push()` → 完成で `take()`/`take_raw()`。
- **エントリ**（`main.cpp`）：  
  日本語プロンプトとログ、ROM→コマンドモード→アンテナ→Inventory2 の流れ。  
  **読取回数はコマンドライン引数で既定値**を与え、最後はプロンプトで確定。

---

## よくある質問（FAQ）

**Q. `.pdb` / `.obj` は必要？**  
A. 実行には不要です。配布時や Git 管理からは除外推奨（`.gitignore` に `/build/` を入れると一括で無視できます）。

**Q. `cl.exe not found` と出る**  
A. `build_msvc.bat` が **Visual Studio の開発者環境**（`VsDevCmd.bat`）を呼び出します。  
   冒頭の `VSDEV_DIR` パスを環境に合わせてください。Build Tools 未導入の場合はインストールが必要です。

**Q. `#include "tr3/xxx.hpp"` のエラー**  
A. 本プロジェクトのビルドは `/I include` を付与しており、`include/tr3/...` が見える想定です。  
   もし相対の `../include/...` を使っているソースがあれば、どちらかに統一してください。

**Q. 実機に接続できない**  
A. IP / PORT、LAN 配線、ファイアウォール、機器のコマンドモード設定を確認してください。

---

## 拡張アイデア（任意）

- CSV ログ保存（時刻 / ANT / UID / DSFID）
- 連続読取モード（間隔 ms、停止キーで終了）
- タイムアウト・リトライ回数のプロンプト化
- 出力パワーやチャネル設定など、コマンドの追加

---

## ライセンス・注意

- 本コードはサンプルです。実運用時は安全対策（タイムアウト処理・再送・ログ管理など）をご検討ください。  
- 仕様は機器やファームウェアにより異なる場合があります。必要に応じてメーカー資料をご参照ください。

---

## 更新履歴（要点）

- `protocol.*`：日本語コメント強化、状態機械の明確化（**変更完了・今後は原則固定**）
- `client.cpp`：コメント充実（挙動不変）
- `main.cpp`：日本語プロンプト維持、**読取回数の引数対応（最小変更）**
- `build_msvc.bat`：**VS 環境固定パス化・絶対パス対応・成果物を build に集約**
- `.gitignore`：`/build/` など生成物の除外
