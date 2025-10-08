## TAKAYA RFID リーダライタ サンプルプログラム ドキュメント

> **ドキュメントの全体像や他のサンプルプログラムについては、[こちらのランディングページ](https://TamaruNorio.github.io/TAKAYA-RFID-Sample-Docs/cpp/index.md)をご覧ください。**

# TR3_LAN_CPP – タカヤ TR3 シリーズ RFID リーダー LAN サンプル（C++）

タカヤ製 TR3 シリーズ（例：TR3XM）を **LAN（TCP）経由**で操作する最小サンプルです。エンドユーザー（C++ の基本知識あり）向けに、ビルド～実行までの手順と、使い方・トラブル対応をまとめています。

## 概要

このサンプルプログラムは、Windows（MSVC）向けの単純で読みやすい構成で、`protocol.hpp / protocol.cpp`によるSTX…CR形式の厳密な構文解析を特徴としています。`main.cpp`は日本語プロンプトで操作可能で、読取回数は引数でも指定できます。ビルド成果物は`build`フォルダに集約されます。

## 動作環境

-   Windows 10/11 (x64)
-   Build Tools for Visual Studio 2022（C++ ツール）または Visual Studio 2022（C++ ツール入り）
-   TR3 シリーズ RFID リーダー（LAN 接続）、IP/PORT が分かること
-   VSCode（任意。コマンドプロンプトのみでも可）

## セットアップと実行方法

1.  **ビルドツールの確認**: 「スタート」→「Visual Studio 2022」→ **x64 Native Tools Command Prompt for VS** があることを確認してください。
2.  **バッチの VS パスを確認**: `build_msvc.bat`冒頭にある`VSDEV_DIR`が環境と一致しているか確認し、異なる場合はそのフォルダに合わせて書き換えてください。
    例（Build Tools 既定インストール）：`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools`
3.  **ネットワーク設定**: リーダーの **IP アドレス** と **ポート番号**（例：9004）を把握し、PC と同一ネットワーク上、ファイアウォールでポートがブロックされていないことを確認してください。
4.  **リポジトリのクローン**:
    ```bash
    git clone https://github.com/TamaruNorio/TR3_LAN_CPP.git
    cd TR3_LAN_CPP
    ```
5.  **ビルド**: コマンドプロンプトで以下のいずれかを実行します。
    ```
    > build_msvc.bat          ← Debug ビルド（既定）
    > build_msvc.bat release  ← Release ビルド
    > build_msvc.bat clean    ← 生成物の削除
    ```
    成果物は `build\tr3xm_lan.exe` に出力されます。
6.  **実行**:
    ```
    > build\tr3xm_lan.exe
    ```
    起動後、日本語のプロンプトに従って IP / PORT を入力します。入力が無ければ `config.txt` の前回値（初回は既定値）を使用します。読取回数を引数で与えることも可能です（例: `build\tr3xm_lan.exe 5`）。

### 使い方の流れ

1.  起動 → IP / PORT を入力（Enter で前回値）
2.  接続成功後、以下を順に実行します:
    -   **ROM バージョン確認**
    -   **コマンドモード設定**
    -   **アンテナ切替（指定本数分）**
    -   **Inventory2 実行** → UID 数受信 → 各タグの DSFID / UID を表示（UID は表示時に MSB→LSB へ整形）
    -   1 読取ごとに **ブザー ON**（演出）
3.  完了後、Enter で終了。

## プロジェクト構成

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
└─ README.md（このファイル）
```

## 実装メモ

-   **プロトコル層**（`protocol.hpp / protocol.cpp`）：STX/ADDR/CMD/LEN/DATA/ETX/SUM/CR の厳密解析。基本的に**変更不要**です。
-   **クライアント層**（`client.cpp`）：Winsock で 1 バイトずつ受信 → `Parser.push()` → 完成で `take()`/`take_raw()`。
-   **エントリ**（`main.cpp`）：日本語プロンプトとログ、ROM→コマンドモード→アンテナ→Inventory2 の流れ。読取回数はコマンドライン引数で既定値を与え、最後はプロンプトで確定。

## ライセンス

本コードはサンプルです。実運用時は安全対策（タイムアウト処理・再送・ログ管理など）をご検討ください。仕様は機器やファームウェアにより異なる場合があります。必要に応じてメーカー資料をご参照ください。

## 変更履歴

-   `protocol.*`：日本語コメント強化、状態機械の明確化（**変更完了・今後は原則固定**）
-   `client.cpp`：コメント充実（挙動不変）
-   `main.cpp`：日本語プロンプト維持、**読取回数の引数対応（最小変更）**
-   `build_msvc.bat`：**VS 環境固定パス化・絶対パス対応・成果物を build に集約**
-   `.gitignore`：`/build/` など生成物の除外

