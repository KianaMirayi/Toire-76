#include "PluginEditor.h"

#include "PluginProcessor.h"

#if TOIRE76_HAS_WEBVIEW

namespace
{
    // 与 HTML body / 面板底色一致，加载期间不会闪白
    constexpr juce::uint32 kBackground = 0xff08080a;

    /**
     * 找 WebUI/index.html。

     * ⚠️ 必须用 invokedExecutableFile（不是 currentExecutableFile）：
     *    宿主 / standalone 的实际路径不一定是 exe 自己所在目录。

     * 目录布局（两种都要能命中）：
     *   Standalone/TOiRE-76.exe              + Standalone/webui/index.html
     *   TOiRE-76.vst3/Contents/x86_64-win/…  + TOiRE-76.vst3/Contents/webui/index.html
     */
    juce::File findWebUiIndex()
    {
        const auto exeDir = juce::File::getSpecialLocation (juce::File::invokedExecutableFile)
                                .getParentDirectory();

        auto dir = exeDir;

        for (int i = 0; i < 8; ++i)
        {
            for (auto* sub : { "webui/index.html",
                               "../webui/index.html",
                               "Contents/webui/index.html",
                               "../../webui/index.html" })
            {
                const auto f = dir.getChildFile (sub);

                if (f.existsAsFile())
                    return f;
            }

            dir = dir.getParentDirectory();
        }

        // 开发期回退：源码树里的 WebUI/（install 到别处时这步自然落空）
        const auto srcPath = juce::String (__FILE__);
        auto srcFile = juce::File (srcPath);

        if (! juce::File::isAbsolutePath (srcPath))
            srcFile = juce::File::getCurrentWorkingDirectory().getChildFile (srcPath);

        const auto devFile = srcFile.getParentDirectory().getParentDirectory()
                                 .getChildFile ("WebUI").getChildFile ("index.html");

        if (devFile.existsAsFile())
            return devFile;

        return {};
    }
} // namespace

//==============================================================================
TOIRE76AudioProcessorEditor::TOIRE76AudioProcessorEditor (TOIRE76AudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setOpaque (true);

    // 参考画布 950x175（= renders/components.json canvas），可等比放大到 1900x350
    setResizable (true, true);
    setResizeLimits (950, 205, 1900, 410);
    setSize (950, 205);

    // ── 参数双向绑定：DAW 自动化 ←→ 网页旋钮 ─────────────────
    // 连续参数（0..1）
    auto attachSlider = [this] (juce::WebSliderRelay& relay,
                                std::unique_ptr<juce::WebSliderParameterAttachment>& slot,
                                const char* paramID)
    {
        if (auto* param = processor.apvts.getParameter (paramID))
            slot = std::make_unique<juce::WebSliderParameterAttachment> (*param, relay, nullptr);
    };

    attachSlider (inputRelay,   inputAttachment,   "input");
    attachSlider (outputRelay,  outputAttachment,  "output");
    attachSlider (attackRelay,  attackAttachment,  "attack");
    attachSlider (releaseRelay, releaseAttachment, "release");

    // Choice 参数（AudioParameterChoice 的 range 是 0..N-1，位置就是档位）
    attachSlider (ratioRelay, ratioAttachment, "ratio");
    attachSlider (meterRelay, meterAttachment, "meter");

    // Bool 参数：POWER / BYPASS
    if (auto* powerParam = processor.apvts.getParameter ("power"))
        powerAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (
            *powerParam, powerRelay, nullptr);

    if (auto* bypassParam = processor.apvts.getParameter ("bypass"))
        bypassAttachment = std::make_unique<juce::WebToggleButtonParameterAttachment> (
            *bypassParam, bypassRelay, nullptr);
}

TOIRE76AudioProcessorEditor::~TOIRE76AudioProcessorEditor()
{
    // ⚠️ 必须在成员析构前先把 WebView2 拆掉：
    //    浏览器析构时会回调 relay 的 webViewDestructed，那时 relay 还活着才对。
    stopTimer();

    if (webView != nullptr)
    {
        webView->onPageLoaded = nullptr;
        webView->goToURL ("about:blank");
        webView.reset();
    }

    // attachment 引用 relay（成员），必须先于 relay 析构
    inputAttachment.reset();
    outputAttachment.reset();
    attackAttachment.reset();
    releaseAttachment.reset();
    ratioAttachment.reset();
    meterAttachment.reset();
    powerAttachment.reset();
    bypassAttachment.reset();
}

