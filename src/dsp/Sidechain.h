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
    static constexpr double kAllThresholdDb = -50.2;

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

    void reset() { vc_ = 0.0; sustain_ = 0.0; }

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
     * 面板 0..1（**顺时针 = 更快**，与手册 "clockwise fastest" 一致）
     *   attack : 800 µs (n=0) → 20 µs (n=1)      【手册 20–800 µs】
     *   release: 1600 ms (n=0) → 85 ms (n=1)      【UAD 实测 1735 / 85 ms】
     * release 还带节目相关性：同样的旋钮，长过载的 release 更慢，
     *   倍率 1.08（慢档）→ 3.94（快档），见 kSustainScale()。
     */
    void setAttackRelease(double attackNorm, double releaseNorm) {
        attackNorm = std::clamp(attackNorm, 0.0, 1.0);
        releaseNorm = std::clamp(releaseNorm, 0.0, 1.0);
        attackNorm_ = attackNorm;
        releaseNorm_ = releaseNorm;
        tauAtk_ = kAttackMaxSec * std::pow(kAttackMinSec / kAttackMaxSec, attackNorm);
        tauRelFast_ = 0.085 * std::pow(1.60 / 0.085, 1.0 - releaseNorm);
        tauRel_ = tauRelFast_ * sustainScale(releaseNorm);
    }

    /** 节目相关性倍率：长过载 → release 变慢（实测 1.08 / 1.24 / 1.94 / 3.94） */
    static double sustainScale(double n) {
        static const double xs[4] = {0.00, 0.34, 0.67, 1.00};
        static const double ys[4] = {1.08, 1.24, 1.94, 3.94};
        for (int i = 1; i < 4; ++i)
            if (n <= xs[i])
                return ys[i-1] + (ys[i]-ys[i-1]) * (n - xs[i-1]) / (xs[i]-xs[i-1]);
        return ys[3];
    }

    /** 全波整流 + 二极管偏置（偏置已含输入衰减补偿） */
    double rectify(double x) const {
        return std::max(std::abs(x) * feed_ - bias_, 0.0);
    }

    /** 一阶 RC 充放电：r > vc 走 attack，否则走 release */
    double process(double sampleAfterGR) {
        const double r = rectify(sampleAfterGR);
        const double dt = 1.0 / sr_;

        // 过载持续性探测：慢跟随器（≈300 ms），用来判断"这次是短瞬态还是长时间压"
        const double sustainAlpha = 1.0 - std::exp(-dt / 0.3);
        sustain_ += (r - sustain_) * sustainAlpha;

        // 持续过载 → 用慢 release；短瞬态 → 用快 release
        const bool sustained = sustain_ > 0.5 * std::max(vc_, 1e-9);
        const double tauRel = sustained ? tauRel_ : tauRelFast_;
        const double tau = (r > vc_) ? tauAtk_ : tauRel;

        const double alpha = 1.0 - std::exp(-dt / std::max(tau, 1e-9));
        vc_ += (r - vc_) * alpha;
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
        const double dB = thrDb_ + attenDb_;
        bias_ = feed_ * std::pow(10.0, dB / 20.0);
    }

    double sr_ = 48000.0;
    double vc_ = 0.0;
    double sustain_ = 0.0;
    double tauRelFast_ = 0.085;
    double attackNorm_ = 0.5;
    double releaseNorm_ = 0.666;
    double tauAtk_ = kAttackMinSec;
    double tauRel_ = kReleaseMinSec;
    int ratio_ = 0;
    double thrDb_ = kThresholdDb[0];
    double ratioTarget_ = 4.0;
    double attenDb_ = 0.0;
    double feed_ = 1.0;
    double bias_ = 0.1;
};

} // namespace toire76
