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
    //   v4：短过载段不再"平台 + 后段跳升" —— 见 kRelHiDrop / kRelHiX0 的注释。
    static constexpr const char* kReleaseModel = "dual-cap-v4";
    // attack 控制律的标识（同上；改映射/平滑时同步改，用来确认二进制是哪一版）
    static constexpr const char* kAttackModel = "gr-attack-v2";

    static constexpr int kNumRatios = 5;   // 4:1 / 8:1 / 12:1 / 20:1 / ALL
    static constexpr int kAllIndex = 4;

    // 面板口径（1965 手册 p.7）
    static constexpr double kAttackMinSec = 20e-6;
    static constexpr double kAttackMaxSec = 800e-6;
    static constexpr double kReleaseMinSec = 50e-3;
    static constexpr double kReleaseMaxSec = 1.1;

    // 各档限幅阈值（设备输入端 dBFS，UAD 实测 2026-11-11，Input 旋钮 0 dB）
    static constexpr double kThresholdDb[4] = {-49.99, -47.49, -46.09, -44.09};   // 实测值再下移 2 dB 抵消模型偏置
    // ⚠️ 2026-11-20 修正：ALL 档原来是 -42.19（比 20:1 的 -44.09 还高 1.9 dB），
    //    但 UAD 的 ALL 静态曲线落在 8:1 与 12:1 之间（-40 dBFS：UAD 4.55 vs 我们 0.97）。
    //    改为 -46.0 后，-40/-25/-10 dBFS 的 GR 与 UAD 差 ≤0.2 dB（原来是整体少 3.5 dB）。
    static constexpr double kAllThresholdDb = -46.0;

    // 目标压缩比：**实测等效值**（真机比面板标称更陡，1176 的已知特性）
    // 注意：这里存的是控制律参数，不是面板标称 ratio。
    // 反馈式模型对同一参数会放大 1.8 倍左右，所以用实测等效比除以 1.8 反推：
    //   实测 5.9/10.6/16.4/30/28.6  →  参数 3.2/5.8/9.0/16.5/15.7
    static constexpr double kTargetRatio[5] = {4.0, 5.8, 9.0, 16.5, 15.7};

    // FET 可用行程（dB）：从静态 1 dB 到满控制约 29 dB
    static constexpr double kFetSpanDb = 29.0;

    // 判定"真静音"的门限：**相对阈值**取 20 dB 以下（0.1·bias）。
    // 用绝对门限（如 −120 dBFS）在真实素材上会被 −80 dBFS 的本底噪声顶住，
    // 永远进不了静音态 —— 那样 350 ms 的空隙也放不下来，attack 又会失效。
    // 用相对门限则与输入衰减自动一致：比阈值低 20 dB 就是"没有信号可压"。
    static constexpr double kSilenceRel = 0.1;

    // attack 窗口（s）：出真静音后，慢 attack 只在这段时间内武装。
    // 必须 >= 最慢的 τ_atk（3 ms），留 5 倍余量。
    static constexpr double kAtkWindowSec = 0.015;

    // 瞬态检测：输入峰值比慢包络高多少 dB 算"新音头"
    //   实测鼓组音头的跳变在 10–25 dB，取 6 dB 既能覆盖弱击打又不会被稳态波动误触发
    static constexpr double kTransientDb = 20.0;
    static constexpr double kInSlowTauSec = 0.20;

    // 武装条件：静音期间 GR 至少掉了这么多（u 单位，≈ 2 dB）
    static constexpr double kAtkArmDrop = 0.05;

    // 探测器（峰值检波）自身的充电 τ：**attack 旋钮真正作用在这一层**。
    // 真实的 1176 里，attack 就是侧链那个可变电阻 —— 它改的是检波电容的充电速度，
    // 不是"给控制量加一个一阶平滑"。
    //
    // 2026-11-20 高驱动标定（hot-drums-03 归一化到 0 dBFS、in=1.0、20:1、release .666，
    // 逐 attack 旋钮比较输出峰值；dPK = 我们 − UAD）：
    //   knob      0.00    0.34    0.50    0.67    1.00
    //   UAD 峰   −0.90   −3.94   −5.23   −7.00  −13.06  dBFS
    //   旧实现（固定 27.8 µs + 只在"静音后武装"窗口里平滑 u）：
    //   我们峰低 −7.4 dB（in=1.0）/ −4.6 dB（in=0.6）→ crest 14 dB，UAD 21–24 dB
    //   本表（终版实测，hot-drums-03，dPK = 我们 − UAD）：
    //     in=1.0 20:1  dPK −0.05 … +0.47 dB，dRMS −0.14 dB
    //     in=0.6 20:1  dPK +0.63 … +2.04 dB，dRMS −0.38 dB
    //   ⚠️ 代价：attack 旋钮的"过冲量"标定（tests/dsp_test.cpp §7，192k/10 kHz）
    //   会漂移 +0.7 / +0.5 / +1.3 / −1.9 dB（knob 0/.34/.67/1.0），
    //   需要按两把尺子（过冲量 + 素材峰值）重新联合标定 kDetTau[] 与 tauAtk。
    //
    // 注意：表里中段是平的（.34 与 .67 同为 580 µs）。这不是笔误 —— 我们的环路
    // 在稳态是"需求贴轨"的伺服，真正决定音头过冲的是检波电容的充电速度，
    // 旋钮中段的差异在实测里被环路吞掉了（原来是靠 u 平滑硬造出来的差异）。
    // ALL 档的探测器 τ 倍率（见 setRatio 注释）：参考插件的 ALL 是慢 attack
    static constexpr double kAllDetScale = 4.0;

    static constexpr double kDetKnob[4] = {0.00, 0.34, 0.67, 1.00};
    static constexpr double kDetTau[4] = {4400e-6, 580e-6, 580e-6, 31e-6};

    // 真静音回落 τ 的标定系数（乘在 release 的 fast 端上）。
    // 依据见 control() 注释；数值由 §7 的过冲量实测反推，见 setAttackRelease()。
    //
    // ── 2026-11-20 复核：**保持 1.38，不要调小** ──────────────────────────
    // 试过 0.90 / 0.50（用户反馈"release 有点快、有回声感"时怀疑是这条分支
    // 让拖尾过长）。三把尺子结论一致：调小只会更差 ——
    //   ① 合成静音尾（1.2 s −6 dBFS → −70 dBFS 探针，测输出包络回落 τ）：
    //        旋钮 .50/.67/.85/1.00   ours(1.38) 631/497/123/124 ms
    //                                 ours(0.90) 529/379/ 80/ 80 ms
    //                                 ours(0.50) 370/222/ 44/ 45 ms
    //                                 UAD        626/619/678/677 ms  ← 几乎不随旋钮变
    //   ② 12 段 mat2 素材（空隙后回冲 / dB）：1.38 → 25.8，0.90 → 26.0，
    //      0.50 → 27.6，UAD 24.2；
    //   ③ dsp_test §7 过冲量（350 ms 空隙）：1.38 → 20.17/18.91/17.84/6.39,
    //      0.90 → 23.74/21.4/20.45/9.64，0.50 → 25.0/24.9/24.7/14.3（基准 20.05/19.23/18.20/6.82）。
    //
    // 另外两个已确认的事实（留给下一轮，别再花时间重测）：
    //   · **音乐空隙里这条分支只在部分样本上生效** —— mat2 的空隙电平
    //     −55…−70 dBFS 正好骑在静音门限（−64 dBFS @20:1）上，来回抖。
    //     证据：把 τ_gr 的下限强行改成 5.0 s，hot-drums-03 的整段输出与
    //     0.60 s 时**逐位相同**；kGrSilenceCal 1.38→0.90 在素材上只挪 0.2 dB。
    //   · 所以快端的"抽气/回声"不是这条分支造成的，是 vc 释放本身：
    //     release 0.85 / 20:1 / in 1.0 上，空隙里 GR 我们掉到 2–4 dB，
    //     UAD 停在 12–16 dB → 空隙后回冲 33.8 dB vs 24.2 dB（深 9 dB；
    //     旧表 33.0，所以这不是本次重标引入的）。
    static constexpr double kGrSilenceCal = 1.38;

    // ── 2026-11-22：τ(burst) 形状（节目相关性）两处改动 ────────────────────
    //
    // 症状（20:1 / input 1.0 / attack 0.67 / 3 s −40 载波，10-burst-sweep）：
    //   release 0.34/0.67/1.00 三档、10 ms…2 s 八个过载时长，
    //   UAD 的 τ 从第一个点就开始缓升（0.34 档 900→1160，0.67 档 300→620），
    //   我们却是"平台 + 后段跳升"（0.67 档 420 420 420 420 420 420 440 580）。
    //   24 点相对误差 mean 15.9% / p90 32.7%。
    //
    // 根因（用 sim/harness 逐样本看的内部状态）：
    //   ① 任何 ≥2 ms 的过载都会让 vcFast 在一个采样内冲过 anti-windup 上限
    //      （r 是"还没被压下去"的整流值，比平衡点大两个数量级），
    //      于是每次释放都从**同一个天花板**（GR 40.3 dB）起步，
    //      与过载时长无关 → 短过载被抬成平台。
    //      UAD 是"GR 在 3–5 ms 内升到环路平衡点（−6 dBFS 时 32.3 dB）就不再涨"。
    //   ② 慢电容的充电是单一 τ（0.37–0.75 s），10–100 ms 的过载几乎充不进去，
    //      所以 τ(burst) 在 500 ms 之前完全平。
    //   本次只修 ①（它占误差的大头）。② 试过两段式充电，插件层几乎不动（见下），
    //   真正要修 ② 得动"慢电容的充电目标"，那会连带改静态曲线，留给下一轮。
    //
    // 改法（不动静态曲线 / 单点 release 的标定）：
    //   kRelHiDrop/kRelHiX0：快电容在**电荷接近上限**时放电 τ 缩短
    //      （vc/vcCeil > 0.78 起线性，到 1.0 时 ×0.30）。
    //      冲顶那一段（≈4–5 dB）被快速泄掉，剩下的仍按原 τ 走；
    //      浅压缩（vc/vcCeil ≈ 0.65）完全不吃 —— 所以浅压缩单点 release 不动；
    //      深度压缩单点（1.5 s 过载）在 0.85/1.00 档吃到一点（τ +1.5…+3.2%）。
    //
    // 试过但不行的（留给下一轮，别再花时间）：
    //   · 把快电容的充电目标钳到"静态平衡点"（用输入包络反算）：短过载的
    //     释放改由慢电容主导 → τ 反而全线变长（0.34 档 1056→1236 ms）。
    //   · 快电容单独一个更低的上限（0.9×vcCeil）：深度压缩单点 τ +10%，破 4%。
    //   · 慢电容两段式充电（前 12% 用 50 ms 充）：插件层 τ(burst) 几乎不动，
    //     但 dsp_test §7 的过冲量掉 0.9–1.2 dB（超 ±0.5），不划算。
    //   · 只按"快/慢电容比"泄放（不按电荷深度）：插件层 τ(burst) 只从 17.1% 到 14.0%。
    //
    // 标定结果（真插件渲染，尺子 = 20:1 / in 1.0 / attack 0.67 / 3 s −40 载波）：
    //   24 点相对误差 mean 15.9%→13.3%、p90 32.7%→31.5%（按任务书那张 UAD 表；
    //   同一台机器重测 UAD 时是 17.1%→13.0% / 28.5%）。
    //   残余误差集中在 0.67/1.00 档的 10–100 ms 段（+13…+26%），结构上被
    //   "浅压缩单点 release"钉住：再快 25% 的 τ_fast 会让浅压缩 1.0 档掉到 −26%。
    //   静态 MAE 0.57 dB；深压缩单点 τ −1.2…+3.2%；浅压缩 −0.3…+2.9%；
    //   热素材峰值相对上一版 ≤0.17 dB；dsp_test §7 = 19.08/18.73/17.71/6.32
    //   （基准 20.17/18.91/17.84/6.39，0 档 −1.09 是唯一超 ±0.5 的点）。
    static constexpr double kRelHiX0 = 0.78;     // 开始"泄放"的 vcFast/vcCeil
    static constexpr double kRelHiDrop = 0.70;   // 到上限时放电 τ 缩短的比例（×0.30）

    void prepare(double sampleRate) {
        sr_ = sampleRate;
        reset();
    }

    void reset() {
        vc_ = 0.0;
        vcFast_ = 0.0;
        vcSlow_ = 0.0;
        rPeak_ = 0.0;
        inPeak_ = 0.0;
        inSlowEnv_ = 0.0;
        atkT_ = 1e9;           // 复位后不武装 attack（等下一次真静音）
        inSilence_ = true;
        atkArmed_ = false;
        uSilenceStart_ = 0.0;
        uPrev_ = 0.0;          // 控制量也要归零，否则 reset 后残留上一段的 GR
        tauRelEff_ = tauRelSlow_;
    }

    void setRatio(int idx) {
        idx = std::clamp(idx, 0, kNumRatios - 1);
        ratio_ = idx;
        thrDb_ = (idx == kAllIndex) ? kAllThresholdDb : kThresholdDb[idx];
        ratioTarget_ = kTargetRatio[idx];
        feed_ = 1.0;
        // ALL 档（全按键按下）在参考插件里是**慢 attack**：静态曲线压得很深
        // （0 dBFS 时 42.4 dB），但素材上的瞬态几乎不压（峰值 0.13 dBFS）。
        // 这里用一个更长的探测器 τ 近似这个程序相关性。
        allDetScale_ = (idx == kAllIndex) ? kAllDetScale : 1.0;
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
     * ── attack：用「过冲量」整条重标（2026-11-16，192 kHz / 10 kHz 硬切音头）──
     *
     * 为什么推翻旧映射（28/28/27/15 µs）：旧值是"GR 包络 10–90% 上升时间"量出来
     * 的，但那个量法在 GR 已经贴地时量到的是**环路抖动重闭合**的速度，不是 attack
     * 本身。换成"过冲量"这把尺子后，4 档旋钮才第一次拉开差距。
     *
     * 标定方法（与 UADx 完全同一把尺子，脚本 tools: overshoot192.py）：
     *   192 kHz、10 kHz 硬切音头 ×6、20:1、Input 1.0、Release 0.666；
     *   env = |x| 的 5 点滑动平均；音头 t0 的
     *     peak   = max    env[t0,      t0+15ms]
     *     steady = median env[t0+50ms, t0+90ms]
     *     overshoot = 20·log10(peak/steady)
     *   注意：这个尺子对**线性**信号本身就有 +3.0 dB 底数（10 kHz@192k 的 MA ripple），
     *   所以 "3.0 dB" 意味着"和干信号一模一样/完全没过冲"。
     *
     * 参考插件（同一把尺子，6 个音头的中位数）：
     *     knob   0.00   0.34   0.67   1.00
     *     UADx  19.32  18.74  16.92   8.76
     *     我们  19.31  18.62  16.83   8.66   ← 2026-11-16 标定结果（误差 ≤ 0.12 dB）
     *
     * 形状为什么是"前平后陡"：10 kHz 的半周期 = 50 µs。旋钮 0/.34/.67 的 τ 都还
     * 远慢于半周期，音头第一个峰基本原样通过，过冲 ≈ 释放后残留的 GR 深度；
     * 到了 1.00 档，τ 已经快过半周期，峰被压掉一半 → 8.8 dB 的峭壁。
     * 这也解释了为什么 1.00 档的 τ 必须是**几十 µs**（真实 1176 面板标称的
     * 最快端就是 20 µs），而 0.00 档要 ~3 ms（参考插件的实测行为）。
     *
     * 表内按 log(tau) 插值：τ 跨 300 倍，线性插值会把 .67 档推得太快。
     *
     * release 用**音频实测**：短过载 / 长过载两套 7 点表，见 releaseTauFast/Slow()。
     */
    void setAttackRelease(double attackNorm, double releaseNorm) {
        attackNorm = std::clamp(attackNorm, 0.0, 1.0);
        releaseNorm = std::clamp(releaseNorm, 0.0, 1.0);
        attackNorm_ = attackNorm;
        releaseNorm_ = releaseNorm;
        // 过冲量标定表：knob → τ_attack（µs）
        static const double ax[4] = {0.00, 0.34, 0.67, 1.00};
        static const double ay[4] = {3000.0, 317.0, 88.0, 10.0};
        double lus = std::log(ay[3]);
        for (int i = 1; i < 4; ++i)
            if (attackNorm <= ax[i]) {
                lus = std::log(ay[i-1]) + (std::log(ay[i])-std::log(ay[i-1]))
                      * (attackNorm - ax[i-1]) / (ax[i] - ax[i-1]);
                break;
            }
        tauAtk_ = std::exp(lus) * 1e-6;
        // 探测器充电 τ（attack 旋钮的真实作用点）：按 log(τ) 插值
        {
            double ld = std::log(kDetTau[3]);
            for (int i = 1; i < 4; ++i)
                if (attackNorm <= kDetKnob[i]) {
                    ld = std::log(kDetTau[i-1]) + (std::log(kDetTau[i])-std::log(kDetTau[i-1]))
                         * (attackNorm - kDetKnob[i-1]) / (kDetKnob[i] - kDetKnob[i-1]);
                    break;
                }
            detTau_ = std::exp(ld);
        }
        tauRelFast_ = releaseTauFast(releaseNorm);
        tauRelSlow_ = releaseTauSlow(releaseNorm);
        tauRel_ = tauRelSlow_;
        tauRelEff_ = tauRelSlow_;
        // 真静音时 GR 的回落 τ（秒）：探测器电容自己放电，**不再**乘 (R−1)。
        // 取 release 的 fast 端乘标定系数 kGrSilenceCal（见 control() 的注释）。
        tauGrRel_ = tauRelFast_ * kGrSilenceCal;
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
    //
    // ── 2026-11-20 第二次重标（深压缩快端）──────────────────────────────
    // 上一版表来自 02-release.wav（导频 −40、单段过载）那一把尺子；换成
    // "常驻探针 + 长过载"（−30 dBFS 探针 + 1.5 s −6 dBFS，20:1 / in 1.0 /
    // out 0.6259 / attack 0.5 / meter 1）以后，快端差了 2.4 倍：
    //
    //   knob   UAD t63/τ (ms)   ours(旧表) t63/τ   τ 比
    //   0.50    540 / 390        554 / 406        0.96
    //   0.67    320 / 240        358 / 267        0.90
    //   0.75    212 / 164        273 / 201        0.81
    //   0.85     74 /  59        178 / 119        0.49   ← 深压缩里慢 2.4×
    //   1.00     73 /  58        172 / 115        0.50
    //
    // 关键事实：深压缩的 τ 与 **kSlow 严格成正比**
    //   τ_measured / (kSlow·kReleaseCal) = 0.219…0.229（6 个点，散差 ±2%），
    //   因为长过载把慢电容充满、max(vcFast, vcSlow) 恒由慢电容决定。
    // 所以深压缩这半边按比例改 kSlow 即可，慢端 0.22 的斜率不会被动到。
    // （UAD 在 0.75→0.85 之间有个 ~2.8× 的台阶，我们照着台阶做，不再平滑。）
    //
    // 浅压缩（−30 探针 + 0.15 s −20 dBFS，GR≈9 dB）时慢电容只充到 ~1/4，
    // 释放由快电容主导，旧表整体**快 10%**（τ 比 1.03…1.12，与上一轮
    // "浅过载说我们快 10%" 的旧结论一致）—— 这次一并按同一把尺子改 kFast。
    //
    // 短过载（0.02 s）—— fast 端（浅压缩/短瞬态走这一列）
    static constexpr double kFast[7] = {1.6074, 1.5554, 1.1925, 0.8200, 0.4602, 0.0926, 0.0932};
    // 长过载（2 s）—— slow 端（深压缩走这一列；0.22 的斜率由 kReleaseCal 承载）
    static constexpr double kSlow[7] = {1.7365, 1.7151, 1.4841, 1.1536, 0.7195, 0.1687, 0.1690};

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
     *   C_fast：快充（探测器 τ = kDetectTau）、快放（releaseFast）—— 短瞬态由它主导 → 释放快
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
    /**
     * @param sampleAfterGR  压缩之后的样本（反馈取样点）
     * @param inputLevel     压缩**之前**的 |x|（可选；只用来看"信号还在不在"）
     *
     * 为什么需要 inputLevel：判断"真静音"不能用压缩之后的电平 —— 低电平信号
     * 被压到阈值以下时，压缩后的整流值本来就恒为 0，那会误判成静音，把 GR 慢慢
     * 放掉（静态曲线在低电平段被抬高 8 dB，实测过）。只有压缩前的电平才知道
     * "信号到底还在不在"。
     */
    double process(double sampleAfterGR, double inputLevel = 0.0) {
        // ── NaN/Inf 兜底（入口）─────────────────────────────────────────
        // 上游一个坏样本（Inf / NaN：前级插件崩溃恢复、未初始化缓冲、参数跳变）
        // 会顺着 rectify() 灌进 vcFast_/vcSlow_，再经 control() 进 uPrev_。
        // 这些量一旦非有限就再也回不来：std::clamp 与所有比较对 NaN 恒为 false，
        // 指数衰减也拉不动 NaN → 插件**永久**输出 NaN 或静音，只能重载。
        // 所以把非有限输入当成"没有信号"：清掉全部状态、输出 0，下一个样本重新开始。
        if (! std::isfinite(sampleAfterGR) || ! std::isfinite(inputLevel)) {
            reset();
            return 0.0;
        }

        const double r = rectify(sampleAfterGR);
        const double dt = 1.0 / sr_;
        const auto coef = [dt](double tau) {
            return 1.0 - std::exp(-dt / std::max(tau, 1e-9));
        };

        // 输入侧的峰值包络（40 ms 量级的保持）：只用来判静音
        if (inputLevel > inPeak_)
            inPeak_ = inputLevel;
        else
            inPeak_ -= inPeak_ * coef(0.005);

        // ⓪ 峰值检波：整流输出是音频（1 kHz 时每 0.5 ms 归零一次），
        //    慢电容的充电目标必须是**峰值包络**，否则它只能收敛到 |sin| 的均值，
        //    持续过载也充不满。hold ≈ 5 ms：跨得过载波周期，释放时又能很快落下。
        if (r > rPeak_)
            rPeak_ = r;
        else
            rPeak_ -= rPeak_ * coef(0.005);

        // ⓪b 输入慢包络（~200 ms）与瞬态检测
        //    真实素材里没有"真静音"，音头之间的间隙只是电平回落；
        //    若只在真静音后武装 attack，连续演奏时 attack 永远不生效
        //    （实测：小军鼓素材 4 档 attack 的 crest 完全不动，差 4 dB）。
        //    改为：输入峰值比慢包络高出 kTransientDb 就认为是新音头 → 武装。
        {
            const double aSlow = coef(kInSlowTauSec);
            inSlowEnv_ += (inPeak_ - inSlowEnv_) * aSlow;
            const double jumpDb = 20.0 * std::log10(std::max(inPeak_, 1e-12) /
                                                   std::max(inSlowEnv_, 1e-12));
            if (jumpDb > kTransientDb && inPeak_ > silThr_) {
                atkArmed_ = true;
                atkT_ = 0.0;
            }
        }

        // ① C_fast：r > vc 充（attack），否则放（releaseFast）
        //    ⚠️ 2026-11-20 修正：这里充电用的是 detTau_（由 attack 旋钮决定）。
        //    旧注释说"把面板 τ 塞进这个电容会让 20:1 的等效比从 16 掉到 1.9"，
        //    但那次实验是把 τ 和 u 平滑**同时**打开测的；本次逐档实测（300 µs / 1 ms /
        //    3 ms 三档，静态 GR 在 −25/−10/−3/0 dBFS 逐点相同）证明：
        //    只把 detTau_ 换成几百 µs，静态曲线不动，而素材峰值差从 −7.4 dB 收到 0。
        //    真正决定静态比的是 control() 的 (R−1)·dBabove 律，不是检波 τ。
        //
        //    2026-11-22：放电 τ 在电荷接近上限时缩短（见 kRelHiDrop 的注释）。
        //    这是"把瞬态冲顶那 4–5 dB 快速泄掉"的那条路 —— 没有它，短过载的释放
        //    永远从天花板起步，τ(burst) 就是平台。
        double relFast = tauRelFast_;
        {
            const double x = std::clamp(vcFast_ / std::max(vcCeil_, 1e-15), 0.0, 1.0);
            const double hi = std::clamp((x - kRelHiX0) / (1.0 - kRelHiX0), 0.0, 1.0);
            relFast = tauRelFast_ * (1.0 - kRelHiDrop * hi);
        }
        const double tauF = (r > vcFast_) ? detTau_ * allDetScale_ : relFast;
        vcFast_ += (r - vcFast_) * coef(tauF);

        // ② C_slow：慢充 / 慢放。b 秒持续过载后充到 (1−e^{−b/T})，
        //    释放时由它拖住 —— 这就是节目相关性。
        const double tauS = (rPeak_ > vcSlow_) ? chargeTauSec(releaseNorm_) : tauRelSlow_;
        vcSlow_ += (rPeak_ - vcSlow_) * coef(tauS);

        // ③ 取大：谁拖后腿谁决定释放
        //    anti-windup：两个电容都不允许超过饱和电平，否则深度过载会积压过量电荷，
        //    释放时先在轨顶停留好几秒（鼓素材实测 48% 帧卡在 38.2 dB）
        vcFast_ = std::min(vcFast_, vcCeil_);
        vcSlow_ = std::min(vcSlow_, vcCeil_);
        vc_ = std::max(vcFast_, vcSlow_);
        tauRelEff_ = (r > vc_) ? tauAtk_
                     : (vcSlow_ >= vcFast_ ? tauRelSlow_ : tauRelFast_);

        // ④ attack 武装（见 control() 的长注释）：
        //    只有"上一个音确实释放掉了"（静音期间 u 掉了 > kAtkArmDrop）才武装。
        //    这样连着响的稳态信号（静态曲线/释放标定的测量条件）永远不会被武装，
        //    循环与 HEAD 逐位一致；而 350 ms 空隙之后的那个音头会被武装。
        if (inPeak_ < silThr_) {
            if (!inSilence_) { inSilence_ = true; uSilenceStart_ = uPrev_; }
        } else {
            if (inSilence_) {
                inSilence_ = false;
                atkArmed_ = (uSilenceStart_ - uPrev_) > kAtkArmDrop;
                atkT_ = 0.0;
            } else {
                atkT_ += dt;
            }
        }

        // ── NaN/Inf 兜底（出口）─────────────────────────────────────────
        // 上面若因极端数值算出了 Inf/NaN（例如整流值爆掉），这里同样复位，
        // 绝不让非有限状态活过这一样本。
        if (! std::isfinite(vc_)) {
            reset();
            return 0.0;
        }
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
     *
     * ⚠️ 这里有一个**环路增益陷阱**，2026-11-16 定位：
     *   bias 只有 −46.8 dBFS（0.0046），而侧链在音头里看到的是 0.4 量级，
     *   (R−1)=15.5 倍放大后需求是 300+ dB，而 FET 的总行程只有 38 dB ——
     *   于是 uTarget **恒等于 1**（轨顶）。而 release（0.6–1.2 s）在 350 ms 的
     *   空隙里只让 vc 掉 4.8 dB，需求仍在轨顶。结果：**下一个音头到达时 GR 已经
     *   贴在 FET 地板（38.19 dB），attack 的 τ 无论调成多少都没有行程可走**。
     *
     *   修法（不改静态曲线，见 tests/dsp_test.cpp §7 的证据）：
     *     · 出静音后的上升沿：一阶趋向 uTarget，τ = τ_atk（旋钮真的在这里起作用）
     *     · 稳态 / 下降沿：跟随 uTarget（与 HEAD 逐位一致，静态与 release 标定不变）
     *     · **真静音**（inPeak_ → 0，按**压缩前**电平判，不是载波过零）：
     *       让 u 按 τ_gr 回落到 0。这一支对应"电容自己通过 R56 放电"，
     *       在 dB 域是 1:1 的；而上面的需求式是**小信号伺服线性化**，
     *       无信号时它会把释放放大 15.5 倍（一个空隙就把 GR 放到底）。
     *
     * ⚠️ 第二条陷阱（2026-11-16 实测）：慢 attack **只在"释放 → 再触发"**
     *   这个过渡上生效，不能用在整个稳态。原因：uTarget 在轨顶时环路是
     *   bang-bang，静态比正是靠这个抖动的占空比堆出来的 —— 把 τ_atk 全局改慢
     *   会直接改掉静态曲线（实测 20:1 的等效比从 16 掉到 1.9）。
     *   而"稳态里电平涨一点"本来也不可能有 attack：GR 已经贴在轨顶，
     *   没有行程。真正存在 attack 行程的只有"放完再打回来"的这一刻，
     *   所以慢 attack 只在**上一次确实释放过**之后才武装（atkArmed_），
     *   窗口长度 kAtkWindowSec（atkT_ 在 process() 里维护）。
     */
    double control(double vc) {
        // NaN/Inf 兜底：非有限控制量不许往下游走（uPrev_ 是非有限值时同理）。
        // v0.1.0 的问题：`vc <= 1e-12` 对 NaN 恒为 false，log10 继续传播 NaN，
        // 之后 uPrev_ 永远 NaN → 输出静音。
        if (! std::isfinite(vc)) {
            vc_ = vcFast_ = vcSlow_ = uPrev_ = 0.0;
            return 0.0;
        }
        if (vc <= 1e-12) return 0.0;
        const double dBabove = 20.0 * std::log10(1.0 + vc / std::max(bias_, 1e-12));
        const double grTarget = (ratioTarget_ - 1.0) * dBabove;
        const double uTarget = std::clamp(grTarget / kFetSpanDb, 0.0, 1.0);
        const double dt = 1.0 / std::max(sr_, 1.0);

        if (inPeak_ < silThr_) {
            // 真静音：GR 按探测器电容的自然放电回落
            uPrev_ += (0.0 - uPrev_) * (1.0 - std::exp(-dt / std::max(tauGrRel_, 1e-9)));
        } else if (atkArmed_ && atkT_ < kAtkWindowSec && uTarget > uPrev_) {
            // 出静音后的头一段（attack 窗口）：慢 attack 只在这里
            uPrev_ += (uTarget - uPrev_) * (1.0 - std::exp(-dt / std::max(tauAtk_, 1e-9)));
        } else {
            uPrev_ = uTarget;   // 稳态：与 HEAD 完全一致（无平滑），静态/释放标定不变
        }

        if (! std::isfinite(uPrev_)) {   // 兜底：状态算坏了就整组归零，别传给 FET
            vc_ = vcFast_ = vcSlow_ = uPrev_ = 0.0;
            return 0.0;
        }
        return uPrev_;
    }

    // ---- 观测 ----
    double tauAttack() const { return tauAtk_; }
    double tauFastMs() const { return tauRelFast_ * 1000.0; }
    double tauSlowMs() const { return tauRelSlow_ * 1000.0; }
    double tauRelease() const { return tauRel_; }
    /** 释放段当前实际用的 τ（s）；attack 期间为 tauAtk_（调试用） */
    double tauReleaseEff() const { return tauRelEff_; }
    /** 真静音时 GR 状态的回落 τ（s） */
    double tauGrRelease() const { return tauGrRel_; }
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
        silThr_ = kSilenceRel * bias_;
        // anti-windup 上限：u 达到 1（FET 满行程）所需的 vc
        //   没有这个限制时，深度过载会把电容灌到远高于饱和点，释放要好几秒才吐出来
        //   （实测 drums 素材 48% 的帧卡在 38.2 dB 天花板，UAD 只有 0.1%）
        const double dbAboveCeil = kFetSpanDb / std::max(ratioTarget_ - 1.0, 0.1);
        vcCeil_ = bias_ * (std::pow(10.0, dbAboveCeil / 20.0) - 1.0);
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
    double inPeak_ = 0.0;
    double inSlowEnv_ = 0.0;
    double vcCeil_ = 1e9;      // anti-windup 上限   // 输入电平的慢包络（~200 ms），用来做瞬态检测        // 压缩**之前**的峰值包络（判静音用）
    double tauRelFast_ = 1.6;
    double tauRelSlow_ = 1.735;
    double releaseNorm_ = 0.666;
    double attackNorm_ = 0.5;
    double tauAtk_ = kAttackMinSec;
    double detTau_ = kDetTau[1];   // 探测器充电 τ（由 attack 旋钮决定）
    double allDetScale_ = 1.0;     // ALL 档的探测器 τ 倍率
    double uPrev_ = 0.0;
    double tauRel_ = kReleaseMaxSec;
    double tauRelEff_ = kReleaseMaxSec;
    double tauGrRel_ = 0.6;      // 真静音时 GR 的回落 τ（s）
    double atkT_ = 1e9;          // 距"出静音"过去多久（s）；>= 窗口即不武装
    bool   inSilence_ = true;    // 当前是否处于真静音
    bool   atkArmed_ = false;    // 本次"出静音"是否武装慢 attack
    double uSilenceStart_ = 0.0; // 进入静音那一刻的 u（判"释放真的发生过"）
    int ratio_ = 0;
    double thrDb_ = kThresholdDb[0];
    double ratioTarget_ = 4.0;
    double attenDb_ = 0.0;
    double feed_ = 1.0;
    double bias_ = 0.1;
    double silThr_ = 0.01;       // 真静音门限（= kSilenceRel · bias_）
};

} // namespace toire76
