/*
    dsp_test.cpp — 离线验证 Compressor76（纯 C++17，不依赖 JUCE）

    跑四组实验：
      1) 静态曲线：-60..0 dBFS，四档 ratio → 阈值 / 斜率，对比 1965 手册
      2) 各档 ratio 的阈值与斜率（对比 1965 手册）
      3) 静态工作点：GR 应为 0（相对静态的压缩量），插入损耗 ≈ −1 dB
      4) attack / release 包络：阶跃信号，报告内部 τ 与实测 GR 包络

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
    double gr3 = 0.0, gr18 = 0.0;
    bool has3 = false, has18 = false;
    for (auto& p : pts) {
        if (std::abs(p.inDb + 3.0) < 0.01) { gr3 = p.grDb; has3 = true; }
        if (std::abs(p.inDb + 18.0) < 0.01) { gr18 = p.grDb; has18 = true; }
    }
    slope = (has3 && has18) ? (gr3 - gr18) / 15.0 : 0.0;
    ratioEff = (slope > 0.001 && slope < 0.999) ? 1.0 / (1.0 - slope) : 0.0;
}

int main(int argc, char** argv) {
    const std::string outDir = (argc > 1) ? argv[1] : ".";
    const char* ratioNames[4] = {"4:1", "8:1", "12:1", "20:1"};
    const double targetRatio[4] = {4.0, 8.0, 12.0, 20.0};
    const double manualThrDb[4] = {-22.0, -21.0, -20.0, -19.0};

    FILE* csv = std::fopen((outDir + "/static_curve.csv").c_str(), "w");
    if (csv) std::fprintf(csv, "ratio,input_db,gr_db\n");

    std::printf("=== 1) 静态曲线（当前馈送量为占位值）===\n");
    std::printf("%-6s %10s %8s %10s %10s\n", "ratio", "thr(-1dB)", "slope", "ratio_eff", "manual_thr");
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
        std::printf("%-6s %10.2f %8.3f %10.2f %10.2f   (want_slope=%.3f)\n",
                    ratioNames[r], thr, slope, reff, manualThrDb[r], wantSlope);
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

    std::printf("\n结果已写入 %s\n", outDir.c_str());
    return 0;
}
