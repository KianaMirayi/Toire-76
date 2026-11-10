/*
    Sidechain.h — 1176 侧链：分压 → 全波整流 → 二极管偏置 → 双 RC 充放电

    电路依据（Rev G 图纸 / UA 复刻版手册 p.10-11）：
      - 取样：Pre-Amp 输出、Output 旋钮(R23) 之前 —— **反馈式**
      - Ratio A 刀：选通送进侧链的交流信号量（R19/R20/R21/R22 = 68k/56k/56k/47k）
      - Ratio B 刀：给整流二极管加直流偏置 → 决定阈值（R75/R73/R68 = 470/560/1.5k）
      - 整流：Q7-Q10 做一对反相信号 → CR2/CR3 全波整流（等效 |x|）
      - 时间常数：R55 控制 C22 充电、R56 控制 C22 放电（改电阻，不是改电容）

    阈值口径（重要）：
      1965 手册的阈值表是**设备输入端 dBm**（−22…−19）。
      但侧链实际看到的是「输入衰减 + GR 之后」的信号，所以
          侧链阈值(dB) = 手册阈值(dB) + 输入衰减(dB，负数)
      否则 Input 旋钮一拧到 −40 dB，压缩就永远不触发。
      这个换算由 setAttenuationDb() 完成，Compressor76 在 setInputDb() 里同步。

    符号约定（1989 手册 §4.5）：
      整流输出是**正向**直流，"subtracts from the bias on the VVR"
      → 偏置变小 → FET 阻抗变小 → 衰减变大
*/

#pragma once
#include <cmath>
#include <algorithm>

namespace toire76 {

class Sidechain {
public:
    static constexpr int kNumRatios = 5;   // 4:1 / 8:1 / 12:1 / 20:1 / ALL
    static constexpr int kAllIndex = 4;

    // 面板口径（1965 手册 p.7）
    static constexpr double kAttackMinSec = 20e-6;
    static constexpr double kAttackMaxSec = 800e-6;
    static constexpr double kReleaseMinSec = 50e-3;
    static constexpr double kReleaseMaxSec = 1.1;

    // 各档限幅阈值（设备输入端 dBFS，UAD 实测 2026-11-11，Input 旋钮 0 dB）
    static constexpr double kThresholdDb[4] = {-52.7, -50.2, -48.8, -46.8};   // 实测值再下移 2 dB 抵消模型偏置
    static constexpr double kAllThresholdDb = -44.9;

    // 目标压缩比：**实测等效值**（真机比面板标称更陡，1176 的已知特性）
    // 注意：这里存的是控制律参数，不是面板标称 ratio。
    // 反馈式模型对同一参数会放大 1.8 倍左右，所以用实测等效比除以 1.8 反推：
    //   实测 5.9/10.6/16.4/30/28.6  →  参数 3.2/5.8/9.0/16.5/15.7
    static constexpr double kTargetRatio[5] = {3.2, 5.8, 9.0, 16.5, 15.7};

    // FET 可用行程（dB）：从静态 1 dB 到满控制约 29 dB
    static constexpr double kFetSpanDb = 29.0;

    void prepare(double sampleRate) {
        sr_ = sampleRate;
        reset();
    }

    void reset() { vc_ = 0.0; vcFast_ = 0.0; vcSlow_ = 0.0; }

    void setRatio(int idx) {
        idx = std::clamp(idx, 0, kNumRatios - 1);
        ratio_ = idx;
        thrDb_ = (idx == kAllIndex) ? kAllThresholdDb : kThresholdDb[idx];
        ratioTarget_ = kTargetRatio[idx];
        feed_ = 1.0;
        updateBias();
    }

    /** 输入衰减（dB，负数）：把"设备输入端阈值"换算到侧链所在的电平 */
    void setAttenuationDb(double dB) {
        attenDb_ = dB;
        updateBias();
    }

    /** 面板 0..1（0=最慢/最小，1=最快/最大）；对数插值 */
    /**
     * 面板 0..1（**顺时针 = 更快**）
     *
     * attack 用**逆向出来的映射**（比测量更可靠）：
     *   UADx 的 algo 里有一张 130×8 的系数表（decrypted.dll @ 0x18143cf70），
     *   索引参数 col7 ∈ [1.60, 2.89] 步进 0.01，正是 log10(attack 微秒)：
     *       10^1.60 =  39.8 µs      10^2.89 = 776 µs
     *   所以：param = 1.60 + knob × 1.29，tau = 10^param (µs)。
     *   （手册标 20–800 µs，实测 39.8–776 µs 落在同一量级）
     *
     * release 用**音频实测**：
     *   1600 ms (n=0) → 85 ms (n=1)
     * release 还带节目相关性：同样的旋钮，长过载的 release 更慢，
     *   倍率 1.08（慢档）→ 3.94（快档），见 kSustainScale()。
     */
    void setAttackRelease(double attackNorm, double releaseNorm) {
        attackNorm = std::clamp(attackNorm, 0.0, 1.0);
        releaseNorm = std::clamp(releaseNorm, 0.0, 1.0);
        attackNorm_ = attackNorm;
        releaseNorm_ = releaseNorm;
        // 逆向映射：log10(µs) = 1.60 + knob × 1.29
        const double log10us = 1.60 + attackNorm * 1.29;
        tauAtk_ = std::pow(10.0, log10us) * 1e-6;
        tauRelFast_ = releaseTauFast(releaseNorm);
        tauRelSlow_ = releaseTauSlow(releaseNorm);
        tauRel_ = tauRelSlow_;
    }

