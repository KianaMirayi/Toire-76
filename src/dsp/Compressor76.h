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

    void reset() { side_.reset(); vcPrev_ = 0.0; onsetPeak_ = 0.0; capRefPeak_ = 0.0; onsetCap_ = 0.0; }

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
        const double inAbs = std::abs(xin);
        updateOnsetPeak(inAbs);
        const double g = fet_.gain(vcPrev_);
        const double yGr = xin * g;

        const double vc = side_.process(yGr, inAbs, onsetVcCap());
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
        const double inAbs = std::max(std::abs(xl), std::abs(xr));
        const double sc = std::max(std::abs(yl), std::abs(yr));
        updateOnsetPeak(inAbs);
        const double vc = side_.process(sc, inAbs, onsetVcCap());
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
    // ── 音头充电上限（2026-11-23）─────────────────────────────────────
    //
    // 症状：−20 dBFS 硬切、attack=0 时，音头 20–70 ms 的 GR 比稳态高 12.4 dB
    // （参考 −0.8）；听感是"音头先被压死、再放开"。
    //
    // 机理（探针逐样本确认）：反馈取样在 GR 之后，阶跃到来时 GR 还没建立，
    // 侧链看到的是**干电平**（比稳态需求大 1–2 个数量级），两个电容都朝它冲，
    // vc 在 1 ms 内撞到 anti-windup 天花板（u=1，≈40 dB），而环路真正的稳态
    // 需求只有 21.7 dB；等 GR 建立、r 塌下去，vc 只能按 release（≈0.7 s）
    // 慢慢退回 —— 这 300 ms 的"深压 → 回放"就是泵感。
    //
    // 修法：每一刻都用「当前输入峰值 → 环路稳态不动点」算一个 vc 上限，
    // 只允许电容**充到这个上限**（不强制放电，放电仍走 release）。
    // 不动点方程（稳态峰值检波）：
    //     vc = p·gain(u(vc)) − bias
    //     u(vc) = clamp((R−1)·20log10(1+vc/bias)/29, 0, 1)
    // 单调，用二分求解；p 用 50 ms 保持的输入峰值（1 kHz 一个周期只掉 1%）。
    //
    // 为什么不影响静态曲线 / release 标定：
    //   · 稳态时 vc 本来就收敛到同一个不动点，上限不改变结果（只挡"冲过头"）；
    //   · 下降沿 r < vc 时走原来的 release，没有任何强制放电；
    //   · 上限只依赖输入峰值，与 release 旋钮无关。
    static constexpr bool   kOnsetCapEnabled = true;
    static constexpr double kOnsetPeakTauSec = 0.050;   // 输入峰值保持
    static constexpr double kOnsetCapMargin  = 1.02;    // >1 = 给上限留余量

    void updateOnsetPeak(double inAbs)
    {
        if (inAbs > onsetPeak_)
            onsetPeak_ = inAbs;
        else if (onsetPeak_ > 0.0)
            onsetPeak_ -= onsetPeak_ * (1.0 - std::exp(-1.0 / (sr_ * kOnsetPeakTauSec)));
    }

    /** 当前输入峰值对应的「环路稳态 vc」；<0 表示本样本不设限 */
    double onsetVcCap()
    {
        if (! kOnsetCapEnabled)
            return -1.0;
        const double p = onsetPeak_;
        if (p <= 0.0)
        {
            capRefPeak_ = 0.0;
            onsetCap_ = 0.0;
            return 0.0;
        }
        // 峰值每变 0.2 dB 才重算（稳态下每周期都会有极小的新峰）
        if (capRefPeak_ > 0.0)
        {
            const double dDb = 20.0 * std::log10(p / capRefPeak_);
            if (std::abs(dDb) < 0.2)
                return onsetCap_;
        }
        capRefPeak_ = p;

        const double bias = std::max(side_.biasPeak(), 1e-12);
        const double rt   = side_.ratioTarget();
        const double span = Sidechain::kFetSpanDb;
        const double floorVc = 0.0;
        const double ceilVc  = side_.vcCeil();

        const auto residual = [&] (double vc)
        {
            const double dBabove = 20.0 * std::log10(1.0 + vc / bias);
            const double u = std::clamp((rt - 1.0) * dBabove / span, 0.0, 1.0);
            return p * fet_.gain(u) - bias - vc;
        };

        double cap;
        if (residual(floorVc) <= 0.0)
        {
            cap = 0.0;                    // 连阈值都没到：不需要压缩
        }
        else if (residual(ceilVc) > 0.0)
        {
            cap = ceilVc;                 // 太热：满行程都不够，交给 anti-windup
        }
        else
        {
            double lo = floorVc, hi = ceilVc;
            for (int i = 0; i < 40; ++i)
            {
                const double mid = 0.5 * (lo + hi);
                if (residual(mid) > 0.0) lo = mid; else hi = mid;
            }
            cap = 0.5 * (lo + hi);
        }
        onsetCap_ = cap * kOnsetCapMargin;
        return onsetCap_;
    }

    // 整机增益：45 dB（手册）+ 1.96 dB 标定差 - 2.71 dB（低电平段 make-up 对齐参考插件）
    // 2026-11-23：再 −1.20 dB。音头充电上限生效后，GR 比原来浅 ~1.2 dB，
    // 素材整体电平随之抬高；实测 16 段素材的 RMS 偏差从 +1.21 dB 回到 +0.01 dB
    // （crest 不受影响）。这一项只改绝对电平，不改 GR / 压缩比 / 释放 / 延迟。
    static constexpr double kTotalGainDb = 44.05;   // 整机 45 dB ±1【1965 p.7】

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
    double onsetPeak_ = 0.0;      // 输入峰值保持（50 ms）→ 算充电上限
    double capRefPeak_ = 0.0;     // 上次算上限用的峰值
    double onsetCap_ = 0.0;       // 当前充电上限（vc 单位）
    bool   bypassComp_ = false;

    FetModel fet_;
    Sidechain side_;
};

} // namespace toire76
