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

//==============================================================================
namespace
{
    struct Preset
    {
        const char* name;
        float input, output, attack, release;
        int   ratio;      // 0=4:1 1=8:1 2=12:1 3=20:1 4=ALL
        int   meter;      // 0=GR 1=+8 2=+4 3=OFF
    };

    // 工厂预设（参照 1176 的常见用法）
    const Preset kPresets[TOIRE76AudioProcessor::kNumPresets] = {
        { "Init",        0.200f, 0.6259f, 0.500f, 0.666f, 0, 0 },
        { "Drum Room",   0.620f, 0.500f,  0.900f, 0.300f, 4, 0 },
        { "Drum Punch",  0.550f, 0.550f,  0.750f, 0.450f, 1, 0 },
        { "Vocal",       0.450f, 0.600f,  0.550f, 0.500f, 1, 0 },
        { "Bass",        0.500f, 0.580f,  0.400f, 0.350f, 2, 0 },
        { "Mix Bus",     0.350f, 0.650f,  0.500f, 0.600f, 0, 0 }
    };
}

void TOIRE76AudioProcessor::setCurrentProgram (int index)
{
    index = juce::jlimit (0, kNumPresets - 1, index);
    currentProgram = index;
    const auto& p = kPresets[index];

    auto setNorm = [this] (const char* id, float v)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (v));
    };

    setNorm ("input",   p.input);
    setNorm ("output",  p.output);
    setNorm ("attack",  p.attack);
    setNorm ("release", p.release);
    setNorm ("ratio",   (float) p.ratio);
    setNorm ("meter",   (float) p.meter);
}

const juce::String TOIRE76AudioProcessor::getProgramName (int index)
{
    index = juce::jlimit (0, kNumPresets - 1, index);
    return kPresets[index].name;
}

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
    meterParam   = apvts.getRawParameterValue ("meter");
    powerParam   = apvts.getRawParameterValue ("power");
    bypassParam  = apvts.getRawParameterValue ("bypass");
    // 默认延迟：48 kHz 下 86 samples = 1.792 ms。
    // prepareToPlay 会按实际过采样重新计算；这里先给宿主一个正确值。
    setLatencySamples (86);
}

TOIRE76AudioProcessor::~TOIRE76AudioProcessor() = default;

//==============================================================================
// ── 面板旋钮 → 相对增益（dB）────────────────────────────────
// 实测：参考插件 21 点扫描（input/output 各一遍），归一化到 v=1.0 → 0 dB。
//   输入旋钮：全程只改变约 6.9 dB，低端最陡（0.00 处 −6.86 dB）
//   输出旋钮：全程约 100 dB（0.00 处 −99.99 dB，近似静音）
static constexpr double kKnobX[21] = {
        0.00, 0.05, 0.10, 0.15, 0.20, 0.25, 0.30, 0.35,
        0.40, 0.45, 0.50, 0.55, 0.60, 0.65, 0.70, 0.75,
        0.80, 0.85, 0.90, 0.95, 1.00
};
// 输入旋钮：**未压缩条件**下实测（−70 dBFS 正弦，knob 0..1 共 11 点，线性插值到 21 点）
//   knob 0.0 → −41.95 dB ... knob 1.0 → 0 dB
//   ⚠️ 之前那版只有 6.9 dB 行程，是在压缩区里测的（增益被 GR 抵消），
//      导致 INPUT 旋钮几乎不起作用、听感上像是方向反了。
static constexpr double kInputRelDb[21] = {
        -41.95, -40.42, -38.89, -36.29, -33.68, -29.81, -25.93, -23.36,
        -20.79, -17.09, -13.39, -11.01,  -8.62,  -7.34,  -6.05,  -3.65,
         -1.25,  -0.66,  -0.07,  -0.04,   0.00
};
static constexpr double kOutputRelDb[21] = {
        -99.99, -93.55, -76.43, -62.35, -55.02, -47.99, -41.53, -35.87,
        -30.78, -26.12, -21.81, -17.85, -14.48, -11.43, -8.60, -6.06,
        -4.01, -2.32, -0.98, -0.12, 0.00
};

