/*
    Compressor76.h — 1176 整体 L2 灰盒模型

    信号链（Rev D/E 血统，对标 UADx 1176）：
        x → Input(衰减) → GR(FET 分压) → Pre-Amp → Output 增益 → Line Amp → y
                                     ↘ 侧链（反馈取样）→ Vc → FET

    反馈式的代数环：Vc[n] 依赖 y[n]，y[n] 依赖 Vc[n]。
    第一版用「一采样延迟」解（docs/DSP-电路映射.md §四）。

    侧链取样点按「输入参考幅度」表达：物理上是 Pre-Amp 输出，
    因为 Pre-Amp 增益是常数，把它约掉后等效于取 GR 之后的信号，
    这样阈值可以直接用 1965 手册的 dB 数值，不用再换算电压。
*/

#pragma once
#include <cmath>
#include <algorithm>

#include "FetModel.h"
#include "Sidechain.h"

namespace toire76 {

class Compressor76 {
public:
    void prepare(double sampleRate) {
        sr_ = sampleRate;
        fet_.fitDefaults();
        side_.prepare(sampleRate);
        side_.setRatio(ratio_);
        side_.setAttackRelease(attackNorm_, releaseNorm_);
        inputGain_ = dbToLin(inputDb_);
        outputGain_ = dbToLin(outputDb_);
        preampGain_ = dbToLin(kTotalGainDb) / fet_.quiescentGain();
    }

    void reset() { side_.reset(); vcPrev_ = 0.0; }

    // ---- 参数（物理量；面板 taper 待实测后用 PluginProcessor 换算）----
    void setInputDb(double dB) { inputDb_ = dB; inputGain_ = dbToLin(dB); }
    void setOutputDb(double dB) { outputDb_ = dB; outputGain_ = dbToLin(dB); }
    void setRatioIndex(int idx) {
        ratio_ = std::clamp(idx, 0, Sidechain::kNumRatios - 1);
        side_.setRatio(ratio_);
    }
    void setAttackNorm(double n) {
        attackNorm_ = std::clamp(n, 0.0, 1.0);
        side_.setAttackRelease(attackNorm_, releaseNorm_);
    }
    void setReleaseNorm(double n) {
        releaseNorm_ = std::clamp(n, 0.0, 1.0);
        side_.setAttackRelease(attackNorm_, releaseNorm_);
    }
    void setFeedGain(double g) { side_.setFeedGain(g); }   // 标定用
    void setPower(bool on) { power_ = on; }

    /** 单样本处理（基带，单声道） */
    double processSample(double x) {
        if (!power_) return x;

        const double xin = x * inputGain_;          // 1) 输入衰减
        const double g = fet_.gain(vcPrev_);        // 2) GR 级（用上一样本的控制电压）
        const double yGr = xin * g;

        const double vc = side_.process(yGr);       // 3) 反馈侧链（GR 之后取样）
        vcPrev_ = side_.control(vc);

        const double yPre = yGr * preampGain_;      // 4) Pre-Amp
        const double yOut = yPre * outputGain_;     // 5) Output 增益
        return softClip(yOut);                      // 6) Line Amp
    }

    // ---- 观测 ----
    /** 相对静态点的压缩量（dB，0 = 无压缩） */
    double grDb() const { return -db(fet_.gain(vcPrev_) / fet_.quiescentGain()); }
    /** GR 表读数：由 FET 偏置推导，0 = 无压缩、向左偏 */
    double meterGrDb() const { return std::min(grDb(), 20.0); }
    /** 静态插入损耗（设计值 ≈ −1 dB） */
    double quiescentGainDb() const { return db(fet_.quiescentGain()); }
    double attackSeconds() const { return side_.tauAttack(); }
    double releaseSeconds() const { return side_.tauRelease(); }
    double controlVoltage() const { return vcPrev_; }
    FetModel& fet() { return fet_; }
    Sidechain& sidechain() { return side_; }

private:
    static constexpr double kTotalGainDb = 45.0;   // 整机 45 dB ±1【1965 p.7】

    static double dbToLin(double dB) { return std::pow(10.0, dB / 20.0); }
    static double db(double lin) { return 20.0 * std::log10(std::max(lin, 1e-12)); }

    /** Line Amp 上限：最大输出 +24 dBm / 600Ω【1965 p.7】，先做软饱和 */
    static double softClip(double x) {
        constexpr double knee = 1.6;
        const double a = std::abs(x);
        if (a <= knee) return x;
        const double shaped = knee + (1.0 - std::exp(-(a - knee))) * 0.9;
        return std::copysign(shaped, x);
    }

    double sr_ = 48000.0;
    double inputDb_ = 0.0;
    double outputDb_ = -45.0;    // 测试用：与 45 dB 整机增益抵消 → 净增益 0
    double inputGain_ = 1.0, outputGain_ = 1.0, preampGain_ = 1.0;
    double attackNorm_ = 0.5, releaseNorm_ = 0.666;
    int ratio_ = 0;
    bool power_ = true;
    double vcPrev_ = 0.0;

    FetModel fet_;
    Sidechain side_;
};

} // namespace toire76
