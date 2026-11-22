#pragma once

#include <JuceHeader.h>

class TOIRE76AudioProcessor;

#if TOIRE76_HAS_WEBVIEW

//==============================================================================
/** WebBrowserComponent 默认不通知"页面加载完成"，这里暴露出来。

    页面没加载完就 emit 事件会被静默丢掉（JS 还没注册监听），
    所以一定要等 pageFinishedLoading 之后。
*/
class Torei76WebView final : public juce::WebBrowserComponent
{
public:
    using WebBrowserComponent::WebBrowserComponent;

    std::function<void (const juce::String&)> onPageLoaded;

    void pageFinishedLoading (const juce::String& url) override
    {
        WebBrowserComponent::pageFinishedLoading (url);

        if (onPageLoaded != nullptr)
            onPageLoaded (url);
    }
};

//==============================================================================
/**
 * TOiRE-76 的 WebView2 编辑器。

 * 数据流：
 *   APVTS ←→ WebSliderRelay/WebSliderParameterAttachment ←→ JS 旋钮（双向、可自动化）
 *   APVTS ←→ WebToggleButtonRelay/...Attachment          ←→ JS POWER/BYPASS 开关
 *   C++ → JS 只用 emitEventIfBrowserIsVisible（evaluateJavascript 在 DAW 里静默失败）

 * 踩过的坑（别改）：
 *   · setBounds() 必须在 addAndMakeVisible() 之前，否则 WebView2 永远黑屏
 *   · 首次可见（visibilityChanged）才创建 WebView，避免 DAW 首载崩溃
 *   · WebView2 的 userData 目录放 %APPDATA%，不要放 %TEMP%
 *   · pageFinishedLoading 之前一律不 emit
 *   · C++→JS 只用 emitEventIfBrowserIsVisible
 */
class TOIRE76AudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit TOIRE76AudioProcessorEditor (TOIRE76AudioProcessor&);
    ~TOIRE76AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    void timerCallback() override;

private:
    void ensureWebView();
    void emitPresetList();
    static juce::File presetDir();
    void savePresetAs (const juce::String& suggestedName);

    /** 读一个参数的**去归一化**值（input/output/attack/release 是 0..1，
        ratio/meter 是档位 index）。只在消息线程同步调用 —— 不要放进异步回调。 */
    double readDenorm (const char* paramID) const;

    TOIRE76AudioProcessor& processor;

    // ── 参数双向绑定（名字必须和 WebUI/app.js 的 getSliderState 一致）──
    // 连续参数
    juce::WebSliderRelay inputRelay   { "input" };
    juce::WebSliderRelay outputRelay  { "output" };
    juce::WebSliderRelay attackRelay  { "attack" };
    juce::WebSliderRelay releaseRelay { "release" };
    // Choice 参数（0..N-1）也走 slider relay，前端按归一化值取整
    juce::WebSliderRelay ratioRelay   { "ratio" };
    juce::WebSliderRelay meterRelay   { "meter" };
    // Bool 参数
    juce::WebToggleButtonRelay powerRelay  { "power" };
    juce::WebToggleButtonRelay bypassRelay { "bypass" };

    std::unique_ptr<juce::WebSliderParameterAttachment> inputAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> outputAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> attackAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> releaseAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> ratioAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> meterAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> powerAttachment;
    std::unique_ptr<juce::WebToggleButtonParameterAttachment> bypassAttachment;

    // ⚠️ 声明顺序：webView 最后 -> 先被析构 -> relay 还活着，能收到 webViewDestructed
    std::unique_ptr<Torei76WebView> webView;

    bool pageLoaded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TOIRE76AudioProcessorEditor)
};

#else // ! TOIRE76_HAS_WEBVIEW

//==============================================================================
/** WebView2 关掉时的占位编辑器（vs2026 preset 也能编译 / 跑起来）。 */
class TOIRE76AudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit TOIRE76AudioProcessorEditor (TOIRE76AudioProcessor&);
    ~TOIRE76AudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override {}

private:
    TOIRE76AudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TOIRE76AudioProcessorEditor)
};

#endif // TOIRE76_HAS_WEBVIEW
