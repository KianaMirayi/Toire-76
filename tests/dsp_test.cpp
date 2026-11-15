/*
    dsp_test.cpp — 离线验证 Compressor76（纯 C++17，不依赖 JUCE）

    跑这些实验：
      1) 静态曲线：-60..0 dBFS，四档 ratio → 阈值 / 斜率，对比 1965 手册
      2) 各档 ratio 的阈值与斜率（对比 1965 手册）
      3) 静态工作点：GR 应为 0（相对静态的压缩量），插入损耗 ≈ −1 dB
      4) attack / release 包络：阶跃信号，报告内部 τ 与实测 GR 包络
      5) 节目相关性（program-dependent release）：短过载 vs 长过载的 release τ
         5a 侧链**开环**（无反馈、无 control() 钳位）
         5b 整机闭环（20:1，−6 dBFS，过载后接真静音）

    用法：TOIRE76_DspTest <输出目录>
*/

#define _USE_MATH_DEFINES
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

#include "../src/dsp/Compressor76.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using toire76::Compressor76;
using toire76::Sidechain;

static constexpr double SR = 48000.0;

static double dbToLin(double dB) { return std::pow(10.0, dB / 20.0); }
static double linToDb(double x) { return 20.0 * std::log10(std::max(std::abs(x), 1e-12)); }

struct LevelResult {
    double inDb = 0.0, grDb = 0.0;
};

/// 稳态正弦下的压缩量（跳过瞬态）
static double measureGr(Compressor76& c, double inDb, double freq = 1000.0,
                        double dur = 1.2, double skip = 0.3) {
    const int n = int(SR * dur), skipN = int(SR * skip);
    const double amp = dbToLin(inDb);
    double sum = 0.0;
    int cnt = 0;
    for (int i = 0; i < n; ++i) {
        const double x = amp * std::sin(2.0 * M_PI * freq * i / SR);
        c.processSample(x);
        if (i >= skipN) { sum += c.grDb(); ++cnt; }
    }
    return sum / std::max(cnt, 1);
}

static std::vector<LevelResult> staticCurve(Compressor76& c, double fromDb = -60.0, double toDb = 0.0,
                                            double stepDb = 3.0) {
    std::vector<LevelResult> out;
    for (double inDb = fromDb; inDb <= toDb + 1e-9; inDb += stepDb) {
        c.reset();
        out.push_back({inDb, measureGr(c, inDb)});
    }
    return out;
}

/// 由静态曲线算阈值（GR 首次到 1 dB）与斜率（-3 → -18 dB 之间的 ΔGR/Δin）
static void analyseCurve(const std::vector<LevelResult>& pts, double& thrDb, double& slope, double& ratioEff) {
    thrDb = 0.0;
    for (size_t i = 1; i < pts.size(); ++i) {
        if (pts[i].grDb >= 1.0 && pts[i - 1].grDb < 1.0) {
            const double t = (1.0 - pts[i - 1].grDb) / std::max(pts[i].grDb - pts[i - 1].grDb, 1e-9);
            thrDb = pts[i - 1].inDb + t * (pts[i].inDb - pts[i - 1].inDb);
            break;
        }
    }
    // 与测量脚本同窗口：−24 → −6 dB（18 dB 跨度）
    double grMinus24 = 0.0, grMinus6 = 0.0;
    bool has24 = false, has6 = false;
    for (auto& p : pts) {
        if (std::abs(p.inDb + 24.0) < 0.6) { grMinus24 = p.grDb; has24 = true; }
        if (std::abs(p.inDb + 6.0)  < 0.6) { grMinus6  = p.grDb; has6  = true; }
    }
    slope = (has24 && has6) ? (grMinus6 - grMinus24) / 18.0 : 0.0;
    ratioEff = (slope > 0.001 && slope < 0.999) ? 1.0 / (1.0 - slope) : 0.0;
}

