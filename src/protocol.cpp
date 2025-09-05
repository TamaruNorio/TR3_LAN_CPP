// =============================================
// src/protocol.cpp
// TR3シリーズ RFIDリーダライタ 通信プロトコル実装
//   - Frame: 送信用フレームの生成（STX〜CR）
//   - Parser: 受信ストリームの構文解析（STX/ETX/SUM/CR検証）
// =============================================

// プロジェクト構成:
//   include/tr3/protocol.hpp
//   src/protocol.cpp
// 相対パスでインクルード（Ctrl+Shift+B でビルドする想定）
#include "../include/tr3/protocol.hpp"

#include <stdexcept>   // runtime_error
#include <cstddef>     // size_t
#include <utility>     // std::move
#include <algorithm>   // std::copy

namespace tr3 {

// ====================================================================
// Frame::encode
// 概要 : フィールド(addr/cmd/data)から STX〜CR の完全フレームを生成
// 形式 : [STX][ADDR][CMD][LEN][DATA...][ETX][SUM][CR]
// 注意 : SUM は STX〜ETX の総和の下位1バイト（SUM自身とCRは含めない）
// ====================================================================
std::vector<uint8_t> Frame::encode() const {
    std::vector<uint8_t> out;
    out.reserve(HEADER_LEN + data.size() + FOOTER_LEN);

    // ---- ヘッダ部 ----
    out.push_back(STX);                                // STX(0x02)
    out.push_back(addr);                               // アドレス(通常0x00)
    out.push_back(cmd);                                // コマンド
    out.push_back(static_cast<uint8_t>(data.size()));  // データ長(LEN)

    // ---- データ部 ----
    out.insert(out.end(), data.begin(), data.end());

    // ---- フッタ部 ----
    out.push_back(ETX);                                // ETX(0x03)

    // SUM: STX〜ETXの総和の下位1B（SUM/CRは含めない）
    const uint8_t sum = calc_sum(out);
    out.push_back(sum);

    out.push_back(CR);                                 // CR(0x0D)

    return out;
}

// ====================================================================
// Frame::calc_sum
// 概要 : 引数の配列（STX〜ETX を想定）の総和を取り、下位1Bを返す
// 例外 : なし（配列長0のときは 0 を返す）
// ====================================================================
uint8_t Frame::calc_sum(const std::vector<uint8_t>& stx_to_etx) {
    uint32_t sum = 0;
    for (uint8_t b : stx_to_etx) {
        sum += b;
    }
    return static_cast<uint8_t>(sum & 0xFF);
}

// ====================================================================
// Parser::push
// 概要 : 受信バイトを1つ積み、状態機械で構文解析を進める
// 戻値 : true  = 完成フレームが内部バッファに揃った（take/take_raw 可）
//        false = まだ未完成（次のバイト待ち）
// 解析順序:
//   1) STX探索
//   2) ヘッダ（ADDR/CMD/LEN）の読込 → LEN確定
//   3) ペイロード（DATA + ETX/SUM/CR）を所要バイト数だけ蓄積
//   4) ETX/CR/総和(SUM)で最終検証（NGならリセットし再同期）
// ====================================================================
bool Parser::push(uint8_t byte) {
    buf_.push_back(byte);

    switch (st_) {
    case State::SEEK_STX:
        if (byte == STX) {
            // STX 検出 → ヘッダ収集へ
            buf_.clear();
            buf_.push_back(byte);
            st_   = State::HEADER;
            need_ = HEADER_LEN - 1; // STX以外のヘッダ残数(ADDR,CMD,LEN)
        } else {
            // ゴミバイトは破棄して STX を待つ
            buf_.clear();
        }
        break;

    case State::HEADER:
        if (--need_ == 0) {
            // ヘッダ4バイトが揃ったので LEN を取得
            if (buf_.size() < static_cast<size_t>(HEADER_LEN)) {
                // 理論上到達しないが保険
                reset();
                break;
            }
            const uint8_t len = buf_[3];

            // DATA(len) + フッタ(ETX,SUM,CR=3B) を待つ
            need_ = static_cast<size_t>(len) + FOOTER_LEN;
            st_   = State::PAYLOAD;
        }
        break;

    case State::PAYLOAD:
        if (--need_ == 0) {
            // ここまでで [STX..CR] の必要バイト数が揃っているはず
            const size_t sz = buf_.size();
            if (sz < static_cast<size_t>(HEADER_LEN + FOOTER_LEN)) {
                // ありえないが保険
                reset();
                break;
            }

            // 最低限のフォーマット検証（末尾CR, ETX位置）
            const bool cr_ok  = (buf_[sz - 1] == CR);
            const bool etx_ok = (buf_[sz - 3] == ETX);

            if (cr_ok && etx_ok) {
                // SUM 検証: 期待SUM=末尾から2番目, 計算SUM=STX〜ETX合計
                const uint8_t sum_expect = buf_[sz - 2];

                // STX〜ETX（SUM/CRは含めない）
                const uint8_t sum_calc =
                    Frame::calc_sum(std::vector<uint8_t>(buf_.begin(), buf_.end() - 2));

                if (sum_expect == sum_calc) {
                    // 完成フレーム
                    st_ = State::FOOTER; // 一応FOOTERへ（呼び出し側は take* で取り出す）
                    return true;
                }
            }

            // どれかが不正 → 受信位置を再同期（STX 探索へ）
            reset();
        }
        break;

    case State::FOOTER:
        // take()/take_raw() 後に reset() される前提。
        // 念のため受信継続で来た場合はリセットして再同期。
        reset();
        break;
    }

    return false;
}

// ====================================================================
// Parser::take
// 概要 : 直近で完成したフレームを構造化して返す（addr/cmd/data）
// 例外 : 未完成の状態で呼ばれた場合は runtime_error
// 備考 : バッファはクリア・状態はSEEK_STXへ戻す（再利用想定）
// ====================================================================
Decoded Parser::take() {
    if (buf_.size() < static_cast<size_t>(HEADER_LEN + FOOTER_LEN)) {
        throw std::runtime_error("Parser::take: フレーム未完成です");
    }

    Decoded d;
    d.addr = buf_[1];
    d.cmd  = buf_[2];

    // DATA 部のみを抽出（LEN バイト）
    const size_t data_len = static_cast<size_t>(buf_[3]);
    const size_t data_beg = static_cast<size_t>(HEADER_LEN);
    const size_t data_end = data_beg + data_len;
    if (buf_.size() < data_end + FOOTER_LEN) {
        throw std::runtime_error("Parser::take: データ長不一致です");
    }
    d.data.assign(buf_.begin() + data_beg, buf_.begin() + data_end);

    // 使い終えたのでリセット
    reset();
    return d;
}

// ====================================================================
// Parser::take_raw
// 概要 : 直近の完成フレームの生データ（STX〜CR）を返す
// 注意 : 取り出し後は内部状態をリセット
// ====================================================================
std::vector<uint8_t> Parser::take_raw() {
    std::vector<uint8_t> raw = buf_;
    reset();
    return raw;
}

// ====================================================================
// Parser::reset
// 概要 : 受信途中のデータをクリアし、STX探索からやり直す
// 用途 : フォーマット不正/タイムアウト後の再同期など
// ====================================================================
void Parser::reset() {
    buf_.clear();
    st_   = State::SEEK_STX;
    need_ = 0;
}

} // namespace tr3
