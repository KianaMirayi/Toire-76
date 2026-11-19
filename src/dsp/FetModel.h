/*
    FetModel.h — 1176 GR 级的 FET 衰减器模型

    结构（Rev D/E，与 UADx 1176 同血统）：
        x ──[ R5 27k 串联 ]── y
                         │
                        Q1 (FET, 对地)
    增益 = Rf / (R5 + Rf)，Rf 随栅极控制电压 u 变化。

    零信号时 Rf ≈ 220k → gain = 0.891（1 dB 插入损耗，1989 手册 §5.3.2 Q-Bias）。
    满压缩时 Rf ≈ 3.0k → gain = 0.1（20 dB 衰减）。

    曲线形式 Rf(u) = exp(a + b·u + c·u²)：逆向 UAD 时发现其内部即用成对的
    exp(a+bx+cx²) 系数做桥接，说明原厂插件本身就是这个灰盒结构。
*/

#pragma once
#include <cmath>
#include <algorithm>

namespace toire76 {

class FetModel {
public:
    // R5：串联固定电阻（Rev D/E 图上为 27 kΩ）
    static constexpr double kSeriesResistance = 27000.0;

    // 静态工作点：零信号时 1 dB 插入损耗
    static constexpr double kQuiescentRf = 220000.0;   // → gain 0.891

    // 满压缩：20 dB 衰减
    // 2026-11-20 高驱动标定：300Ω → 233Ω。
    //   300Ω 时 GR 上限 = 20log10(0.8907/(300/27300)) = 38.19 dB，而 UADx 在 0/+2 dBFS
    //   仍到 40.32/40.34 dB（16-hot-stairs 实测）→ 热端我们比参考少 2.1 dB GR。
    //   233Ω → 上限 40.36 dB，与 UAD 的 40.3 对齐；u=0..0.9 段的静态曲线不变
    //   （fitDefaults 用 rangeDb 反解 b，整条曲线按同一 shape 重新归一）。
    static constexpr double kMinRf = 233.0;    // → 约 40.36 dB 衰减上限

    // exp(a + b·u + c·u²) 的系数；先按两个锚点 + 中点线性假设占位，待测后替换
    // u=0 → 220k, u=1 → 3k, u=0.5 → 26k（先按对数中点取几何平均）
    void setCurve(double a, double b, double c) {
        a_ = a; b_ = b; c_ = c;
    }

    /**
     * 由三个锚点算 a,b,c：
     *   u=0 → rf0,  u=0.5 → rfHalf,  u=1 → rf1
     * 解：
     *   a = ln rf0
     *   b + c/4 = ln(rfHalf) − a
     *   b + c   = ln(rf1)   − a
     */
    void fitThreePoints(double rf0, double rfHalf, double rf1) {
        const double a = std::log(rf0);
        const double m1 = std::log(rfHalf) - a;   // = b + c/4
        const double m2 = std::log(rf1) - a;      // = b + c
        const double c = (m2 - m1) / 0.75;        // c(1 − 1/4)
        const double b = m1 - c * 0.25;
        a_ = a; b_ = b; c_ = c;
    }

    /**
     * 默认曲线：线性 dB —— u=0 → 220k（1 dB 插入损耗），u=1 → 300Ω（约 29 dB 衰减）。
     * 即在 exp(a+b·u+c·u²) 里取 b = −ln10·(ΔdB/20)、c = 0，
     * 于是每 0.1 的 u 对应约 2 dB 衰减（恒定斜率 = 恒定压缩比的基础）。
     * 实测后用 fitThreePoints() 替三个锚点即可引入曲率。
     */
    void fitDefaults() {
        const double rf0 = kQuiescentRf;
        const double rangeDb = 20.0 * std::log10(rf0 / kMinRf + 1.0);   // ≈ 29 dB
        a_ = std::log(rf0);
        b_ = -std::log(10.0) * (rangeDb / 20.0);
        c_ = 0.0;
    }

    /** u ∈ [0,1] → 漏源等效电阻 Rf */
    double rds(double u) const {
        u = std::clamp(u, 0.0, 1.0);
        const double e = a_ + b_ * u + c_ * u * u;
        double rf = std::exp(e);
        return std::clamp(rf, kMinRf * 0.5, kQuiescentRf * 4.0);
    }

    /** GR 级增益（分压比） */
    double gain(double u) const {
        const double rf = rds(u);
        return rf / (kSeriesResistance + rf);
    }

    /** 反推：给定目标衰减 dB，返回需要的 u（校验用） */
    double uForReductionDb(double reductionDb) const {
        const double targetGain = quiescentGain() * std::pow(10.0, -reductionDb / 20.0);
        double lo = 0.0, hi = 1.0;
        for (int i = 0; i < 40; ++i) {
            const double mid = 0.5 * (lo + hi);
            if (gain(mid) > targetGain) lo = mid; else hi = mid;
        }
        return 0.5 * (lo + hi);
    }

    double quiescentGain() const { return gain(0.0); }

    /**
     * LN 局部反馈：把输出端正的信号按 β 反馈回栅极，
     * 等效于在小信号范围内把 FET 拉回线性区（降低失真）。
     * 第一版只做一个一阶高通耦合的反馈项，β 待实测。
     */
    double lnCorrectedControl(double u, double vds, double beta) const {
        return std::clamp(u + beta * vds, 0.0, 1.0);
    }

private:
    double a_ = 0.0, b_ = 0.0, c_ = 0.0;
};

} // namespace toire76
