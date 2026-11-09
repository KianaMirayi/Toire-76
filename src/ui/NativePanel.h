#pragma once

#include <JuceHeader.h>

#include <array>

/**
 * TOiRE-76 原生 JUCE 渲染面板（POC，用于和 WebView2 版对比）。

 * 这个类和现有产品代码完全无关：不吃 APVTS、不做 DSP，只负责
 *   1. 把同一批 WebUI/assets 贴图画出来（panel_base / knob film strip / 按键 / 指针 / 玻璃）
 *   2. 把原来 WebUI 用 SVG 画的刻度、数字、铭牌改用 Graphics 画
 *   3. 交互：4 个旋钮拖拽/滚轮、RATIO/METER 单选、指针弹簧动画
 *
 * 画布 = 950x175 逻辑像素（和 components.json 的参考画布一致，@2x 素材按 0.5 缩放）。
 * 布局常量全部抄自 renders/components.json，和 WebUI/app.js 用同一份数据，
 * 只是从 JSON 变成了 C++ 常量。
 *
 * 图层顺序照抄 WebUI 的 CSS 堆叠（这点和直觉不一样，别改）：
 *   panel_base(z0) → VU 指针 → meter_glass → 旋钮/按键(z40/41) → overlay(z10：刻度/数字/铭牌/大旋钮指针)
 *   即：overlay 的 z-index=10 会盖在 z-auto 的旋钮和指针上面。
 */
class NativePanel final : public juce::Component,
                          private juce::Timer
{
public:
    NativePanel();
    ~NativePanel() override = default;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr float canvasW = 950.0f;
    static constexpr float canvasH = 175.0f;

private:
    //==============================================================================
    void loadAssets();
    void buildCachesIfNeeded();

    void paintPanel (juce::Graphics&);
    void paintOverlay (juce::Graphics&);        // 刻度 / 数字 / 铭牌的集合（SVG 层的等价物）
    void paintKnobScales (juce::Graphics&);
    void paintVuScale (juce::Graphics&);
    void paintCaptions (juce::Graphics&);

    void paintNeedle (juce::Graphics&);
    void paintKnobs (juce::Graphics&);
    void paintButtons (juce::Graphics&);
    void paintBigKnobPointers (juce::Graphics&);
    void paintToggles (juce::Graphics&);

    void timerCallback() override;
    void applyMeterMode (bool force);
    float needleTargetForState() const;
    float vuThetaForDb (float db) const;

    int  hitTestKnob (juce::Point<float>) const;
    float getUiScale() const;
    static juce::File findAssetsDir();

    //==============================================================================
    juce::File assetsDir;

    juce::Image panelImage;     // 1900x350
    juce::Image glassImage;     // 1900x350
    juce::Image stripBig;       // 192x(192*61)
    juce::Image stripSmall;     // 120x(120*61)
    juce::Image dotImage;       // 80x80
    juce::Image bladeImage;     // 360x360
    juce::Image shadowImage;    // 360x360
    juce::Image ratioUp[4], ratioDown[4];
    juce::Image meterUp[4], meterDown[4];

    // 离屏缓存：面板 / overlay / 玻璃按当前 HiDPI 比例栅格化一次
    juce::Image panelCache, overlayCache, glassCache;
    float cacheScale = 0.0f;

    // 假状态（没有 DSP）：默认值 = 插件契约默认值
    float knobValues[4] { 0.2f, 0.6259f, 0.5f, 0.666f };
    int   ratioValue = 0;    // 0=4:1 1=8:1 2=12:1 3=20:1 4=ALL
    int   meterMode  = 0;    // 0=GR 1=+8 2=+4 3=OFF
    bool  powerOn    = true;
    bool  bypassed   = false;

    std::array<juce::Rectangle<float>, 2> toggleRects {};

    int   draggingKnob = -1;
    float dragLastY = 0.0f;

    struct
    {
        float cur = 0.0f;
        float vel = 0.0f;
        float target = 0.0f;
        float k = 700.0f;
        float c = 38.0f;
        int   mode = -1;
        bool  animating = false;
    } needle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NativePanel)
};
