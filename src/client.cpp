// =============================================
// src/client.cpp
// TR3シリーズ - 通信クライアント実装（Windows専用サンプル）
//
// ポリシー：
//  - 既存挙動を変えない（最小変更）
//  - main.cpp / protocol.* への影響なし
//  - 日本語コメントで「何を・なぜ」を明確化
//
// 役割：
//  - Client::connect  : TCP接続の確立（ブロッキング接続）と受信タイムアウト設定
//  - Client::transact : 1コマンド送信 → 1フレーム受信（Parserで厳密構文解析）
//  - Client::receive_only : 受信のみ（次フレームを1つ取り出す）
//  - Client::close    : ソケットクローズ
//
// 注意：
//  - Windows専用（_WIN32）分岐。Linux等では例外を投げて終了します。
//  - 受信は1バイト単位で recv() → Parser.push() し、完成フレームになったら返します。
//  - 受信タイムアウト時は「リトライ回数（retries）」に応じて再送→再受信します。
// =============================================

#include <chrono>
#include <thread>
#include <iostream>
#include "tr3/client.hpp"
#include "tr3/protocol.hpp"
#include "tr3/utils.hpp"

namespace tr3 {

// ------------------------------------------------------------
// コンストラクタ／デストラクタ
//  - Windowsでは WinSock の初期化／後始末を行う
// ------------------------------------------------------------
Client::Client() {
#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        throw NetError("WSAStartup failed");
    }
#endif
}

Client::~Client() {
    close();              // 念のため接続をクローズ
#ifdef _WIN32
    WSACleanup();         // WinSock後始末
#endif
}

// ------------------------------------------------------------
// 関数名 : connect
// 概要   : 指定IP/PORTにTCP接続し、受信タイムアウトを設定
// 引数   : ip         - 接続先IP（例: "192.168.0.10"）
//          port       - ポート番号（例: 9004）
//          timeout_ms - 受信タイムアウト（ミリ秒）
// 例外   : ネットワーク系エラーで NetError を送出
// 備考   : Windows以外は未サポート（例外送出）
// ------------------------------------------------------------
void Client::connect(const std::string& ip, uint16_t port, int timeout_ms) {
#ifdef _WIN32
    // 1) ソケット生成（TCP）
    sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_ == INVALID_SOCKET) {
        throw NetError("socket() failed");
    }

    // 2) 宛先アドレス設定
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        ::closesocket(sock_);
        sock_ = INVALID_SOCKET;
        throw NetError("inet_pton failed");
    }

    // 3) ブロッキング接続（失敗で例外）
    if (::connect(sock_, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        ::closesocket(sock_);
        sock_ = INVALID_SOCKET;
        throw NetError("connect() failed");
    }

    // 4) 受信タイムアウト（SO_RCVTIMEO）設定
    //    recv() が timeout_ms を超えてブロックした場合にタイムアウト扱いになる
    DWORD tv = static_cast<DWORD>(timeout_ms);
    setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#else
    (void)ip; (void)port; (void)timeout_ms;
    throw NetError("Windows only sample");
#endif
}

// ------------------------------------------------------------
// 関数名 : close
// 概要   : ソケットをクローズ（接続を終了）
// ------------------------------------------------------------
void Client::close() {
#ifdef _WIN32
    if (sock_ != INVALID_SOCKET) {
        ::closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }
#endif
}

