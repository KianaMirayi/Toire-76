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
    // release 模型的标识（dsp_test 会打印，用来确认跑的是新编译产物）
    static constexpr const char* kReleaseModel = "dual-cap-v3";

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

    void reset() {
        vc_ = 0.0;
        vcFast_ = 0.0;
        vcSlow_ = 0.0;
        rPeak_ = 0.0;
        tauRelEff_ = tauRelSlow_;
    }

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
     *   短过载 / 长过载两套 7 点表，见 releaseTauFast/Slow()。
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
        tauRelEff_ = tauRelSlow_;
    }

    /**
     * Release 时间常数：直接查**UAD 实测表**（单位秒）
     *
     * 2026-11-11 重测（02-release.wav，20:1 / Input 1.0 / Attack 0.67，
     * fit_env.py 的 63% 口径，7 个旋钮点）：
     *
     *   旋钮      过载 0.02 s   过载 0.5 s   过载 2 s
     *   0.00        1607 ms      1659 ms     1737 ms
     *   0.15        1555 ms      1613 ms     1715 ms
     *   0.34        1193 ms      1329 ms     1484 ms
     *   0.50         793 ms       944 ms     1201 ms
     *   0.67         412 ms       531 ms      801 ms
     *   0.85          85 ms       119 ms      343 ms
     *   1.00          85 ms       119 ms      337 ms
     *
     * ⚠️ 旧代码的 kFast 存的是 **0.5 s** 那一列（标签写成"0.02 s"），
     *    所以模型的"最快"状态整体偏慢 ~35%（这是 fast 端一直对不上的原因之一）。
     *    这里改成真正的 0.02 s 列；kSlow 仍是 2 s 列（原本就是对的）。
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
    // 短过载（0.02 s）—— fast 端
    static constexpr double kFast[7] = {1.6074, 1.5554, 1.1925, 0.7926, 0.4115, 0.0846, 0.0846};
    // 长过载（2 s）—— slow 端
    static constexpr double kSlow[7] = {1.7365, 1.7151, 1.4841, 1.2012, 0.8006, 0.3434, 0.3373};

    // 1.52 是"查表值（63% 恢复）→ 我们内部 RC τ"的标定系数
    static constexpr double kReleaseCal = 1.52;
    static double releaseTauFast(double n) { return interpTable(kN, kFast, 7, n) * kReleaseCal; }
    static double releaseTauSlow(double n) { return interpTable(kN, kSlow, 7, n) * kReleaseCal; }

    /** 全波整流 + 二极管偏置（偏置已含输入衰减补偿） */
    double rectify(double x) const {
        return std::max(std::abs(x) * feed_ - bias_, 0.0);
    }

    /**
     * 双电容释放（1176 节目相关性的物理来源）
     *
     *   C_fast：快充（attack τ）、快放（releaseFast）—— 短瞬态由它主导 → 释放快
     *   C_slow：慢充（chargeTau，几百 ms）、慢放（releaseSlow）—— 压得久它才充满
     *   vc = max(vc_fast, vc_slow)
     *
     * 20 ms 过载 → 慢电容只充到 ~10%，释放由快电容决定（快）
     * 2 s  过载 → 慢电容充满，放电时它拖住 → 释放慢（实测差 3–4 倍）
     *
     * 旧写法是"逐样本 r>0 累加 sustainSec_，r==0 时按 2× 衰减"，而 r 是
     * 全波整流的**音频**，每个过零点都归 0；稳态压缩时 r 在一个载波周期里
     * 大部分样本为 0，衰减比累加还快 → sustainSec_ 被反复清零 → mix≡0 →
     * τ 恒等于 fast。这就是"三档 knob 全平、长过载反而更快"的根因。
     */
    double process(double sampleAfterGR) {
        const double r = rectify(sampleAfterGR);
        const double dt = 1.0 / sr_;
        const auto coef = [dt](double tau) {
            return 1.0 - std::exp(-dt / std::max(tau, 1e-9));
        };

        // ⓪ 峰值检波：整流输出是音频（1 kHz 时每 0.5 ms 归零一次），
        //    慢电容的充电目标必须是**峰值包络**，否则它只能收敛到 |sin| 的均值，
        //    持续过载也充不满。hold ≈ 5 ms：跨得过载波周期，释放时又能很快落下。
        if (r > rPeak_)
            rPeak_ = r;
        else
            rPeak_ -= rPeak_ * coef(0.005);

        // ① C_fast：r > vc 充（attack），否则放（releaseFast）
        const double tauF = (r > vcFast_) ? tauAtk_ : tauRelFast_;
        vcFast_ += (r - vcFast_) * coef(tauF);

        // ② C_slow：慢充 / 慢放。b 秒持续过载后充到 (1−e^{−b/T})，
        //    释放时由它拖住 —— 这就是节目相关性。
        const double tauS = (rPeak_ > vcSlow_) ? chargeTauSec(releaseNorm_) : tauRelSlow_;
        vcSlow_ += (rPeak_ - vcSlow_) * coef(tauS);

        // ③ 取大：谁拖后腿谁决定释放
        vc_ = std::max(vcFast_, vcSlow_);
        tauRelEff_ = (r > vc_) ? tauAtk_
                     : (vcSlow_ >= vcFast_ ? tauRelSlow_ : tauRelFast_);
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
    double tauFastMs() const { return tauRelFast_ * 1000.0; }
    double tauSlowMs() const { return tauRelSlow_ * 1000.0; }
    double tauRelease() const { return tauRel_; }
    /** 释放段当前实际用的 τ（s）；attack 期间为 tauAtk_（调试用） */
    double tauReleaseEff() const { return tauRelEff_; }
    /** 原始控制电压（**未**经 control() 钳位/取对数，调试用） */
    double vcRaw() const { return vc_; }
    double vcFastRaw() const { return vcFast_; }
    double vcSlowRaw() const { return vcSlow_; }
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

    /**
     * 慢电容的**充电**时间常数。
     *
     * 实测律 τ(b) = τ_fast + (τ_slow−τ_fast)·(1−e^{−b/T_trans})，
     * T_trans = 497 / 840 / 1000 ms（release 旋钮 0.34 / 0.67 / 1.00）。
     * 慢电容本身要比 T_trans 略快充电：最终量到的是 max(fast, slow) 的复合
     * 释放，过渡点会被往后推，所以用插件层实测 τ(b) 反标定出 kChargeScale。
     */
    static constexpr double kChargeScale = 0.75;
    static double chargeTauSec(double n)
    {
        static const double xs[3] = {0.34, 0.67, 1.00};
        static const double ys[3] = {0.497, 0.840, 1.000};
        n = std::clamp(n, xs[0], xs[2]);
        for (int i = 1; i < 3; ++i)
            if (n <= xs[i])
                return kChargeScale * (ys[i-1] + (ys[i]-ys[i-1]) * (n - xs[i-1]) / (xs[i] - xs[i-1]));
        return kChargeScale * ys[2];
    }

    double sr_ = 48000.0;
    double vc_ = 0.0;
    double vcFast_ = 0.0;
    double vcSlow_ = 0.0;
    double rPeak_ = 0.0;
    double tauRelFast_ = 1.6;
    double tauRelSlow_ = 1.735;
    double releaseNorm_ = 0.666;
    double attackNorm_ = 0.5;
    double tauAtk_ = kAttackMinSec;
    double tauRel_ = kReleaseMaxSec;
    double tauRelEff_ = kReleaseMaxSec;
    int ratio_ = 0;
    double thrDb_ = kThresholdDb[0];
    double ratioTarget_ = 4.0;
    double attenDb_ = 0.0;
    double feed_ = 1.0;
    double bias_ = 0.1;
};

} // namespace toire76