//==============================================================================
juce::File TOIRE76AudioProcessorEditor::presetDir()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("TOiRE-76").getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

void TOIRE76AudioProcessorEditor::emitPresetList()
{
    if (webView == nullptr) return;

    juce::Array<juce::var> list;
    auto files = presetDir().findChildFiles (juce::File::findFiles, false, "*.t76preset");
    files.sort();

    for (auto& f : files)
    {
        auto obj = juce::JSON::parse (f.loadFileAsString());
        if (auto* dyn = obj.getDynamicObject())
        {
            auto* copy = new juce::DynamicObject (*dyn);
            copy->setProperty ("name", f.getFileNameWithoutExtension());
            list.add (juce::var (copy));
        }
    }

    webView->emitEventIfBrowserIsVisible ("toire76Presets", juce::var (list));
}

void TOIRE76AudioProcessorEditor::savePresetAs (const juce::String& suggestedName)
{
    auto name = suggestedName.trim();
    if (name.isEmpty())
        name = "Preset " + juce::Time::getCurrentTime().formatted ("%H%M%S");

    auto chooser = std::make_shared<juce::FileChooser> ("保存预设",
        presetDir().getChildFile (name + ".t76preset"), "*.t76preset");

    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, chooser] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File()) return;
        if (! file.hasFileExtension ("t76preset")) file = file.withFileExtension ("t76preset");

        auto* obj = new juce::DynamicObject();
        auto readParam = [this] (const char* id) -> double
        {
            if (auto* p = processor.apvts.getParameter (id))
                if (auto* raw = processor.apvts.getRawParameterValue (id))
                    return (double) p->convertFrom0to1 (raw->load());
            return 0.0;
        };
        obj->setProperty ("input",   readParam ("input"));
        obj->setProperty ("output",  readParam ("output"));
        obj->setProperty ("attack",  readParam ("attack"));
        obj->setProperty ("release", readParam ("release"));
        obj->setProperty ("ratio",   readParam ("ratio"));
        obj->setProperty ("meter",   readParam ("meter"));

        file.replaceWithText (juce::JSON::toString (juce::var (obj), false));
        emitPresetList();
    });
}

void TOIRE76AudioProcessorEditor::timerCallback()
{
    if (webView == nullptr || ! pageLoaded)
        return;

    juce::Array<juce::var> msg;
    msg.add (juce::var ((double) processor.grDbForUi.load  (std::memory_order_relaxed)));
    msg.add (juce::var ((double) processor.meterInDb.load  (std::memory_order_relaxed)));
    msg.add (juce::var ((double) processor.meterOutDb.load (std::memory_order_relaxed)));
    webView->emitEventIfBrowserIsVisible ("toire76Meter", msg);
}

void TOIRE76AudioProcessorEditor::visibilityChanged()
{
    // ⚠️ 延迟创建：构造期宿主消息循环还没稳、WebView2 环境也没就绪。
    //    首次可见才建 WebView —— 避免 DAW 首次加载就闪退。
    if (isVisible())
        ensureWebView();

    AudioProcessorEditor::visibilityChanged();
}

