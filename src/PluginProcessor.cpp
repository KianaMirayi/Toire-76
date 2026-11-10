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
    inputParam   = apvts.getRawParameterValue ("input");
    outputParam  = apvts.getRawParameterValue ("output");
    attackParam  = apvts.getRawParameterValue ("attack");
    releaseParam = apvts.getRawParameterValue ("release");
    ratioParam   = apvts.getRawParameterValue ("ratio");
    powerParam   = apvts.getRawParameterValue ("power");
    bypassParam  = apvts.getRawParameterValue ("bypass");
    // 默认延迟：48 kHz 下 86 samples = 1.792 ms。
    // prepareToPlay 会按实际过采样重新计算；这里先给宿主一个正确值。
    setLatencySamples (86);
}

TOIRE76AudioProcessor::~TOIRE76AudioProcessor() = default;

//==============================================================================
// 面板归一化 → 物理量（taper 实测自 UADx 1176 的 --sweep 回读）
//
// 实测表（Input 与 Output 完全一致）：
//   norm 0.0909 → −51.17 dB    norm 0.5455 → −21.82 dB
//   norm 0.1818 → −42.34 dB    norm 0.6364 → −17.46 dB
//   norm 0.2727 → −34.91 dB    norm 1.0000 →   0.00 dB
// 0.2727 以上严格线性：4.36 dB / 0.0909 ≈ 48 dB/unit；0 处为 −Inf（静音）。
static double knobNormToDb (float norm)
{
    const double n = double (juce::jlimit (0.0f, 1.0f, norm));
    static const double xs[] = { 0.0, 0.0909, 0.1818, 0.2727 };
    static const double ys[] = { -80.0, -51.17, -42.34, -34.91 };
    if (n <= xs[0]) return ys[0];
    for (int i = 1; i < 4; ++i)
        if (n <= xs[i])
            return ys[i-1] + (ys[i]-ys[i-1]) * (n-xs[i-1]) / (xs[i]-xs[i-1]);
    return 48.0 * (n - 1.0);          // 线性段：norm 1 → 0 dB
}

double TOIRE76AudioProcessor::inputNormToDb (float norm)  { return knobNormToDb (norm); }
double TOIRE76AudioProcessor::outputNormToDb (float norm) { return knobNormToDb (norm); }

void TOIRE76AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    compressor.prepare (sampleRate);
    compressor.reset();

    // 过采样倍数：让内部 FSR 落在 ~192 kHz（与参考实现同量级）
    //   48k  → 4×（内部 192k）
    //   96k  → 2×（内部 192k）
    //   192k 及以上 → 不升频
    int factorLog2 = 0;
    if (sampleRate <= 60000.0)      factorLog2 = 2;   // 4×
    else if (sampleRate <= 120000.0) factorLog2 = 1;   // 2×
    else                             factorLog2 = 0;   // 1×

    oversampler.reset();
    if (factorLog2 > 0)
    {
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) juce::jmax (1, getTotalNumOutputChannels()),
            (size_t) factorLog2,
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
            true,          // isMaxQuality
            true);         // useIntegerLatency
        oversampler->initProcessing ((size_t) juce::jmax (1, samplesPerBlock));
    }

    const int osLatency = oversampler != nullptr
                            ? (int) std::lround (oversampler->getLatencyInSamples())
                            : 0;

    // 目标总延迟（48 kHz 下 86 samples，和参考实现对齐）；不足的用纯延迟补齐
    const int targetLatency = (int) std::lround (86.0 * (sampleRate / 48000.0) * 0.0) + 86;
    const int extra = juce::jmax (0, targetLatency - osLatency + 1);

    extraDelay.reset();
    if (extra > 0)
    {
        extraDelay = std::make_unique<juce::dsp::DelayLine<float>> (extra);
        extraDelay->prepare ({ sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock),
                               (juce::uint32) juce::jmax (1, getTotalNumOutputChannels()) });
        extraDelay->setDelay ((float) extra);
    }

    latencySamples = osLatency + extra;
    setLatencySamples (latencySamples);
    updateHostDisplay (juce::AudioProcessor::ChangeDetails().withLatencyChanged (true));
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

    if (latencySamples > 0 && getLatencySamples() != latencySamples)
        setLatencySamples (latencySamples);

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

    // ── 参数 → 物理量 ─────────────────────────────────────────
    compressor.setInputDb   (inputNormToDb   (inputParam   ? inputParam  ->load() : 0.2f));
    compressor.setOutputDb  (outputNormToDb  (outputParam  ? outputParam ->load() : 0.6259f));
    compressor.setAttackNorm (attackParam  ? attackParam  ->load() : 0.5f);
    compressor.setReleaseNorm(releaseParam ? releaseParam ->load() : 0.666f);
    compressor.setRatioIndex (ratioParam ? int (ratioParam->load() + 0.5f) : 0);
    compressor.setPower (true);

    // ── 处理（先升频，在内部 FSR 跑模型，再降频）────────────────
    if (oversampler != nullptr)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        auto upBlock = oversampler->processSamplesUp (block);

        const int upSamples = (int) upBlock.getNumSamples();
        const int upChans   = (int) upBlock.getNumChannels();

        if (upChans >= 2)
        {
            auto* l = upBlock.getChannelPointer (0);
            auto* r = upBlock.getChannelPointer (1);
            for (int i = 0; i < upSamples; ++i)
            {
                double dl = double (l[i]), dr = double (r[i]);
                compressor.processStereo (dl, dr);
                l[i] = float (dl);
                r[i] = float (dr);
            }
        }
        else if (upChans == 1)
        {
            auto* m = upBlock.getChannelPointer (0);
            for (int i = 0; i < upSamples; ++i)
                m[i] = float (compressor.processSample (double (m[i])));
        }

        oversampler->processSamplesDown (block);
    }
    else
    {
        const int numCh = buffer.getNumChannels();
        if (numCh >= 2)
        {
            auto* l = buffer.getWritePointer (0);
            auto* r = buffer.getWritePointer (1);
            for (int i = 0; i < numSamples; ++i)
            {
                double dl = double (l[i]), dr = double (r[i]);
                compressor.processStereo (dl, dr);
                l[i] = float (dl);
                r[i] = float (dr);
            }
        }
        else if (numCh == 1)
        {
            auto* m = buffer.getWritePointer (0);
            for (int i = 0; i < numSamples; ++i)
                m[i] = float (compressor.processSample (double (m[i])));
        }
    }

    // 补齐到目标延迟（纯延迟，不影响音色）
    if (extraDelay != nullptr)
    {
        juce::dsp::AudioBlock<float> blk (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (blk);
        extraDelay->process (ctx);
    }

    // 多余的输出通道（输入比输出少时）必须清掉，否则宿主会读到噪声
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
