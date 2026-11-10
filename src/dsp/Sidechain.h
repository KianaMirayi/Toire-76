/*
    Sidechain.h — 1176 侧链：分压 → 全波整流 → 二极管偏置 → 双 RC 充放电

    电路依据（Rev G 图纸 / UA 复刻版手册 p.10-11）：
      - 取样：Pre-Amp 输出、Output 旋钮(R23) 之前 —— **反馈式**
      - Ratio A 刀：选通送进侧链的交流信号量（R19/R20/R21/R22 = 68k/56k/56k/47k）
      - Ratio B 刀：给整流二极管加直流偏置 → 决定阈值（R75/R73/R68 = 470/560/1.5k）
      - 整流：Q7-Q10 做一对反相信号 → CR2/CR3 全波整流（等效 |x|）
      - 时间常数：R55 控制 C22 充电、R56 控制 C22 放电（改电阻，不是改电容）

    单位约定（重要）：
      侧链以「输入参考幅度」工作（0 dBFS 正弦 = 峰值 1.0）。
      阈值表取自 1965 手册 p.7（dBm，本模型把 0 dBu 视作 0 dBFS 参考）：
          20:1 = −19 dB  → 峰值 = 10^(−19/20)·√2
          12:1 = −20 dB
           8:1 = −21 dB
           4:1 = −22 dB
      二极管偏置直接等于该峰值幅度：|x| 减去它之后才产生控制电压，
      所以阈值以下的 vc 恒为 0（静态就是 1 dB 插入损耗）。

    符号约定（1989 手册 §4.5）：
      整流输出是**正向**直流，"subtracts from the bias on the VVR"
      → 偏置变小 → FET 阻抗变小 → 衰减变大
*/

