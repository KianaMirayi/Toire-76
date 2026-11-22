#pragma once

#include <JuceHeader.h>

#include <juce_dsp/juce_dsp.h>

#include "dsp/Compressor76.h"

/**
 * TOiRE-76 —— UAD 1176 风格的 FET 压缩器（L2 灰盒 DSP）。
 *
 * 这一层负责三件事：
 *   1. APVTS 参数表（与 web/ + renders/components.json 的插件契约一致）
 *   2. processBlock：参数 → 物理量 → toire76::Compressor76（立体声联动侧链）
 *   3. createEditor：交给 WebView2 版编辑器
 *
 * 参数 id / 顺序是前端契约的一部分（components.json -> params.order），
 * 改名字要同步改 WebUI/app.js 的 getSliderState / getToggleState 名字。
 */
class TOIRE76AudioProcessor final : public juce::AudioProcessor
{
public:
    /** 供编辑器定时读取的实时增益衰减（dB，正数=压得更多） */
    std::atomic<float> grDbForUi { 0.0f };
    /** 表针用：输入/输出峰值电平（dBFS） */
    std::atomic<float> meterInDb  { -100.0f };
    std::atomic<float> meterOutDb { -100.0f };

    float meterInRmsDb  = -100.0f;   // 表针平滑状态
    float meterOutRmsDb = -100.0f;

    TOIRE76AudioProcessor();
    ~TOIRE76AudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "TOiRE-76"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override { return kNumPresets; }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    static constexpr int kNumPresets = 6;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** 宿主自己的 Bypass 按钮走 bypass 参数（与前端 BYPASS 开关是同一个参数）。 */
    juce::AudioProcessorParameter* getBypassParameter() const override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // processBlock 用到的参数缓存（前端 relay 仍直接读写 APVTS）
    std::atomic<float>* inputParam   = nullptr;
    std::atomic<float>* outputParam  = nullptr;
    std::atomic<float>* attackParam  = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* ratioParam   = nullptr;
    std::atomic<float>* meterParam   = nullptr;

    int currentProgram = 0;

    // 预设切换检测（M2）：本块 vs 上块有 **≥3 个**参数同时变化才复位侧链。
    // 单旋钮拖动只变 1 个 → 不会在音频中途 reset；换预设会一次变 4–5 个 → 复位。
    float lastMeterIn_    = 0.2f;
    float lastMeterOut_   = 0.6259f;
    float lastMeterRel_   = 0.666f;
    float lastMeterAtk_   = 0.5f;
    int   lastMeterRatio_ = -1;
    int   lastMeterMeter_ = -1;
    bool  hasMeterState_  = false;
    std::atomic<float>* powerParam   = nullptr;
    std::atomic<float>* bypassParam  = nullptr;

    toire76::Compressor76 compressor;

    /** 4× 过采样（内部 FSR ~192 kHz）。倍数按宿主采样率选，使延迟时间与参考一致。 */
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> oversampledBuffer;

    /** 把总延迟补齐到目标值（48k=86 / 96k=118 / 192k=151，见 dsp/LatencyPlan.h），
        便于逐样本对齐比较；旁路时走的是同一条延迟线。 */
    std::unique_ptr<juce::dsp::DelayLine<float>> extraDelay;
    int latencySamples = 0;

    /** 面板归一化值 → 物理量（taper 待实测替换，见 docs/DSP-电路映射.md §二 #1/#8） */
    static double inputNormToDb  (float norm);
    static double outputNormToDb (float norm);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TOIRE76AudioProcessor)
};

/** ⚠️ 必须是裸函数（默认 __cdecl），不要加 JUCE_CALLTYPE */
juce::AudioProcessor* createPluginFilter();
