// =============================================
// src/main.cpp
// TR3シリーズ リーダライタ：LAN経由テストツール
// =============================================
// ポリシー：
//  - 既存挙動を変えない（最小変更）
//  - 読取回数は「引数で既定値→プロンプトで最終決定」
//  - プロンプトはすべて日本語のまま
//  - 通信プロトコル層（protocol.hpp/cpp）は変更しない
// =============================================

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <array>
#include <optional>
#include <fstream>

#ifndef NOMINMAX
#define NOMINMAX 1
#endif

// Windowsの古いwinsock.hとの競合を避けるため、必ずwinsock2.hを先にinclude
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

// ↓ 既存のクライアント／プロトコル／ユーティリティをそのまま利用
#include "tr3/client.hpp"
#include "tr3/protocol.hpp"
#include "tr3/utils.hpp"

// ---------------------------------------------
// 時刻文字列（mm/dd HH:MM:SS.mmm）
//  ログ表示用の簡易フォーマッタ
// ---------------------------------------------
static std::string now_str() {
    using namespace std::chrono;
    auto tp = system_clock::now();
    auto t  = system_clock::to_time_t(tp);
    auto ms = duration_cast<milliseconds>(tp.time_since_epoch()) % 1000;
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%m/%d %H:%M:%S")
        << "." << std::setw(3) << std::setfill('0') << ms.count();
    return oss.str();
}

// ---------------------------------------------
// バイト列 → "AA BB CC" のHEX文字列
//  ログの見やすさ重視
// ---------------------------------------------
static std::string hex_str(const std::vector<uint8_t>& v) {
    std::ostringstream oss; oss << std::uppercase;
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) oss << " ";
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)v[i];
    }
    return oss.str();
}

// ---------------------------------------------
// ROMバージョン応答の簡易パーサ
//  仕様：先頭0x90、以降に ASCII 数字/記号が並ぶ想定
// ---------------------------------------------
struct RomInfo {
    int         major{};
    int         minor{};
    int         patch{};
    std::string series;
    std::string code;
};

static RomInfo parse_rom(const std::vector<uint8_t>& d) {
    RomInfo r;
    if (d.size() >= 10 && d[0] == 0x90) {
        auto dig = [](uint8_t c){ return (c>='0' && c<='9') ? c - '0' : 0; };
        r.major  = dig(d[1]);
        r.minor  = dig(d[2]) * 10 + dig(d[3]);
        r.patch  = dig(d[4]);
        r.series = { char(d[5]), char(d[6]), char(d[7]) };
        r.code   = { char(d[8]), char(d[9]) };
    }
    return r;
}

// ---------------------------------------------
// Inventory ACK（タグ件数通知）
//  フォーマット例：F0 NN
// ---------------------------------------------
static std::optional<int> parse_uid_count(const std::vector<uint8_t>& d) {
    if (d.size() == 2 && d[0] == 0xF0) return (int)d[1];
    return std::nullopt;
}

// ---------------------------------------------
// Inventory タグ応答（1タグ）
//  CMD=0x49, DATA= [DSFID][UID(8B)]
// ---------------------------------------------
struct TagInfo { uint8_t dsfid{}; std::array<uint8_t,8> uid; };

static std::optional<TagInfo> parse_tag(uint8_t cmd, const std::vector<uint8_t>& d) {
    if (cmd != 0x49 || d.size() != 9) return std::nullopt;
    TagInfo t;
    t.dsfid = d[0];
    for (int i = 0; i < 8; ++i) t.uid[i] = d[1 + i];
    return t;
}