#pragma once
#include <cmath>
#include <algorithm>
#include <array>

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

    // 各档的限幅阈值（输入参考，dB re 满刻度正弦峰值）【1965 p.7】
    static constexpr double kThresholdDb[4] = {-22.0, -21.0, -20.0, -19.0};

    void prepare(double sampleRate) {
        sr_ = sampleRate;
        reset();
    }

    void reset() { vc_ = 0.0; }

    /** 每档的侧链馈送量（A 刀）。数值由 tests/dsp_test.cpp 自动标定。 */
    static constexpr double kFeedGain[4] = {1.00, 1.00, 1.00, 1.00};
    /** ALL-buttons：偏置串短接 + 最大馈送（UA 只说 12:1–20:1 之间，先取 20:1 的馈送） */
    static constexpr double kAllFeedGain = 1.00;
    static constexpr double kAllThresholdDb = -19.0;

    void setRatio(int idx) {
        idx = std::clamp(idx, 0, kNumRatios - 1);
        ratio_ = idx;
        const double thrDb = (idx == kAllIndex) ? kAllThresholdDb : kThresholdDb[idx];
        feed_ = (idx == kAllIndex) ? kAllFeedGain : kFeedGain[idx];
        bias_ = feed_ * dbToPeak(thrDb);   // B 刀钉住阈值：bias 随 A 刀一起缩放
        // A 刀的等效结果：目标压缩比（4:1 / 8:1 / 12:1 / 20:1；ALL 由 UA 给区间，取 18）
        static constexpr double kTargetRatio[5] = {4.0, 8.0, 12.0, 20.0, 18.0};
        ratioTarget_ = kTargetRatio[idx];
    }

    /** 面板 0..1（0=最慢/最小，1=最快/最大）；对数插值 */
    void setAttackRelease(double attackNorm, double releaseNorm) {
        attackNorm = std::clamp(attackNorm, 0.0, 1.0);
        releaseNorm = std::clamp(releaseNorm, 0.0, 1.0);
        tauAtk_ = kAttackMinSec * std::pow(kAttackMaxSec / kAttackMinSec, attackNorm);
        tauRel_ = kReleaseMinSec * std::pow(kReleaseMaxSec / kReleaseMinSec, releaseNorm);
    }

    /**
     * 全波整流 + 二极管偏置 + 侧链馈送。
     *
     * 顺序很重要：偏置在**馈送之前**减（阈值因此与 A 刀无关），
     * 馈送量只决定环路增益 → 决定压缩比。
     * 这样能同时满足两件事：
     *   · 阈值表（-22/-21/-20/-19 dBFS）不随 ratio 大幅漂移
     *   · A 刀（R19/R20/R21/R22）单独控制"送进侧链多少信号"= 环路增益
     */
    double rectify(double x) const {
        return std::max(std::abs(x) * feed_ - bias_, 0.0);
    }

    /**
     * 一阶 RC 充放电：r > vc 走 attack，否则走 release。
     * 采样率归一化到原始 48k（tau 是物理时间常数，与 sr 无关）。
     */
    double process(double sampleAfterGR) {
        const double r = rectify(sampleAfterGR);
        const double dt = 1.0 / sr_;
        const double tau = (r > vc_) ? tauAtk_ : tauRel_;
        const double alpha = 1.0 - std::exp(-dt / std::max(tau, 1e-9));
        vc_ += (r - vc_) * alpha;
        return vc_;
    }

    /** 控制电压归一化 0..1：满刻度正弦整流后的直流 ≈ 0.637·feed */
    /**
     * 控制电压 → FET 控制量 u ∈ [0,1]，用的是**对数（dB）映射**：
     *   u = log10(1 + vc / vcThreshold) / log10(1 + 10)
     * 于是「输入超过阈值 20 dB」对应 u=1，「超过 10 dB」对应 u=0.5。
     * 这是恒定压缩比的前提：输入 dB → 衰减 dB 线性。
     */
    /**
     * 控制电压 → FET 控制量 u。
     *
     * 分两步，各自对应电路上的一把"刀"：
     *   1) 超阈值多少 dB：dB = 20·log10(1 + vc/bias)      ← 由 B 刀（偏置）定位零点
     *   2) 按压缩比折算成衰减量：GR = (R − 1)·dB             ← 由 A 刀（环路增益）定斜率
     *
     * 注意这里是 (R−1) 而不是 (1−1/R)：
     *   反馈式里检测器看到的是**输出**，设输出 y、衰减 GR=f(y)，
     *   则 dy/dx = 1/(1+f'(y))，要 dy/dx = 1/R 就必须 f'(y) = R−1。
     *   用 (1−1/R) 会得到等效比 ≈ 1/(2−1/R)（4:1 只压出 2.3:1），这是反馈拓扑的常见坑。
     * 再除以 FET 的可用行程（kFetSpanDb）就得到 u。
     * 这样静态曲线天然对齐 1965 手册的阈值表与各档 R，而动态由 RC 决定。
     */
    double control(double vc) const {
        if (vc <= 1e-12) return 0.0;
        const double dBabove = 20.0 * std::log10(1.0 + vc / std::max(bias_, 1e-12));
        const double grTarget = (ratioTarget_ - 1.0) * dBabove;
        return std::clamp(grTarget / kFetSpanDb, 0.0, 1.0);
    }

    double ratioTarget() const { return ratioTarget_; }

    double tauAttack() const { return tauAtk_; }
    double tauRelease() const { return tauRel_; }
    int ratio() const { return ratio_; }
    double feedGain() const { return feed_; }
    double biasPeak() const { return bias_; }

    /** 标定用：直接改馈送量（自动拟合 ratio 斜率时用） */
    void setFeedGain(double g) { feed_ = std::max(g, 1e-6); }

private:
    /** 阈值换算成"正弦峰值"（本模型 0 dBFS 正弦峰值 = 1.0） */
    static double dbToPeak(double dB) { return std::pow(10.0, dB / 20.0); }

    double sr_ = 48000.0;
    double vc_ = 0.0;
    double tauAtk_ = kAttackMinSec;
    double tauRel_ = kReleaseMinSec;
    int ratio_ = 0;
    double feed_ = 1.0;
    double bias_ = 0.1;
    double ratioTarget_ = 4.0;
    static constexpr double kFetSpanDb = 29.0;   // FET 可从 0 压到约 29 dB
};

} // namespace toire76
