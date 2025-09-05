// =============================================
// protocol.hpp
// タカヤ製 TR3 シリーズ RFIDリーダライタ制御用ヘッダ
// =============================================
//
// 本ヘッダは、TR3シリーズリーダライタと上位機器間で
// コマンド送信・レスポンス受信を行うための通信プロトコルを定義する。
//
// フレーム構造（通信プロトコル仕様書より）
//   [STX][ADDR][CMD][LEN][DATA...][ETX][SUM][CR]
//    1B    1B    1B   1B   0-255B   1B    1B   1B
//
// - STX(0x02): 開始バイト
// - ADDR: アドレス（通常0x00）
// - CMD : コマンドコード
// - LEN : データ長
// - DATA: 可変データ部
// - ETX(0x03): 終了バイト
// - SUM: STX〜ETXまでの総和（下位1B）
// - CR (0x0D): 終端バイト
//
// =============================================

#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>

namespace tr3 {

// ================================================================
// 固定値定義（仕様準拠）
// ================================================================
inline constexpr uint8_t STX = 0x02;   // Start of Text
inline constexpr uint8_t ETX = 0x03;   // End of Text
inline constexpr uint8_t CR  = 0x0D;   // Carriage Return

inline constexpr int HEADER_LEN = 4;   // ヘッダ部: STX, ADDR, CMD, LEN
inline constexpr int FOOTER_LEN = 3;   // フッタ部: ETX, SUM, CR

// ================================================================
// Frame 構造体
//   - 送信コマンドを組み立てるための単位
//   - encode() で STX〜CR の生バイト列を生成する
// ================================================================
struct Frame {
    uint8_t addr{};              // アドレス（デフォルト 0x00）
    uint8_t cmd{};               // コマンドコード
    std::vector<uint8_t> data;   // データ部（可変長）

    // ------------------------------------------------------------
    // 関数: encode
    // 概要: Frame構造体を STX〜CR のフレームに変換
    // 戻値: 完全フレーム（バイト配列）
    // ------------------------------------------------------------
    std::vector<uint8_t> encode() const;

    // ------------------------------------------------------------
    // 関数: calc_sum
    // 概要: STX〜ETXまでの総和を計算し、下位1バイトを返す
    // 引数: stx_to_etx - STX〜ETXまでのバイト列
    // 戻値: 計算されたSUM値（1バイト）
    // ------------------------------------------------------------
    static uint8_t calc_sum(const std::vector<uint8_t>& stx_to_etx);
};

// ================================================================
// Decoded 構造体
//   - 受信したフレームを解析した結果を保持する
// ================================================================
struct Decoded {
    uint8_t addr{};              // アドレス
    uint8_t cmd{};               // コマンドコード
    std::vector<uint8_t> data;   // データ部（LENバイト）
};

// ================================================================
// Parser クラス
//   - ストリーミング入力を解析する状態機械
//   - 1バイトずつ push() で流し込み、フレーム完成時に true
//   - 完成したら take() または take_raw() で取り出す
// ================================================================
class Parser {
public:
    // ------------------------------------------------------------
    // 関数: push
    // 概要: 受信した1バイトを内部バッファに追加し解析を進める
    // 引数: byte - 受信した1バイト
    // 戻値: true = 完成フレームあり, false = 未完成
    // ------------------------------------------------------------
    bool push(uint8_t byte);

    // ------------------------------------------------------------
    // 関数: take
    // 概要: 完成済みフレームを Decoded 構造体に変換して返す
    // 戻値: Decoded - addr, cmd, data を格納
    // 例外: フレーム未完成の場合 runtime_error を投げる
    // ------------------------------------------------------------
    Decoded take();

    // ------------------------------------------------------------
    // 関数: take_raw
    // 概要: 完成済みフレームの生バイト列 (STX〜CR) を返す
    // 戻値: 完成フレームの生データ
    // ------------------------------------------------------------
    std::vector<uint8_t> take_raw();

    // ------------------------------------------------------------
    // 関数: reset
    // 概要: 内部バッファと状態をリセットする
    // ------------------------------------------------------------
    void reset();

private:
    // 状態遷移（状態機械）
    enum class State { SEEK_STX, HEADER, PAYLOAD, FOOTER };
    State st_ = State::SEEK_STX;     // 現在の状態

    std::vector<uint8_t> buf_;       // 内部受信バッファ
    size_t need_ = 0;                // 残り読み取り必要バイト数
};

// ================================================================
// cmd 名前空間
//   - よく使う標準コマンドのビルダー
//   - 返り値は Frame.encode() で生成されたバイト列
// ================================================================
namespace cmd {

    // ROMバージョン確認コマンド
    inline std::vector<uint8_t> check_rom_version(uint8_t addr=0x00) {
        Frame f; f.addr=addr; f.cmd=0x4F; f.data = {0x90};
        return f.encode();
    }

    // コマンドモード設定コマンド
    inline std::vector<uint8_t> set_command_mode(uint8_t addr=0x00) {
        Frame f; f.addr=addr; f.cmd=0x4E;
        f.data = {0x00,0x00,0x00,0x1C};
        return f.encode();
    }

    // アンテナ切替コマンド
    inline std::vector<uint8_t> switch_antenna(uint8_t ant, uint8_t addr=0x00) {
        Frame f; f.addr=addr; f.cmd=0x4E;
        f.data = {0x9C, ant};
        return f.encode();
    }

    // Inventory2 コマンド
    inline std::vector<uint8_t> inventory2(uint8_t addr=0x00) {
        Frame f; f.addr=addr; f.cmd=0x78;
        f.data = {0xF0,0x40,0x01};
        return f.encode();
    }

    // ブザー制御コマンド
    inline std::vector<uint8_t> buzzer(uint8_t onoff=0x01, uint8_t addr=0x00) {
        Frame f; f.addr=addr; f.cmd=0x42;
        f.data = {onoff,0x00};
        return f.encode();
    }
}

} // namespace tr3