    /**
     * Release 时间常数：直接查**UAD 实测表**（单位秒）
     *
     *   旋钮     短过载(0.02 s)   中(0.5 s)   长过载(2 s)
     *   0.00        1600 ms        1659 ms     1735 ms
     *   0.34        1195 ms        1328 ms     1484 ms
     *   0.67         412 ms         532 ms      800 ms
     *   1.00          85 ms         119 ms      335 ms
     *
     * 短过载用 fast 曲线，持续过载切到 slow 曲线 —— 这就是"节目相关性"。
     */
    static double interpTable(const double* xs, const double* ys, int n, double x) {
        x = std::clamp(x, xs[0], xs[n-1]);
        for (int i = 1; i < n; ++i)
            if (x <= xs[i])
                return ys[i-1] + (ys[i]-ys[i-1]) * (x - xs[i-1]) / (xs[i] - xs[i-1]);
        return ys[n-1];
    }

    // UAD 实测 7 点（秒）：旋钮 0 / .15 / .34 / .50 / .67 / .85 / 1.0
    static constexpr double kN[7] = {0.00, 0.15, 0.34, 0.50, 0.67, 0.85, 1.00};
    // 短过载（0.02 s）
    static constexpr double kFast[7] = {1.689, 1.643, 1.349, 0.954, 0.534, 0.115, 0.114};
    // 长过载（2 s）
    static constexpr double kSlow[7] = {1.768, 1.745, 1.508, 1.215, 0.810, 0.343, 0.338};

    // 1.7 是"查表值 → 我们一阶 RC 实测值"的标定系数
    static constexpr double kReleaseCal = 1.52;

    static double releaseTauFast(double n) { return interpTable(kN, kFast, 7, n) * kReleaseCal; }
    static double releaseTauSlow(double n) { return interpTable(kN, kSlow, 7, n) * kReleaseCal; }

    /** 全波整流 + 二极管偏置（偏置已含输入衰减补偿） */
    double rectify(double x) const {
        return std::max(std::abs(x) * feed_ - bias_, 0.0);
    }

    /** 一阶 RC 充放电：r > vc 走 attack，否则走 release */
    /**
     * 双电容 release（1176 节目相关性的物理来源）
     *
     *   C_fast：充 Attack、放 releaseFast   —— 短瞬态由它主导 → 释放快
     *   C_slow：充慢（~120 ms）、放 releaseSlow —— 长时间压缩时它被充满 → 释放慢
     *   vc = max(vc_fast, vc_slow)
     *
     * 于是同一旋钮位置下：
     *   20 ms 过载  → 慢电容只充到 ~15%，释放由快电容决定（快）
     *   2 s  过载  → 两个都充满，放电时慢电容拖住 → 释放慢（实测差 4 倍）
     */
    double process(double sampleAfterGR) {
        const double r = rectify(sampleAfterGR);
        const double dt = 1.0 / sr_;
        const auto coef = [dt](double tau) {
            return 1.0 - std::exp(-dt / std::max(tau, 1e-9));
        };

        // 快电容
        {
            const double tau = (r > vcFast_) ? tauAtk_ : tauRelFast_;
            vcFast_ += (r - vcFast_) * coef(tau);
        }
        // 慢电容（充电慢，模拟"持续压缩"的累积；放电 = releaseSlow）
        {
            const double tauCharge = std::max(tauAtk_ * 6.0, 0.12);
            const double tau = (r > vcSlow_) ? tauCharge : tauRelSlow_;
            vcSlow_ += (r - vcSlow_) * coef(tau);
        }

        vc_ = std::max(vcFast_, vcSlow_);
        return vc_;
    }

    /**
     * 控制电压 → FET 控制量 u ∈ [0,1]。
     *
     * 两步，各对应电路上一把"刀"：
     *   1) 超阈值多少 dB：dB = 20·log10(1 + vc/bias)        ← B 刀（偏置）定零点
     *   2) 折算成衰减量：GR = (R − 1)·dB                      ← A 刀（环路增益）定斜率
     *
     * 为什么是 (R−1) 不是 (1−1/R)：反馈式里检测器看到的是**输出**，
     * 设 dy/dx = 1/R 需要 f'(y) = R−1；用 (1−1/R) 会塌成 1/(2−1/R)。
     */
    double control(double vc) const {
        if (vc <= 1e-12) return 0.0;
        const double dBabove = 20.0 * std::log10(1.0 + vc / std::max(bias_, 1e-12));
        const double grTarget = (ratioTarget_ - 1.0) * dBabove;
        return std::clamp(grTarget / kFetSpanDb, 0.0, 1.0);
    }

    // ---- 观测 ----
    double tauAttack() const { return tauAtk_; }
    double tauRelease() const { return tauRel_; }
    int ratio() const { return ratio_; }
    double ratioTarget() const { return ratioTarget_; }
    double biasPeak() const { return bias_; }
    double thresholdDb() const { return thrDb_; }

private:
    void updateBias() {
        // bias 是"正弦峰值"单位：0 dBFS 正弦峰值 = 1.0
        // 阈值直接按设备输入端定义：侧链本来就取自衰减之后的信号，
        // 输入衰减会自动把"触发所需输入电平"抬高，不需要再补偿一次
        const double dB = thrDb_;
        bias_ = feed_ * std::pow(10.0, dB / 20.0);
    }

    double sr_ = 48000.0;
    double vc_ = 0.0;
    double vcFast_ = 0.0;
    double vcSlow_ = 0.0;
    double tauRelFast_ = 1.6;
    double tauRelSlow_ = 1.735;
    double attackNorm_ = 0.5;
    double releaseNorm_ = 0.666;
    double tauAtk_ = kAttackMinSec;
    double tauRel_ = kReleaseMaxSec;
    int ratio_ = 0;
    double thrDb_ = kThresholdDb[0];
    double ratioTarget_ = 4.0;
    double attenDb_ = 0.0;
    double feed_ = 1.0;
    double bias_ = 0.1;
};

} // namespace toire76
