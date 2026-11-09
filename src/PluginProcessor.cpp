#include "PluginProcessor.h"

#include "PluginEditor.h"

//==============================================================================
namespace
{
    // 参数表（顺序 = 插件契约 params.order，别乱）
    //
    //   input    Float  0..1  默认 0.2
    //   output   Float  0..1  默认 0.6259
    //   attack   Float  0..1  默认 0.5
    //   release  Float  0..1  默认 0.666
    //   ratio    Choice 4:1 / 8:1 / 12:1 / 20:1 / ALL  默认 0
    //   meter    Choice GR / +8 / +4 / OFF             默认 0
    //   power    Bool   默认 true
    //   bypass   Bool   默认 false
    auto makeUnitFloat (const char* id, const char* name, float defaultValue)
    {
        // 0..1 连续参数：interval 0 = 不量化（前端旋钮用归一化值）
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 },
            name,
            juce::NormalisableRange<float> { 0.0f, 1.0f },
            defaultValue);
    }
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TOIRE76AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (makeUnitFloat ("input",   "Input",   0.2000f));
    layout.add (makeUnitFloat ("output",  "Output",  0.6259f));
    layout.add (makeUnitFloat ("attack",  "Attack",  0.5000f));
    layout.add (makeUnitFloat ("release", "Release", 0.6660f));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "ratio", 1 }, "Ratio",
        juce::StringArray { "4:1", "8:1", "12:1", "20:1", "ALL" }, 0));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "meter", 1 }, "Meter",
        juce::StringArray { "GR", "+8", "+4", "OFF" }, 0));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "power", 1 }, "Power", true));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return layout;
}

//==============================================================================
TOIRE76AudioProcessor::TOIRE76AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    powerParam  = apvts.getRawParameterValue ("power");
    bypassParam = apvts.getRawParameterValue ("bypass");
}

TOIRE76AudioProcessor::~TOIRE76AudioProcessor() = default;

//==============================================================================
void TOIRE76AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (sampleRate, samplesPerBlock);

    // 这一轮没有 DSP、没有延迟补偿
    setLatencySamples (0);
}

void TOIRE76AudioProcessor::releaseResources() {}

bool TOIRE76AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::stereo()
        && mainOut != juce::AudioChannelSet::mono())
        return false;

    return mainOut == layouts.getMainInputChannelSet();
}

void TOIRE76AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    const auto numSamples = buffer.getNumSamples();

    if (numSamples <= 0)
        return;

    const bool powered  = powerParam  == nullptr || powerParam->load()  > 0.5f;
    const bool bypassed = bypassParam != nullptr && bypassParam->load() > 0.5f;

    if (! powered || bypassed)
    {
        // 关机 / 旁路：静音（直通也不做，保持和真实硬件的"关机无输出"一致）
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.clear (ch, 0, numSamples);

        return;
    }

    // ── 直通 ──────────────────────────────────────────────────
    // 这一轮不实现 DSP。输入缓冲原样保留即可（in-place）。
    // 多余的输出通道（输入比输出少时）必须清掉，否则宿主会读到噪声。
    const int numInputChannels = getTotalNumInputChannels();

    for (int ch = numInputChannels; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
}

//==============================================================================
juce::AudioProcessorParameter* TOIRE76AudioProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

//==============================================================================
juce::AudioProcessorEditor* TOIRE76AudioProcessor::createEditor()
{
    return new TOIRE76AudioProcessorEditor (*this);
}

//==============================================================================
void TOIRE76AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState().createXml())
        copyXmlToBinary (*state, destData);
}

void TOIRE76AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* createPluginFilter()
{
    return new TOIRE76AudioProcessor();
}