int main(int argc, char** argv) {
    const std::string outDir = (argc > 1) ? argv[1] : ".";
    const char* ratioNames[4] = {"4:1", "8:1", "12:1", "20:1"};
    const double targetRatio[4] = {4.0, 8.0, 12.0, 20.0};
    const double manualThrDb[4] = {-22.0, -21.0, -20.0, -19.0};

    // 构建身份：确认跑的是刚编出来的二进制（我们踩过 stale object 的坑）
    std::printf("build: %s %s | release model: %s\n",
                __DATE__, __TIME__, Sidechain::kReleaseModel);

    FILE* csv = std::fopen((outDir + "/static_curve.csv").c_str(), "w");
    if (csv) std::fprintf(csv, "ratio,input_db,gr_db\n");

    std::printf("=== 1) 静态曲线（当前馈送量为占位值）===\n");
    std::printf("%-6s %10s %8s %10s %10s %8s\n", "ratio", "thr(-1dB)", "slope", "ratio_eff", "manual_thr", "GR@-6");
    double bestFeed[4] = {0, 0, 0, 0};
    double bestSlope[4] = {0, 0, 0, 0};
    double bestThr[4] = {0, 0, 0, 0};
    double bestRatioEff[4] = {0, 0, 0, 0};

    for (int r = 0; r < 4; ++r) {
        Compressor76 c;
        c.setInputDb(0.0);
        c.setOutputDb(-45.0);
        c.setRatioIndex(r);
        c.setAttackNorm(0.5);
        c.setReleaseNorm(0.666);
        c.prepare(SR);

        // 静态压缩比已由 ratioTarget 参数化，不再需要搜索 feed
        const double wantSlope = 1.0 - 1.0 / targetRatio[r];


        auto pts = staticCurve(c, -60.0, 0.0, 3.0);
        for (auto& p : pts) {
            if (csv) std::fprintf(csv, "%s,%.1f,%.3f\n", ratioNames[r], p.inDb, p.grDb);
        }
        double thr, slope, reff;
        analyseCurve(pts, thr, slope, reff);
        bestThr[r] = thr; bestSlope[r] = slope; bestRatioEff[r] = reff;
        double grAt6 = 0.0;
        for (auto& p : pts) if (std::abs(p.inDb + 6.0) < 0.6) grAt6 = p.grDb;
        std::printf("%-6s %10.2f %8.3f %10.2f %10.2f %8.1f   (want=%.2f)\n",
                    ratioNames[r], thr, slope, reff, manualThrDb[r], grAt6, wantSlope);
    }
    if (csv) std::fclose(csv);

    std::printf("\n=== 3) 静态工作点 ===\n");
    {
        Compressor76 c;
        c.setInputDb(0.0);
        c.setOutputDb(-45.0);
        c.setRatioIndex(3);
        c.prepare(SR);
        c.reset();
        std::printf("  静态插入损耗 = %.3f dB（设计 −1.000）\n", c.quiescentGainDb());
        double gr = measureGr(c, -60.0, 1000.0, 1.0, 0.3);
        std::printf("  −60 dBFS 时 GR = %.3f dB（应 ≈ 0）\n", gr);
    }

    std::printf("\n=== 4) attack / release 包络（20:1）===\n");
    {
        Compressor76 c;
        c.setInputDb(0.0);
        c.setOutputDb(-45.0);
        c.setRatioIndex(3);
        c.setAttackNorm(0.5);
        c.setReleaseNorm(0.666);
        c.prepare(SR);
        c.reset();
        std::printf("  内部 τ_attack = %.1f µs，τ_release = %.1f ms\n",
                    c.attackSeconds() * 1e6, c.releaseSeconds() * 1e3);

        FILE* g = std::fopen((outDir + "/envelope.csv").c_str(), "w");
        if (g) std::fprintf(g, "t_ms,gr_db\n");

        // 阶跃：-60 → -6 dBFS
        for (int i = 0; i < int(SR * 0.5); ++i)
            c.processSample(dbToLin(-60.0) * std::sin(2.0 * M_PI * 1000.0 * i / SR));
        double grFinal = 0.0;
        for (int i = 0; i < int(SR * 2.0); ++i) {
            c.processSample(dbToLin(-6.0) * std::sin(2.0 * M_PI * 1000.0 * i / SR));
            grFinal = c.grDb();
            if (g && (i % 8 == 0)) std::fprintf(g, "%.3f,%.3f\n", i * 1000.0 / SR, grFinal);
        }
        std::printf("  稳定 GR = %.2f dB（@ −6 dBFS）\n", grFinal);
        if (g) std::fclose(g);
    }

    // ══════════════════════════════════════════════════════════════════
    //  5) 节目相关性（program-dependent release）
    //
    //  量法：**raw 控制电压 vc_ 的 63% 衰减时间**。
    //    · 不看 control(vc)：那是 (R−1)·20log10(1+vc/bias)/29 再钳到 [0,1]，
    //      深压缩时长期贴着 1.0，会把衰减前段整个吃掉（旧测试正是栽在这里）。
    //    · 释放段用**真静音**（r≡0），vc 就是纯指数衰减，不受载波再触发污染。
    //  5a 侧链开环：直接喂 Sidechain，无反馈环、无钳位 —— 最干净的隔离。
    //  5b 整机闭环：20:1，−6 dBFS 过载，看真实信号链（含反馈）里的表现。
    // ══════════════════════════════════════════════════════════════════
    constexpr double kDspSR = 192000.0;   // 与插件 48 kHz × 4 过采样的内部 FSR 一致
    const double kKnobs[3] = {0.34, 0.67, 1.00};
    const double kBursts[8] = {0.010, 0.020, 0.050, 0.100, 0.200, 0.500, 1.000, 2.000};

    std::printf("\n=== 5a) 侧链开环（no feedback / no clamp）：raw vc 的 63%% 衰减 τ ===\n");
    std::printf("  输入 −8 dBFS 正弦（rectify 后 r≈0.395，避开 control() 的 1.0 钳位）\n");
    std::printf("  %-7s", "burst");
    for (double k : kKnobs) std::printf(" %13s", ("knob " + std::to_string(k).substr(0, 4)).c_str());
    std::printf("   %s\n", "vc(结束)/vSlow(结束) @knob1.0");
    for (double b : kBursts) {
        std::printf("  %5.0fms", b * 1000.0);
        double vcEnd10 = 0.0, vslowEnd10 = 0.0;
        for (int ki = 0; ki < 3; ++ki) {
            Sidechain sc;
            sc.prepare(kDspSR);
            sc.setRatio(3);                     // 20:1
            sc.setAttenuationDb(0.0);
            sc.setAttackRelease(0.67, kKnobs[ki]);

            const int nB = int(b * kDspSR);
            const double amp = dbToLin(-8.0);
            for (int i = 0; i < nB; ++i)
                sc.process(amp * std::sin(2.0 * M_PI * 1000.0 * i / kDspSR));

            const double vc0 = sc.vcRaw();
            const double vslow0 = sc.vcSlowRaw();
            double tau = -1.0;
            const int nR = int(4.0 * kDspSR);
            for (int i = 0; i < nR; ++i) {
                sc.process(0.0);
                if (tau < 0 && sc.vcRaw() <= 0.37 * vc0)
                    tau = double(i) / kDspSR;
            }
            std::printf(" %9.1f ms", tau * 1000.0);
            if (ki == 2) { vcEnd10 = vc0; vslowEnd10 = vslow0; }
        }
        std::printf("   vc=%.4f  vSlow=%.4f\n", vcEnd10, vslowEnd10);
    }

    std::printf("\n=== 5b) 整机闭环（20:1, −6 dBFS, 真静音释放）：raw vc 的 63%% 衰减 τ ===\n");
    std::printf("  %-7s", "burst");
    for (double k : kKnobs) std::printf(" %13s", ("knob " + std::to_string(k).substr(0, 4)).c_str());
    std::printf("   %s\n", "vc/vFast/vSlow(结束) @knob1.0");
    for (double b : kBursts) {
        std::printf("  %5.0fms", b * 1000.0);
        double vcEnd10 = 0.0, vfEnd10 = 0.0, vsEnd10 = 0.0;
        for (int ki = 0; ki < 3; ++ki) {
            Compressor76 c;
            c.setInputDb(0.0);
            c.setOutputDb(0.0);        // 不影响侧链（取样在输出增益之前）
            c.setRatioIndex(3);        // 20:1
            c.setAttackNorm(0.67);
            c.setReleaseNorm(kKnobs[ki]);
            c.prepare(kDspSR);
            c.reset();

            const int nB = int(b * kDspSR);
            for (int i = 0; i < nB; ++i)
                c.processSample(dbToLin(-6.0) * std::sin(2.0 * M_PI * 1000.0 * i / kDspSR));

            const double vc0 = c.sidechain().vcRaw();
            const double vf0 = c.sidechain().vcFastRaw();
            const double vs0 = c.sidechain().vcSlowRaw();
            double tau = -1.0;
            const int nR = int(4.0 * kDspSR);
            for (int i = 0; i < nR; ++i) {
                c.processSample(0.0);
                if (tau < 0 && c.sidechain().vcRaw() <= 0.37 * vc0)
                    tau = double(i) / kDspSR;
            }
            std::printf(" %9.1f ms", tau * 1000.0);
            if (ki == 2) {
                vcEnd10 = vc0;
                vfEnd10 = vf0;
                vsEnd10 = vs0;
            }
        }
        std::printf("   %.4f / %.4f / %.4f\n", vcEnd10, vfEnd10, vsEnd10);
    }

    // ══════════════════════════════════════════════════════════════════
    //  6) 用「插件层同款量法」在模型内部预告 release τ
    //     信号 = 02-release.wav 同构：pilot −40 dBFS 1 s → 三段过载
    //     （0.5 / 0.02 / 2 s，各接 −40 dBFS 载波 8 s）
    //     量法 = campaign_compare.release_taus：10 ms RMS 窗、ref 取导频、
    //     peak=释放段前 3 窗均值、final=窗尾 3 窗均值，
    //     τ = 从释放段起点到 GR 跌到 final + 0.37·(peak−final) 的时间
    //     （这是**外面实际用的尺子**，先在这里对齐，再去重建 VST3 复测）
    // ══════════════════════════════════════════════════════════════════
    {
        std::printf("\n=== 6) 载波释放段（插件同款量法）：模型内 release τ ===\n");
        const double kP = 192000.0;
        const double durs[3] = {0.5, 0.02, 2.0};
        const double carrierSec = 8.0;

        std::vector<double> x;
        double phase = 0.0;
        auto appendSine = [&](double db, double sec, double fadeMs) {
            const int n = int(sec * kP);
            const int nf = std::max(1, int(fadeMs * 1e-3 * kP));
            const double amp = dbToLin(db);
            for (int i = 0; i < n; ++i) {
                const double w = std::min(1.0, std::min(i, n - 1 - i) / double(nf));
                x.push_back(amp * w * std::sin(phase));
                phase += 2.0 * M_PI * 1000.0 / kP;
            }
        };
        appendSine(-40.0, 1.0, 0.0);                 // pilot
        for (double d : durs) {
            appendSine(-6.0, d, 2.0);                // 过载
            appendSine(-40.0, carrierSec, 0.0);      // 载波释放段
        }

        const int W = int(0.01 * kP);                // 10 ms 窗
        const int m = int(x.size()) / W;
        std::vector<double> t(m), gr(m);
        for (int k = 0; k < m; ++k) t[k] = k * 0.01;

        for (int ki = 0; ki < 3; ++ki) {
            Compressor76 c;
            c.setInputDb(0.0);
            c.setRatioIndex(3);                      // 20:1
            c.setAttackNorm(0.67);
            c.setReleaseNorm(kKnobs[ki]);
            c.prepare(kP);
            c.reset();

            const double grDie = 0.0;   // grDb() 已扣除静态插入损耗，这里 0 参考
            std::vector<double> sum(m, 0.0);
            for (size_t i = 0; i < x.size(); ++i) {
                c.processSample(x[i]);
                sum[(int)(i / W)] += c.grDb();
            }
            for (int k = 0; k < m; ++k) gr[k] = sum[k] / W - grDie;

            // ref：导频段（0.1–0.9 s）中位数（这里直接用均值即可，导频稳态）
            double ref = 0.0; int nref = 0;
            for (int k = 0; k < m; ++k)
                if (t[k] >= 0.1 && t[k] <= 0.9) { ref += gr[k]; ++nref; }
            ref /= std::max(nref, 1);

            auto measureTau = [&](double t0) {
                int i0 = -1;
                for (int k = 0; k < m; ++k) if (t[k] >= t0) { i0 = k; break; }
                if (i0 < 0 || i0 + 8 > m) return -1.0;
                std::vector<double> seg;
                int i1 = std::min(m - 1, i0 + 300);         // 3 s 窗
                for (int k = i0; k <= i1; ++k) seg.push_back(gr[k] - ref);
                if (seg.size() < 8) return -1.0;
                const double peak = (seg[0] + seg[1] + seg[2]) / 3.0;
                const double final = (seg[seg.size() - 1] + seg[seg.size() - 2]
                                      + seg[seg.size() - 3]) / 3.0;
                if (peak - final < 2.0) return -1.0;
                const double target = final + 0.37 * (peak - final);
                for (size_t i = 1; i < seg.size(); ++i)
                    if (seg[i] <= target) {
                        const double frac = (seg[i - 1] - target) / (seg[i - 1] - seg[i] + 1e-12);
                        return (double(i - 1) + frac) * 10.0;   // ms
                    }
                return -1.0;
            };

            double tc = 1.0;
            std::printf("  knob %.2f :", kKnobs[ki]);
            for (double d : durs) {
                tc += d;
                const double tau = measureTau(tc);
                std::printf("  %5.0fms→%7.1f ms", d * 1000.0, tau);
                tc += carrierSec;
            }
            std::printf("\n");
        }
    }

    // ══════════════════════════════════════════════════════════════════
    //  7) attack 过冲量（192 kHz / 10 kHz 硬切音头）—— 与参考插件同尺子
    //
    //  信号 = 11-attack192k.wav 同构：0.35 s 静音 + 6 ×（0.1 s 10 kHz
    //         −6 dBFS + 0.35 s 静音）。插件在 192k 走 factorLog2=0（不升频），
    //         所以这里直接按 192k 准备模型。
    //  量法 = 外部脚本同款：
    //           env  = |x| 的 5 点滑动平均
    //           音头 t0：peak   = max  env[t0, t0+15ms]
    //                    steady = median env[t0+50ms, t0+90ms]
    //                    overshoot = 20·log10(peak/steady)
    //  注意：这个尺子对**线性**信号本身就有 +3.0 dB 底数（10 kHz@192k 时
    //        5 点 MA 的 ripple），所以 "ours=3.0" = "和干信号一模一样"，
    //        也就是**过冲完全没被压掉**。
    //  参考（UADx，同一把尺子）：knob 0→19.3 / .34→18.7 / .67→16.9 / 1.0→8.8 dB
    // ══════════════════════════════════════════════════════════════════
    {
        std::printf("\n=== 7) attack 过冲量（192k / 10 kHz 音头，UAD 同尺子）===\n");
        std::printf("  attack model: %s\n", Sidechain::kAttackModel);

        const double kA = 192000.0;
        const double dPre = 0.35, dBurst = 0.10, dGap = 0.35;
        const int nPre = int(dPre * kA), nB = int(dBurst * kA), nG = int(dGap * kA);
        const int nTot = nPre + 6 * (nB + nG);

        std::vector<double> sig(size_t(nTot), 0.0);
        const double amp = dbToLin(-6.0);
        for (int k = 0; k < 6; ++k) {
            const int i0 = nPre + k * (nB + nG);
            for (int i = 0; i < nB; ++i)
                sig[size_t(i0 + i)] = amp * std::sin(2.0 * M_PI * 10000.0 * i / kA);
        }

        auto env5 = [](const std::vector<double>& x) {
            std::vector<double> e(x.size(), 0.0);
            for (size_t i = 0; i < x.size(); ++i) {
                double s = 0.0; int c = 0;
                for (int d = -2; d <= 2; ++d) {
                    const long j = long(i) + d;
                    if (j >= 0 && j < long(x.size())) { s += std::abs(x[size_t(j)]); ++c; }
                }
                e[i] = s / std::max(c, 1);
            }
            return e;
        };
        auto median = [](std::vector<double> v) {
            if (v.empty()) return 0.0;
            std::sort(v.begin(), v.end());
            const size_t n = v.size();
            return (n % 2) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
        };

        const std::vector<double> eIn = env5(sig);
        const double knobs[4] = {0.00, 0.34, 0.67, 1.00};
        const double uadOs[4] = {19.32, 18.74, 16.92, 8.76};

        std::printf("  %-5s %9s %9s %9s %9s | %-7s | %s\n",
                    "knob", "tau_atk", "peak_dB", "sted_dB", "oversht", "UAD", "per-onset (dB)");
        for (int ki = 0; ki < 4; ++ki) {
            Compressor76 c;
            c.setInputDb(0.0);
            c.setOutputDb(-12.90);          // = PluginProcessor::outputNormToDb(0.6259)
            c.setRatioIndex(3);             // 20:1（--param 4=0.75）
            c.setReleaseNorm(0.666);
            c.setAttackNorm(knobs[ki]);
            c.prepare(kA);
            c.reset();

            std::vector<double> out(size_t(nTot), 0.0);
            for (int i = 0; i < nTot; ++i) {
                double l = sig[size_t(i)], r = l;
                c.processStereo(l, r);
                out[size_t(i)] = l;
            }
            const std::vector<double> e = env5(out);

            std::vector<double> os;
            double pkMed = 0.0, stMed = 0.0;
            for (int k = 0; k < 6; ++k) {
                const int i0 = nPre + k * (nB + nG);
                double pk = 0.0;
                for (int i = i0; i < i0 + int(0.015 * kA); ++i)
                    pk = std::max(pk, e[size_t(i)]);
                std::vector<double> st;
                for (int i = i0 + int(0.050 * kA); i < i0 + int(0.090 * kA); ++i)
                    st.push_back(e[size_t(i)]);
                const double sm = median(st);
                os.push_back(linToDb(pk / std::max(sm, 1e-12)));
                if (k == 1) { pkMed = linToDb(pk); stMed = linToDb(sm); }
            }
            std::printf("  %-5.2f %8.0fus %9.2f %9.2f %9.2f | %7.2f |",
                        knobs[ki], c.attackSeconds() * 1e6, pkMed, stMed,
                        median(os), uadOs[ki]);
            for (double v : os) std::printf(" %+5.2f", v);
            std::printf("\n");
        }
        std::printf("  注：第 1 个音头（第一个数字）不算 —— reset 后 u 本来就是 0，"
                    "\"上一次释放\"没有发生过 → 不武装慢 attack。\n"
                    "      参考插件第 1 个音头也特殊（23.41 vs 中位数 19.22），"
                    "所以两边都比**中位数**。\n");

        // τ 扫描：一次跑出 (τ_atk → 过冲量) 的标定曲线，用来反解映射表
        std::printf("\n  τ 扫描（knob 网格 0..1，每格换算成当前映射的 τ）:\n");
        std::printf("  %6s %10s %9s\n", "knob", "tau", "oversht");
        for (int k = 0; k <= 10; ++k) {
            const double kn = k / 10.0;
            Compressor76 c;
            c.setInputDb(0.0);
            c.setOutputDb(-12.90);
            c.setRatioIndex(3);
            c.setReleaseNorm(0.666);
            c.setAttackNorm(kn);
            c.prepare(kA);
            c.reset();
            std::vector<double> out(size_t(nTot), 0.0);
            for (int i = 0; i < nTot; ++i) {
                double l = sig[size_t(i)], r = l;
                c.processStereo(l, r);
                out[size_t(i)] = l;
            }
            const std::vector<double> e = env5(out);
            std::vector<double> os;
            for (int q = 0; q < 6; ++q) {
                const int i0 = nPre + q * (nB + nG);
                double pk = 0.0;
                for (int i = i0; i < i0 + int(0.015 * kA); ++i)
                    pk = std::max(pk, e[size_t(i)]);
                std::vector<double> st;
                for (int i = i0 + int(0.050 * kA); i < i0 + int(0.090 * kA); ++i)
                    st.push_back(e[size_t(i)]);
                os.push_back(linToDb(pk / std::max(median(st), 1e-12)));
            }
            std::printf("  %6.2f %8.0fus %9.2f\n", kn, c.attackSeconds() * 1e6, median(os));
        }

        // 音头之后的 GR 轨迹：直接看 attack 到底有没有进入 GR 路径
        std::printf("\n  GR 轨迹（第 2 个音头，dB）:\n");
        std::printf("  %-5s %8s %8s %8s %8s %8s %8s %8s %8s %8s %8s %8s\n",
                    "knob", "0", "20us", "50us", "100us", "200us", "500us",
                    "1ms", "2ms", "3ms", "5ms", "10ms");
        const double tms[11] = {0, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 3, 5, 10};
        for (int ki = 0; ki < 4; ++ki) {
            Compressor76 c;
            c.setInputDb(0.0);
            c.setOutputDb(-12.90);
            c.setRatioIndex(3);
            c.setReleaseNorm(0.666);
            c.setAttackNorm(knobs[ki]);
            c.prepare(kA);
            c.reset();

            const int i0 = nPre + 1 * (nB + nG);
            std::vector<double> at(size_t(11), 0.0);
            int j = 0;
            for (int i = 0; i < nTot; ++i) {
                double l = sig[size_t(i)], r = l;
                c.processStereo(l, r);
                const double tAfterMs = (i - i0 + 1) / kA * 1000.0;
                if (i >= i0 && j < 11 && tAfterMs >= tms[j]) {
                    at[size_t(j)] = c.grDb();
                    ++j;
                }
            }
            std::printf("  %-5.2f", knobs[ki]);
            for (int q = 0; q < 11; ++q) std::printf(" %8.2f", at[size_t(q)]);
            std::printf("\n");
        }

        // 静音期（第 1 个 burst 结束后的 350 ms 空隙）：release 到底把控制量松开了吗？
        std::printf("\n  空隙里的状态（knob 0.00 / 释放 0.666，t=burst1 结束后）:\n");
        std::printf("  %8s %10s %10s %10s %10s %8s\n",
                    "t(ms)", "vc", "vcFast", "vcSlow", "u", "GR(dB)");
        {
            Compressor76 c;
            c.setInputDb(0.0);
            c.setOutputDb(-12.90);
            c.setRatioIndex(3);
            c.setReleaseNorm(0.666);
            c.setAttackNorm(0.00);
            c.prepare(kA);
            c.reset();

            const int iEnd = nPre + nB;          // burst 1 结束
            const double probes[8] = {0, 25, 50, 100, 200, 300, 345, 350};
            int p = 0;
            for (int i = 0; i < nTot; ++i) {
                double l = sig[size_t(i)], r = l;
                c.processStereo(l, r);
                if (i >= iEnd && p < 8 && (i - iEnd) / kA * 1000.0 >= probes[p]) {
                    const auto& s = c.sidechain();
                    std::printf("  %8.0f %10.5f %10.5f %10.5f %10.3f %8.2f\n",
                                probes[p], s.vcRaw(), s.vcFastRaw(), s.vcSlowRaw(),
                                c.controlVoltage(), c.grDb());
                    ++p;
                }
            }
        }
    }

    std::printf("\n结果已写入 %s\n", outDir.c_str());
    return 0;
}