// ------------------------------------------------------------
// 関数名 : transact
// 概要   : 1コマンド送信 → 1フレーム受信 を行う
// 引数   : frame   - 送信フレーム（protocol::Frame::encode() 済み）
//          retries - 受信タイムアウト時の再送回数（0で再送なし）
// 戻り値 : Reply   - 解析済み CMD, DATA, 受信RAW
// 例外   : 送受信失敗/タイムアウトで NetError を送出
// 挙動   :
//   1) [send] ログを出して全体フレームを送信
//   2) 1バイトずつ recv → Parser.push() で構文解析
//   3) タイムアウト（n==0 or WSAETIMEDOUT）時は retries が残っていれば再送→受信継続
//   4) 完成フレームになったら RAW を確保し、Decoded に変換して [recv] ログ出力
// ------------------------------------------------------------
Client::Reply Client::transact(const std::vector<uint8_t>& frame, int retries) {
#ifdef _WIN32
    // 送信ログ
    std::cout << tr3::ts_now() << "  [send]  " << tr3::hex_spaced(frame) << "\n";

    // 送信
    int sent = send(sock_, (const char*)frame.data(), (int)frame.size(), 0);
    if (sent == SOCKET_ERROR) {
        throw NetError("send() failed");
    }

    // 受信：1バイトずつ Parser に積んで、完成フレームを待つ
    Parser p;
    std::vector<uint8_t> raw;
    for (;;) {
        char c;
        int n = recv(sock_, &c, 1, 0);
        if (n == 1) {
            // 1バイト受信 → 状態機械に投入
            if (p.push((uint8_t)c)) {
                // 完成フレーム（STX..CR）をRAWとして取得
                raw = p.take_raw();   // ★ 先にRAWを確保してから解析へ
                break;
            }
        } else if (n == 0 || (n == SOCKET_ERROR && WSAGetLastError() == WSAETIMEDOUT)) {
            // タイムアウトまたは切断
            if (retries-- > 0) {
                // 指定回数の範囲で再送 → 受信継続
                int sent2 = send(sock_, (const char*)frame.data(), (int)frame.size(), 0);
                if (sent2 == SOCKET_ERROR) {
                    throw NetError("send(retry) failed");
                }
                continue;
            }
            // リトライなし／尽きた → タイムアウト扱い
            throw NetError("recv timeout");
        }
        // それ以外のエラーは SO_RCVTIMEO によってここに来にくい想定
    }

    // 受信RAW → Decoded に変換（addr/cmd/dataを取り出す）
    Parser p2;
    for (auto b : raw) p2.push(b);
    Decoded d = p2.take();

    // 受信ログ（RAWのままを可視化）
    std::cout << tr3::ts_now() << "  [recv]  " << tr3::hex_spaced(raw) << "\n";

    // 呼び出し側がデータ本体とRAWの両方を扱えるように返却
    return Reply{ d.cmd, d.data, std::move(raw) };
#else
    throw NetError("Windows only sample");
#endif
}

// ------------------------------------------------------------
// 関数名 : receive_only
// 概要   : 受信のみ（次に到着したフレーム1つを取り出す）
// 引数   : timeout_ms（未使用。SO_RCVTIMEOに従ってブロック/タイムアウト）
// 戻り値 : Reply（CMD, DATA, RAW）
// 例外   : タイムアウトで NetError("recv timeout (receive_only)")
// ------------------------------------------------------------
Client::Reply Client::receive_only(int /*timeout_ms*/) {
#ifdef _WIN32
    Parser p;
    std::vector<uint8_t> raw;

    for (;;) {
        char c;
        int n = recv(sock_, &c, 1, 0);
        if (n == 1) {
            if (p.push((uint8_t)c)) {
                raw = p.take_raw();   // フレーム完成
                break;
            }
        } else if (n == 0 || (n == SOCKET_ERROR && WSAGetLastError() == WSAETIMEDOUT)) {
            // タイムアウトまたは切断
            throw NetError("recv timeout (receive_only)");
        }
    }

    // RAW → Decoded へ
    Parser p2;
    for (auto b : raw) p2.push(b);
    Decoded d = p2.take();

    // 受信ログ
    std::cout << tr3::ts_now() << "  [recv]  " << tr3::hex_spaced(raw) << "\n";

    return Reply{ d.cmd, d.data, std::move(raw) };
#else
    throw NetError("Windows only sample");
#endif
}

} // namespace tr3
