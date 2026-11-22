/*
    Compressor76.h — 1176 整体 L2 灰盒模型

    信号链（Rev D/E 血统，对标 UADx 1176）：
        x → Input(衰减) → GR(FET 分压) → Pre-Amp → Output 增益 → Line Amp → y
                                     ↘ 侧链（反馈取样）→ Vc → FET

    反馈式的代数环：Vc[n] 依赖 y[n]，y[n] 依赖 Vc[n]。
    第一版用「一采样延迟」解（docs/DSP-电路映射.md §四）。

    立体声：真机是双单声道/可联动；这里用**侧链联动**（取 L/R 较大者），
    避免两声道独立压缩造成声像漂移。
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
        side_.setAttenuationDb(inputDb_);
    }

    void reset() { side_.reset(); vcPrev_ = 0.0; }

    // ---- 参数（物理量；面板 taper 待实测后用 PluginProcessor 换算）----
    void setInputDb(double dB) {
        inputDb_ = dB;
        inputGain_ = dbToLin(dB);
        side_.setAttenuationDb(dB);   // 阈值换算到侧链电平
    }
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
    void setPower(bool on) { power_ = on; }

    /** 压缩旁通（METER 打到 OFF 时用）：音频照过，但增益固定为静态点位，不做 GR */
    void setCompressionBypass(bool b) { bypassComp_ = b; }
    bool compressionBypassed() const { return bypassComp_; }

    /** 单样本处理（单声道，基带） */
    double processSample(double x) {
        if (!power_) return finiteClamp(x);

        if (bypassComp_) {
            vcPrev_ = 0.0;
            return finiteClamp(softClip(x * inputGain_ * fet_.quiescentGain() * preampGain_ * outputGain_));   // OFF 档：无 GR，但保留 make-up（听感与压缩时接近）
        }

        const double xin = x * inputGain_;
        const double g = fet_.gain(vcPrev_);
        const double yGr = xin * g;

        const double vc = side_.process(yGr, std::abs(xin));
        vcPrev_ = side_.control(vc);

        return finiteClamp(softClip(yGr * preampGain_ * outputGain_));
    }

    /** 立体声：侧链取 L/R 较大者，两声道共用同一个控制电压（避免声像漂移） */
    void processStereo(double& l, double& r) {
        if (!power_) {
            l = finiteClamp(l);
            r = finiteClamp(r);
            return;
        }

        if (bypassComp_) {
            vcPrev_ = 0.0;
            const double k = inputGain_ * fet_.quiescentGain() * preampGain_ * outputGain_;
            l = finiteClamp(softClip(l * k));
            r = finiteClamp(softClip(r * k));
            return;
        }

        const double xl = l * inputGain_;
        const double xr = r * inputGain_;
        const double g = fet_.gain(vcPrev_);
        const double yl = xl * g;
        const double yr = xr * g;

        // 侧链输入取两声道峰值的较大者（整流器内部取绝对值）
        const double sc = std::max(std::abs(yl), std::abs(yr));
        const double vc = side_.process(sc, std::max(std::abs(xl), std::abs(xr)));
        vcPrev_ = side_.control(vc);

        const double make = preampGain_ * outputGain_;
        l = finiteClamp(softClip(yl * make));
        r = finiteClamp(softClip(yr * make));
    }

    // ---- 观测 ----
    double grDb() const { return -db(fet_.gain(vcPrev_) / fet_.quiescentGain()); }
    double meterGrDb() const { return std::min(grDb(), 20.0); }
    double quiescentGainDb() const { return db(fet_.quiescentGain()); }
    double attackSeconds() const { return side_.tauAttack(); }
    double releaseSeconds() const { return side_.tauRelease(); }
    double controlVoltage() const { return vcPrev_; }
    double inputDb() const { return inputDb_; }
    double outputDb() const { return outputDb_; }
    FetModel& fet() { return fet_; }
    Sidechain& sidechain() { return side_; }

private:
    // 整机增益：45 dB（手册）+ 1.96 dB 标定差 - 2.71 dB（低电平段 make-up 对齐参考插件）
    static constexpr double kTotalGainDb = 45.25;   // 整机 45 dB ±1【1965 p.7】

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

    /** 输出兜底（等价于 juce::jlimit(-4, 4, y)）。本头文件保持不依赖 JUCE（dsp_test
        是纯 C++17），所以用 std::clamp。±4 = +12 dBFS，远高于软限幅 2.5 的上限：
        正常信号永远碰不到，只有数值爆掉（Inf/NaN）时才生效 —— 宁可一瞬静音，
        也不要把 NaN 交给宿主（传下去会毒化后面整条链路和 DAC）。 */
    static double finiteClamp(double y) {
        if (! std::isfinite(y)) return 0.0;
        return std::clamp(y, -4.0, 4.0);
    }

    double sr_ = 48000.0;
    double inputDb_ = 0.0;
    double outputDb_ = -45.0;    // 与 45 dB 整机增益抵消 → 净增益 0（测试默认）
    double inputGain_ = 1.0, outputGain_ = 1.0, preampGain_ = 1.0;
    double attackNorm_ = 0.5, releaseNorm_ = 0.666;
    int ratio_ = 0;
    bool power_ = true;
    double vcPrev_ = 0.0;
    bool   bypassComp_ = false;

    FetModel fet_;
    Sidechain side_;
};

} // namespace toire76
