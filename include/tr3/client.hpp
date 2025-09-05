// =============================================
// include/tr3/client.hpp
// =============================================
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>

// Windows の max/min マクロ無効化
#ifndef NOMINMAX
#define NOMINMAX 1
#endif

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
#endif

namespace tr3 {

struct NetError : std::runtime_error { using std::runtime_error::runtime_error; };
struct ProtoError : std::runtime_error { using std::runtime_error::runtime_error; };

class Client {
public:
    Client();
    ~Client();
    void connect(const std::string& ip, uint16_t port, int timeout_ms=5000);
    void close();

    // コマンド送信（raw フレーム）→ デコード済み応答を返す
    // retries: タイムアウト時の再送回数
    struct Reply { uint8_t cmd; std::vector<uint8_t> data; std::vector<uint8_t> raw; };
    Reply transact(const std::vector<uint8_t>& frame, int retries=1);
    // ★ 送信せず“次の1フレームだけ”受信（Inventory後のUIDフレーム読取り用）
    Reply receive_only(int timeout_ms = 2000);

private:
#ifdef _WIN32
    SOCKET sock_ = INVALID_SOCKET;
#endif
};

} // namespace tr3
