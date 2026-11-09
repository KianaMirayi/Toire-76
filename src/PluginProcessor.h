#pragma once

#include <JuceHeader.h>

/**
 * TOiRE-76 —— UAD 1176 风格的 FET 压缩器（当前阶段：只有骨架，DSP 直通）。
 *
 * 这一层只负责三件事：
 *   1. APVTS 参数表（与 web/ + renders/components.json 的插件契约一致）
 *   2. processBlock：power / bypass 检查 + 直通，**没有任何 DSP**
 *   3. createEditor：交给 WebView2 版编辑器
 *
 * 参数 id / 顺序是前端契约的一部分（components.json -> params.order），
 * 改名字要同步改 WebUI/app.js 的 getSliderState / getToggleState 名字。
 */
class TOIRE76AudioProcessor final : public juce::AudioProcessor
{
public:
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
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** 宿主自己的 Bypass 按钮走 bypass 参数（与前端 BYPASS 开关是同一个参数）。 */
    juce::AudioProcessorParameter* getBypassParameter() const override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // 只有 processBlock 会用到的两个开关（其余参数由前端 relay 直接读写）
    std::atomic<float>* powerParam  = nullptr;
    std::atomic<float>* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TOIRE76AudioProcessor)
};

/** ⚠️ 必须是裸函数（默认 __cdecl），不要加 JUCE_CALLTYPE */
juce::AudioProcessor* createPluginFilter();
