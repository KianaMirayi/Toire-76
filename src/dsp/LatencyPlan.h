/*
    LatencyPlan.h — 目标总延迟（samples）的**唯一**计算入口

    为什么单独一个 header：
      · processBlock / prepareToPlay 用它算延迟线长度；
      · tests/dsp_test.cpp（纯 C++17，不链接 JUCE）用它做离线断言。
      两者必须用同一份算术，否则"上报值 = 实际补偿值"迟早会分叉。

    ── 数值来源（实测，2026-11-22 v0.1.0 审计后重测）────────────────────
    参考实现（相位互相关，脉冲输入）：
       44.1 kHz → 86 samples
       48.0 kHz → 86 samples      ← 对外承诺的 86
       96.0 kHz → 118 samples
       192.0 kHz → 151 samples

    注意它**不是** 86 · sr/48000 的线性比例：那个公式在 96/192 kHz 会给
    172 / 344 samples，和参考实现差 54 / 193 samples（跨采样率对齐会整体漂）。
    参考实现的过采样倍数随采样率下降、滤波器阶数随之上升，实测总延迟是
    上面这组"分档"值；分档边界与 PluginProcessor::prepareToPlay 里选
    过采样倍数的边界（60 kHz / 120 kHz）**故意一致**，这样上报值不会在
    换档时跳变。

    ── 与 M1 的关系 ──────────────────────────────────────────────
    v0.1.0 的写法 `86.0 * (sr/48000.0) * 0.0 + 86` 把采样率缩放乘成了 0，
    再用 `extra = target - osLatency + 1` 多补 1 sample → 实际上报 87。
    本文件只负责"目标值"，`extra = max(0, target - osLatency)` 由
    prepareToPlay 负责，两者相加必须**正好**等于 target（那里有 jassert）。
*/

#pragma once

namespace toire76
{
    /** 目标总延迟（samples）。分档与过采样倍数一致（≤60k=4×，≤120k=2×，其余 1×）。 */
    inline int targetLatencySamples (double sampleRate) noexcept
    {
        if (sampleRate <= 60000.0)   return 86;    // 44.1 / 48 kHz 档（实测 86）
        if (sampleRate <= 120000.0)  return 118;   // 88.2 / 96 kHz 档（实测 118）
        return 151;                                // 176.4 / 192 kHz 档（实测 151）
    }
} // namespace toire76