void TOIRE76AudioProcessorEditor::ensureWebView()
{
    if (webView != nullptr)
        return;

    auto winOpts = juce::WebBrowserComponent::Options::WinWebView2{};

    // ⚠️ userData 放 %APPDATA%，不要放 %TEMP%（清 Temp 会逼 WebView2 重新冷启动）
    const auto userData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                              .getChildFile ("TOiRE-76")
                              .getChildFile ("WebView2");
    userData.createDirectory();

    winOpts = winOpts.withUserDataFolder (userData)
                     .withStatusBarDisabled()
                     .withBuiltInErrorPageDisabled()
                     .withBackgroundColour (juce::Colour (kBackground));

    const auto opts = juce::WebBrowserComponent::Options{}
                          .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
                          .withWinWebView2Options (winOpts)
                          .withNativeIntegrationEnabled()
                          .withKeepPageLoadedWhenBrowserIsHidden()
                          .withOptionsFrom (inputRelay)
                          .withOptionsFrom (outputRelay)
                          .withOptionsFrom (attackRelay)
                          .withOptionsFrom (releaseRelay)
                          .withOptionsFrom (ratioRelay)
                          .withOptionsFrom (meterRelay)
                          .withOptionsFrom (powerRelay)
                          .withOptionsFrom (bypassRelay)
                          .withEventListener ("t76PresetCmd", [this] (const juce::var& payload)
                          {
                              const juce::String action = payload.getProperty ("action", juce::var()).toString();
                              if (action == "save")
                                  savePresetAs (payload.getProperty ("name", juce::var()).toString());
                              else if (action == "delete")
                              {
                                  auto nm = payload.getProperty ("name", juce::var()).toString();
                                  if (nm.isNotEmpty())
                                      presetDir().getChildFile (nm + ".t76preset").deleteFile();
                                  emitPresetList();
                              }
                              else if (action == "refresh")
                                  emitPresetList();
                          });

    webView = std::make_unique<Torei76WebView> (opts);

    // ── 不抢键盘焦点 ─────────────────────────────────────────
    // JUCE 的 WebView2 拿到焦点会把键盘交给网页，DAW 的空格/快捷键全失效。
    // 这个 UI 没有文本输入，不需要键盘。
    webView->setWantsKeyboardFocus (false);
    webView->setMouseClickGrabsKeyboardFocus (false);

    webView->onPageLoaded = [this] (const juce::String& url)
    {
        // 初始 about:blank 也会回调 —— 别把它当成"页面好了"
        if (url.startsWithIgnoreCase ("about:"))
            return;

        pageLoaded = true;

        // C++ → JS 只走这一条路（evaluateJavascript 在 DAW 里会静默失败）
        juce::Array<juce::var> payload;
        payload.add (juce::var ("ok"));
        payload.add (juce::var ("0.1.0"));
        webView->emitEventIfBrowserIsVisible ("toire76Status", payload);

        // 实时 GR 推送到网页：30 Hz，音频线程只写原子量
        startTimerHz (30);
        emitPresetList();
    };

    // ⚠️⚠️ 顺序是硬性的：
    //   setBounds 必须在 addAndMakeVisible 之前，否则 WebView2 永远黑屏（JUCE 8 的坑）
    webView->setBounds (getLocalBounds());
    webView->setOpaque (true);
    addAndMakeVisible (*webView);
    webView->setVisible (true);
    webView->toFront (true);

    // 加载前端（HUD 默认隐藏；调试时在地址末尾手动加 ?debug=1）
    const auto html = findWebUiIndex();

    if (html.existsAsFile())
    {
        auto url = juce::URL (html).toString (false);
        webView->goToURL (url);
    }
    else
    {
        juce::Logger::writeToLog ("TOiRE-76: WebUI/index.html not found next to "
                                  + juce::File::getSpecialLocation (juce::File::invokedExecutableFile)
                                        .getFullPathName());
        webView->goToURL ("about:blank");
    }
}

void TOIRE76AudioProcessorEditor::resized()
{
    if (webView != nullptr)
        webView->setBounds (getLocalBounds());
}

void TOIRE76AudioProcessorEditor::paint (juce::Graphics& g)
{
    // 与 HTML body 底色一致，避免加载期间闪白
    g.fillAll (juce::Colour (kBackground));
}

#else // ! TOIRE76_HAS_WEBVIEW

//==============================================================================
//  WebView2 关掉时的占位编辑器
//==============================================================================
TOIRE76AudioProcessorEditor::TOIRE76AudioProcessorEditor (TOIRE76AudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setOpaque (true);
    setSize (950, 205);
}

void TOIRE76AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff08080a));
    g.setColour (juce::Colour (0xffe0b050));
    g.setFont (juce::Font (juce::FontOptions (18.0f).withStyle ("Bold")));
    g.drawText ("TOiRE-76  (built with TOIRE76_WEBVIEW2=OFF)",
                getLocalBounds(), juce::Justification::centred);
}

#endif // TOIRE76_HAS_WEBVIEW