static double interpKnob(const double* ys, double x)
{
    x = juce::jlimit (0.0, 1.0, x);
    for (int i = 1; i < 21; ++i)
        if (x <= kKnobX[i])
            return ys[i-1] + (ys[i]-ys[i-1]) * (x - kKnobX[i-1]) / (kKnobX[i] - kKnobX[i-1]);
    return ys[20];
}

double TOIRE76AudioProcessor::inputNormToDb (float norm)
{
    return interpKnob (kInputRelDb, double (norm));
}

double TOIRE76AudioProcessor::outputNormToDb (float norm)
{
    return interpKnob (kOutputRelDb, double (norm));
}

void TOIRE76AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
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

    // ⚠️ 模型跑在内部 FSR 上（过采样 4×/2×），时间常数必须按内部采样率准备，
    //    否则 release/attack 会快 2–4 倍（曾经踩过：release 1613 ms 变成 445 ms）
    const double internalRate = sampleRate * double (1 << factorLog2);
    compressor.prepare (internalRate);
    compressor.reset();

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
    // 输入 RMS（处理前量，供表针使用）
    float inRmsLinear = 0.0f;
    {
        double sum = 0.0; int n = 0;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* d = buffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i) { sum += (double) d[i] * (double) d[i]; ++n; }
        }
        inRmsLinear = n > 0 ? (float) std::sqrt (sum / (double) n) : 0.0f;
    }

    const bool bypassed = bypassParam != nullptr && bypassParam->load() > 0.5f;

    if (! powered || bypassed)
    {
        // 关机 / 旁路：静音（直通也不做，保持和真实硬件的"关机无输出"一致）
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.clear (ch, 0, numSamples);

        grDbForUi.store (0.0f, std::memory_order_relaxed);
        meterInRmsDb = meterOutRmsDb = -100.0f;
        meterInDb.store (-100.0f, std::memory_order_relaxed);
        meterOutDb.store (-100.0f, std::memory_order_relaxed);
        return;
    }

    // ── 参数 → 物理量 ─────────────────────────────────────────
    compressor.setInputDb   (inputNormToDb   (inputParam   ? inputParam  ->load() : 0.2f));
    compressor.setOutputDb  (outputNormToDb  (outputParam  ? outputParam ->load() : 0.6259f));
    compressor.setAttackNorm (attackParam  ? attackParam  ->load() : 0.5f);
    compressor.setReleaseNorm(releaseParam ? releaseParam ->load() : 0.666f);
    compressor.setRatioIndex (ratioParam ? int (ratioParam->load() + 0.5f) : 0);
    compressor.setPower (true);
    // METER = OFF（第 4 档）→ 压缩旁通（与参考插件一致：打到 OFF 不再压缩）
    compressor.setCompressionBypass (meterParam != nullptr && int (meterParam->load() + 0.5f) == 3);

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

    // 实时 GR + 电平（给编辑器的 HUD / 表针用）：音频线程只做原子写
    grDbForUi.store (powered && ! bypassed ? (float) compressor.grDb() : 0.0f,
                     std::memory_order_relaxed);

    // 表针用电平：**RMS**（VU 标准），并做 300 ms 快起慢落，避免逐块跳动
    auto outputRms = [&buffer, numSamples]
    {
        double sum = 0.0; int n = 0;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* d = buffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i) { sum += (double) d[i] * (double) d[i]; ++n; }
        }
        return n > 0 ? (float) std::sqrt (sum / (double) n) : 0.0f;
    };

    auto toDb = [] (float rms) { return juce::Decibels::gainToDecibels (juce::jmax (rms, 1.0e-6f), -100.0f); };

    // 原始 RMS 直送前端：表针的上升/阻尼/释放全部在 JS 里可调（调试面板）
    meterInDb.store  (toDb (inRmsLinear),   std::memory_order_relaxed);
    meterOutDb.store (toDb (outputRms()),   std::memory_order_relaxed);
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