// ------------------------------------------------------------
// main
//  - 既存フロー（設定読込→接続→ROM→コマンドモード→読取ループ）
//  - 読取回数は「引数で既定値→プロンプト最終決定」（最小変更）
// ------------------------------------------------------------
int main(int argc, char** argv) {
    using namespace tr3;

    try {
        // コンソール出力をUTF-8に（日本語ログの文字化け防止）
        SetConsoleOutputCP(CP_UTF8);

        // ---- 設定ファイルから前回値を復元 ----
        std::string ip   = "192.168.0.2";
        int         port = 9004;
        {
            std::ifstream cfg("config.txt");
            if (cfg) { std::getline(cfg, ip); cfg >> port; }
        }

        // ---- 接続先の入力（Enterで前回値）----
        std::cout << "==== TR3XM LAN ツール C++ ====\n";
        std::cout << "接続先IPアドレスを入力してください（Enterで前回値: " << ip << "）：";
        std::string ipIn; std::getline(std::cin, ipIn); if (!ipIn.empty()) ip = ipIn;

        std::cout << "ポート番号を入力してください（Enterで前回値: " << port << "）：";
        std::string portIn; std::getline(std::cin, portIn); if (!portIn.empty()) port = std::stoi(portIn);

        // ---- 設定の保存（次回の既定値）----
        { std::ofstream cfgOut("config.txt"); cfgOut << ip << "\n" << port; }

        // ---- 接続 ----
        std::cout << "[接続中] " << ip << ":" << port << "\n";
        Client cli;
        cli.connect(ip, static_cast<uint16_t>(port), /*timeout_ms*/ 5000);
        std::cout << "[LOG] 接続成功\n";

        // ---- ROMバージョン読み取り ----
        std::cout << now_str() << "  [cmt]   /* ROMバージョンの読み取り */\n";
        auto rep0 = cli.transact(cmd::check_rom_version());
        RomInfo info = parse_rom(rep0.data);
        std::cout << now_str() << "  [cmt]   ROMバージョン : "
                  << info.major << "." << std::setw(2) << std::setfill('0') << info.minor
                  << "." << info.patch << " " << info.series << info.code << "\n";

        // ---- コマンドモード設定 ----
        std::cout << now_str() << "  [cmt]   /* コマンドモード設定 */\n";
        cli.transact(cmd::set_command_mode());

        // ---- 読取回数・アンテナ数 ----
        //  既定値：reads は「引数 argv[1] があればそれを採用（1未満なら1）」→ その後プロンプトで最終決定
        int reads = 1;
        if (argc >= 2) {
            try { reads = std::max(1, std::stoi(argv[1])); } catch (...) { reads = 1; }
        }

        std::string s;
        std::cout << "読取回数を入力してください（Enterで " << reads << " ）：";
        std::getline(std::cin, s);
        if (!s.empty()) reads = std::stoi(s);

        std::cout << "接続アンテナ数を入力してください（最大3）：";
        std::getline(std::cin, s);
        int ants = 1;
        if (!s.empty()) ants = std::max(1, std::min(3, std::stoi(s)));

        // ---- 読取ループ（読取回数 × アンテナ数）----
        for (int i = 0; i < reads; ++i) {
            std::cout << "\n-- 読取 " << (i + 1) << "/" << reads << " --\n";

            for (int a = 0; a < ants; ++a) {
                // アンテナ切替
                std::cout << "[アンテナ切替] ANT#" << a << "\n";
                cli.transact(cmd::switch_antenna(static_cast<uint8_t>(a)));

                // Inventory2（タグ探索）
                std::cout << now_str() << "  [cmt]   /* Inventory2 */\n";
                auto repI = cli.transact(cmd::inventory2());

                // 先頭応答で UID 数を把握（ACK：F0 NN）
                if (auto n = parse_uid_count(repI.data)) {
                    std::cout << now_str() << "  [cmt]   UID数 : " << *n << "\n";

                    // 続くタグ応答（*n 件）を逐次受信・表示
                    for (int k = 0; k < *n; ++k) {
                        auto repTag = cli.receive_only();
                        if (auto t = parse_tag(repTag.cmd, repTag.data)) {
                            // DSFID
                            std::cout << now_str() << "  [cmt]   DSFID : "
                                      << std::hex << std::uppercase
                                      << std::setw(2) << std::setfill('0') << (int)t->dsfid
                                      << std::dec << "\n";
                            // UID（LSB→MSB を表示順にMSB→LSBへ並べ替え）
                            std::vector<uint8_t> uid(t->uid.begin(), t->uid.end());
                            std::reverse(uid.begin(), uid.end());
                            std::cout << now_str() << "  [cmt]   UID   : " << hex_str(uid) << "\n";
                        }
                    }
                }

                // 読み取りごとにブザーを鳴らす（任意演出）
                cli.transact(cmd::buzzer(0x01));
            }
        }

        // ---- 切断 ----
        cli.close();
        std::cout << "[終了] 接続を閉じました\n";

        // ---- 終了待機（ログ確認用）----
        std::cout << "Enterキーを押すと終了します...";
        std::cin.ignore();
        std::cin.get();

    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
    }
}
