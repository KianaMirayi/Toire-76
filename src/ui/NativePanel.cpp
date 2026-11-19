#include "NativePanel.h"

#include <array>
#include <cmath>
#include <functional>

namespace
{
    //==============================================================================
    //  布局数据（全部来自 renders/components.json，和 WebUI 用的是同一份）
    //==============================================================================
    struct KnobDef
    {
        const char* key;
        bool  isBig;
        float angleMinDeg;     // theta_cw（12 点 = 0，顺时针为正）
        float angleMaxDeg;
        float pivotX, pivotY;  // 参考画布 950x175
    };

    const KnobDef kKnobDefs[] =
    {
        { "input",   true,  159.90f, -161.52f, 136.0f,  87.0f },
        { "output",  true,  159.90f, -161.52f, 323.0f,  87.0f },
        { "attack",  false, 210.80f,  509.70f, 475.0f,  49.0f },
        { "release", false, 210.80f,  509.70f, 475.0f, 125.0f },
    };

    // 大旋钮（INPUT/OUTPUT）指针角度（theta_cw）：−∞ = 198.5° → 0 dB = 519.9°，顺时针。
    // 与 markings.input_scale 的印字一致；值↑ → 指针顺时针 → 朝 0 dB → 更响。
    // ⚠️ 不要用 kKnobDefs 里 INPUT/OUTPUT 的 angleMin/angleMax：那一对是素材的
    //    "值↑ → 角度↓" 反向约定（159.9 = 0 dB、−161.52 = −∞），直接用会把指针转反。
    constexpr float kBigKnobAngleStart = 198.5f;
    constexpr float kBigKnobAngleSweep = 321.4f;

    inline float bigKnobAngle (float v) noexcept
    {
        return kBigKnobAngleStart + juce::jlimit (0.0f, 1.0f, v) * kBigKnobAngleSweep;
    }

    struct ScaleMark
    {
        const char* label;
        float angleCw;
    };

    // INPUT / OUTPUT：0..∞（顺时针递减，注意 30/36/48/∞ 要跨 360）
    const ScaleMark kInputScale[] =
    {
        { "0",  159.9f }, { "6",  119.5f }, { "12",  80.0f }, { "18",  40.5f },
        { "24",   0.5f }, { "30", 318.9f }, { "36", 279.4f }, { "48", 239.0f },
        { "∞",  198.5f }
    };

    // ATTACK / RELEASE：1/3/5/7 四档，首尾均分 6 段
    const char* kAttackLabels[] = { "1", "3", "5", "7" };

    struct CaptionDef
    {
        const char* text;
        float x, y;
    };

    const CaptionDef kCaptions[] =
    {
        { "INPUT",   136.0f, 155.0f },
        { "OUTPUT",  323.0f, 155.0f },
        { "ATTACK",  475.0f,  18.0f },
        { "RELEASE", 475.0f, 169.0f },
        { "RATIO",   596.0f,  22.0f },
        { "METER",   834.0f,  22.0f },
    };

    struct SideLabel
    {
        const char* text;
        float y;
    };

    const SideLabel kRatioLabels[] = { { "20", 44.0f }, { "12", 75.0f }, { "8", 104.0f }, { "4", 133.0f } };
    const SideLabel kMeterLabels[] = { { "GR", 47.0f }, { "+8", 79.0f }, { "+4", 108.0f }, { "OFF", 135.0f } };

    struct ButtonDef
    {
        const char* up;
        const char* down;
        juce::Rectangle<float> sprite;   // 32x40 逻辑像素（贴图本体；比视觉框大，四周有 bleed）
    };

    const ButtonDef kRatioButtons[] =
    {
        { "btn_ratio_20_up.png", "btn_ratio_20_down.png", { 582.0f,  22.0f, 32.0f, 40.0f } },
        { "btn_ratio_12_up.png", "btn_ratio_12_down.png", { 582.0f,  51.0f, 32.0f, 40.0f } },
        { "btn_ratio_8_up.png",  "btn_ratio_8_down.png",  { 582.0f,  80.0f, 32.0f, 40.0f } },
        { "btn_ratio_4_up.png",  "btn_ratio_4_down.png",  { 582.0f, 109.0f, 32.0f, 40.0f } },
    };

    const ButtonDef kMeterButtons[] =
    {
        { "meter_btn_gr_up.png",  "meter_btn_gr_down.png",  { 819.5f,  22.0f, 32.0f, 40.0f } },
        { "meter_btn_p8_up.png",  "meter_btn_p8_down.png",  { 819.5f,  51.0f, 32.0f, 40.0f } },
        { "meter_btn_p4_up.png",  "meter_btn_p4_down.png",  { 819.5f,  80.0f, 32.0f, 40.0f } },
        { "meter_btn_off_up.png", "meter_btn_off_down.png", { 819.5f, 109.0f, 32.0f, 40.0f } },
    };

    // VU 刻度：角度是“图像角”（0 = +x，屏幕顺时针为正；theta_cw = imageDeg + 90）
    struct VuLabelDef
    {
        const char* text;
        float imageDeg;
        bool  positive;
    };

    const VuLabelDef kVuLabels[] =
    {
        { "-20", -132.6f, false }, { "-10", -122.8f, false }, { "-7", -112.5f, false },
        { "-5",  -104.2f, false }, { "-3",   -94.9f, false }, { "-1",  -79.9f, false },
        { "0",    -71.0f, false }, { "+1",   -61.6f, true  }, { "+2",  -52.9f, true  },
        { "+3",   -46.0f, true  }
    };

    constexpr int kNumVuLabels = (int) std::size (kVuLabels);
    constexpr int kZeroLabelIndex = 6;

    const juce::Point<float> kVuCentre { 715.63f, 120.51f };
    const juce::Rectangle<float> kMeterWindow { 651.0f, 43.0f, 130.0f, 66.0f };
    const juce::Rectangle<float> kMeterFace { 645.0f, 42.0f, 137.0f, 60.0f };

    const juce::Colour kInk     { 0xff2e2c25 };   // rgb(46,44,37)
    const juce::Colour kRedInk  { 0xffc53a28 };   // rgb(197,58,40)
    const juce::Colour kRedBand { 0xff8f1208 };   // rgb(143,18,8)
    const juce::Colour kDotColour { 0xffcfcfd2 };
    const juce::Colour kScaleText { 0xffdcdcdf };
    const juce::Colour kCaptionText { 0xffc9c9cd };
    const juce::Colour kSideText { 0xffd6d6da };

    constexpr float kToggleBottom = 5.0f;
    constexpr float kToggleHeight = 14.0f;

    //==============================================================================
    juce::Point<float> polar (juce::Point<float> centre, float radius, float imageDeg)
    {
        const auto a = juce::degreesToRadians (imageDeg);
        return { centre.x + radius * std::cos (a), centre.y + radius * std::sin (a) };
    }

    /** 把一段圆弧拆成折线再描边 —— 比 Path::addCentredArc 的角度约定更不容易搞错。 */
    juce::Path arcPath (juce::Point<float> centre, float radius, float fromDeg, float toDeg)
    {
        juce::Path p;
        const auto steps = juce::jmax (8, juce::roundToInt (std::abs (toDeg - fromDeg) * 1.5f));

        for (int i = 0; i <= steps; ++i)
        {
            const auto t = (float) i / (float) steps;
            const auto pt = polar (centre, radius, fromDeg + (toDeg - fromDeg) * t);

            if (i == 0) p.startNewSubPath (pt);
            else        p.lineTo (pt);
        }

        return p;
    }

    juce::Font narrowFont (float height, float extraKerning = 0.0f)
    {
        auto f = juce::Font (juce::FontOptions ("Arial Narrow", height, juce::Font::plain));
        return extraKerning != 0.0f ? f.withExtraKerningFactor (extraKerning) : f;
    }

    juce::Font serifFont (float height, float extraKerning = 0.0f)
    {
        auto f = juce::Font (juce::FontOptions ("Georgia", height, juce::Font::plain));
        return extraKerning != 0.0f ? f.withExtraKerningFactor (extraKerning) : f;
    }

    /** 文字宽度：JUCE 8 的 Font::getStringWidthFloat 已废弃，用 TextLayout 量。 */
    float measureTextWidth (const juce::Font& font, const juce::String& text)
    {
        juce::AttributedString attributed;
        attributed.setFont (font);
        attributed.append (text);

        juce::TextLayout layout;
        layout.createLayout (attributed, 2000.0f, 1000.0f);
        return layout.getWidth();
    }

    void drawCentredText (juce::Graphics& g,
                          const juce::String& text,
                          juce::Point<float> centre,
                          const juce::Font& font,
                          juce::Colour colour,
                          float rotationDeg = 0.0f)
    {
        // 不需要精确宽度：给一个足够宽的矩形 + centred 对齐即可
        const juce::Rectangle<float> area { centre.x - 200.0f, centre.y - font.getHeight() * 0.9f,
                                            400.0f, font.getHeight() * 1.8f };

        g.setColour (colour);
        g.setFont (font);

        if (std::abs (rotationDeg) > 0.01f)
        {
            g.saveState();
            g.addTransform (juce::AffineTransform::rotation (juce::degreesToRadians (rotationDeg),
                                                             centre.x, centre.y));
            g.drawText (text, area, juce::Justification::centred, false);
            g.restoreState();
        }
        else
        {
            g.drawText (text, area, juce::Justification::centred, false);
        }
    }

    void drawSprite (juce::Graphics& g, const juce::Image& image, juce::Rectangle<float> dest)
    {
        if (image.isValid())
            g.drawImage (image, dest, juce::RectanglePlacement::stretchToFit, false);
    }

    /** POWER / BYPASS 两个小开关的位置（index 0 = POWER，1 = BYPASS）。 */
    std::array<juce::Rectangle<float>, 2> computeToggleRects()
    {
        std::array<juce::Rectangle<float>, 2> rects {};
        const auto font = narrowFont (9.0f);
        auto right = NativePanel::canvasW - 10.0f;

        for (int i = 1; i >= 0; --i)
        {
            const auto text = juce::String (i == 0 ? "POWER" : "BYPASS");
            const auto w = measureTextWidth (font, text) + 14.0f;

            rects[(size_t) i] = { right - w,
                                  NativePanel::canvasH - kToggleBottom - kToggleHeight,
                                  w, kToggleHeight };

            right -= w + 6.0f;
        }

        return rects;
    }

    juce::Image loadAsset (const juce::File& dir, const char* name)
    {
        const auto file = dir.getChildFile (name);

        if (! file.existsAsFile())
        {
            DBG ("NativePanel: missing asset " << name << " in " << dir.getFullPathName());
            return {};
        }

        return juce::ImageFileFormat::loadFrom (file);
    }
} // namespace

//==============================================================================
NativePanel::NativePanel()
{
    setOpaque (true);
    setSize ((int) canvasW, (int) canvasH);
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::NormalCursor);

    assetsDir = findAssetsDir();
    loadAssets();

    toggleRects = computeToggleRects();

    needle.cur = needle.target = needleTargetForState();
    needle.mode = meterMode;

    startTimerHz (60);
}

//==============================================================================
juce::File NativePanel::findAssetsDir()
{
    const auto exeDir = juce::File::getSpecialLocation (juce::File::invokedExecutableFile)
                            .getParentDirectory();

    auto dir = exeDir;

    for (int i = 0; i < 8; ++i)
    {
        for (auto* sub : { "assets", "../assets", "../../assets", "WebUI/assets" })
        {
            const auto probe = dir.getChildFile (sub).getChildFile ("panel_base.png");

            if (probe.existsAsFile())
                return probe.getParentDirectory();
        }

        dir = dir.getParentDirectory();
    }

    // 开发期回退：源码树里的 WebUI/assets（demo exe 在 build-native/ 里跑时命中）
    const auto srcPath = juce::String (__FILE__);
    auto src = juce::File (srcPath);

    if (! juce::File::isAbsolutePath (srcPath))
        src = juce::File::getCurrentWorkingDirectory().getChildFile (srcPath);

    const auto dev = src.getParentDirectory().getParentDirectory()
                        .getChildFile ("WebUI").getChildFile ("assets");

    if (dev.getChildFile ("panel_base.png").existsAsFile())
        return dev;

    return {};
}

void NativePanel::loadAssets()
{
    if (! assetsDir.isDirectory())
    {
        DBG ("NativePanel: assets dir not found — run from build-native/ or set WebUI/assets next to the exe");
        return;
    }

    panelImage  = loadAsset (assetsDir, "panel_base.png");
    glassImage  = loadAsset (assetsDir, "meter_glass.png");
    stripBig    = loadAsset (assetsDir, "knob_body_big_61f.png");
    stripSmall  = loadAsset (assetsDir, "knob_body_small_61f.png");
    dotImage    = loadAsset (assetsDir, "knob_dot.png");
    bladeImage  = loadAsset (assetsDir, "needle_blade.png");
    shadowImage = loadAsset (assetsDir, "needle_shadow.png");

    for (int i = 0; i < 4; ++i)
    {
        ratioUp[i]   = loadAsset (assetsDir, kRatioButtons[i].up);
        ratioDown[i] = loadAsset (assetsDir, kRatioButtons[i].down);
        meterUp[i]   = loadAsset (assetsDir, kMeterButtons[i].up);
        meterDown[i] = loadAsset (assetsDir, kMeterButtons[i].down);
    }
}

//==============================================================================
float NativePanel::getUiScale() const
{
    auto scale = 1.0f;

    if (auto* peer = getPeer())
        scale = (float) peer->getPlatformScaleFactor();

    return juce::jlimit (1.0f, 3.0f, scale);
}

void NativePanel::buildCachesIfNeeded()
{
    const auto scale = getUiScale();

    if (panelCache.isValid() && std::abs (scale - cacheScale) < 0.01f)
        return;

    cacheScale = scale;

    const auto w = juce::roundToInt (canvasW * scale);
    const auto h = juce::roundToInt (canvasH * scale);

    const auto renderCache = [&] (juce::Image& target, const std::function<void (juce::Graphics&)>& draw)
    {
        target = juce::Image (juce::Image::ARGB, w, h, true);
        juce::Graphics g (target);
        g.addTransform (juce::AffineTransform::scale (scale));
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        draw (g);
    };

    renderCache (panelCache,   [this] (juce::Graphics& g) { paintPanel (g); });
    renderCache (overlayCache, [this] (juce::Graphics& g) { paintOverlay (g); });

    renderCache (glassCache,   [this] (juce::Graphics& g)
    {
        if (glassImage.isValid())
            g.drawImage (glassImage, { 0.0f, 0.0f, canvasW, canvasH },
                         juce::RectanglePlacement::stretchToFit, false);
    });
}

//==============================================================================
void NativePanel::paintPanel (juce::Graphics& g)
{
    // 面板底图（1900x350 @2x -> 950x175 逻辑）
    if (panelImage.isValid())
        g.drawImage (panelImage, { 0.0f, 0.0f, canvasW, canvasH },
                     juce::RectanglePlacement::stretchToFit, false);
    else
        g.fillAll (juce::Colour (0xff08080a));
}

void NativePanel::paintOverlay (juce::Graphics& g)
{
    paintKnobScales (g);
    paintCaptions (g);
    paintVuScale (g);
}

//==============================================================================
void NativePanel::paintKnobScales (juce::Graphics& g)
{
    // 大旋钮：9 个刻度点（r=44 小圆点 + r=52 数字）
    for (int i = 0; i < 2; ++i)
    {
        const auto& d = kKnobDefs[i];
        const juce::Point<float> centre { d.pivotX, d.pivotY };

        for (const auto& m : kInputScale)
        {
            const auto imageDeg = m.angleCw - 90.0f;

            const auto dot = polar (centre, 44.0f, imageDeg);
            g.setColour (kDotColour.withAlpha (0.85f));
            g.fillEllipse (dot.x - 1.1f, dot.y - 1.1f, 2.2f, 2.2f);

            const auto labelPos = polar (centre, 52.0f, imageDeg);
            drawCentredText (g, m.label, labelPos, narrowFont (10.5f), kScaleText);
        }
    }

    // 小旋钮：1/3/5/7 等距（首尾角度均分 6 段，共 7 个刻度点）
    constexpr float a0 = 221.7f;
    constexpr float a1 = 140.3f + 360.0f;
    constexpr int segments = 6;
    const auto step = (a1 - a0) / (float) segments;

    for (int k = 2; k < 4; ++k)
    {
        const auto& d = kKnobDefs[k];
        const juce::Point<float> centre { d.pivotX, d.pivotY };

        for (int t = 0; t <= segments; ++t)
        {
            const auto imageDeg = a0 + step * (float) t - 90.0f;

            const auto dot = polar (centre, 23.0f, imageDeg);
            g.setColour (kDotColour.withAlpha (0.8f));
            g.fillEllipse (dot.x - 0.95f, dot.y - 0.95f, 1.9f, 1.9f);

            if (t % 2 == 0)
            {
                const auto labelPos = polar (centre, 31.0f, imageDeg);
                drawCentredText (g, kAttackLabels[t / 2], labelPos, narrowFont (9.5f), kScaleText);
            }
        }
    }
}

void NativePanel::paintCaptions (juce::Graphics& g)
{
    const auto font = narrowFont (10.0f, 0.08f);

    for (const auto& c : kCaptions)
        drawCentredText (g, c.text, { c.x, c.y }, font, kCaptionText);

    // RATIO 标签（右对齐到 x=572）
    for (const auto& l : kRatioLabels)
    {
        g.setColour (kSideText);
        g.setFont (narrowFont (11.0f));
        g.drawText (l.text, juce::Rectangle<float> (572.0f - 80.0f, l.y - 8.0f, 80.0f, 16.0f),
                    juce::Justification::centredRight, false);
    }

    // METER 标签（左对齐到 x=852）
    for (const auto& l : kMeterLabels)
    {
        g.setColour (kSideText);
        g.setFont (narrowFont (11.0f));
        g.drawText (l.text, juce::Rectangle<float> (852.0f, l.y - 8.0f, 80.0f, 16.0f),
                    juce::Justification::centredLeft, false);
    }
}

void NativePanel::paintVuScale (juce::Graphics& g)
{
    const auto c = kVuCentre;
    constexpr float rArc = 42.8f;
    constexpr float rOut = 61.0f;
    constexpr float rIn = 43.0f;
    constexpr float rMinOut = 53.0f;
    constexpr float rLabel = 66.0f;

    // 刻度弧：负 dB 段墨色
    {
        const auto path = arcPath (c, rArc, kVuLabels[0].imageDeg, kVuLabels[kZeroLabelIndex].imageDeg);
        g.setColour (kInk);
        g.strokePath (path, juce::PathStrokeType (1.0f));
    }

    // 红区：0 .. +3 同一半径加粗（TOiRE-2A 的做法）
    {
        const auto path = arcPath (c, rArc, kVuLabels[kZeroLabelIndex].imageDeg,
                                   kVuLabels[kNumVuLabels - 1].imageDeg);
        g.setColour (kRedBand.withAlpha (0.95f));
        g.strokePath (path, juce::PathStrokeType (4.0f));
    }

    for (int i = 0; i < kNumVuLabels; ++i)
    {
        const auto& lab = kVuLabels[i];
        const auto colour = lab.positive ? kRedInk : kInk;

        // 主刻度
        const auto outer = polar (c, rOut, lab.imageDeg);
        const auto inner = polar (c, rIn, lab.imageDeg);
        g.setColour (colour);
        g.drawLine ({ outer, inner }, 1.3f);

        // dB 数字：各自刻度角、r=66，沿弧倾一半
        const auto labelPos = polar (c, rLabel, lab.imageDeg);
        drawCentredText (g, lab.text, labelPos,
                         narrowFont (lab.positive ? 6.2f : 7.4f), colour,
                         (lab.imageDeg + 90.0f) * 0.55f);

        // 次刻度：相邻主刻度中点
        if (i + 1 < kNumVuLabels)
        {
            const auto mid = (lab.imageDeg + kVuLabels[i + 1].imageDeg) * 0.5f;
            const auto bothPositive = lab.positive && kVuLabels[i + 1].positive;

            const auto mOuter = polar (c, rMinOut, mid);
            const auto mInner = polar (c, rIn, mid);
            g.setColour ((bothPositive ? kRedInk : kInk).withAlpha (0.9f));
            g.drawLine ({ mOuter, mInner }, 1.1f);
        }
    }

    // 百分比刻度：0..100 均分在首尾刻度角之间，r=37
    {
        const char* percentText[] = { "0", "20", "40", "60", "80", "100" };
        const auto first = kVuLabels[0].imageDeg;
        const auto last = kVuLabels[kNumVuLabels - 1].imageDeg;

        for (int i = 0; i < 6; ++i)
        {
            const auto a = first + (last - first) * (float) i / 5.0f;
            drawCentredText (g, percentText[i], polar (c, 37.0f, a), narrowFont (6.6f), kInk);
        }
    }

    // 表盘角上的 VU 字
    drawCentredText (g, "VU", { kMeterFace.getX() + 9.0f, kMeterFace.getY() + 9.0f },
                     serifFont (8.2f), kInk);
    drawCentredText (g, "VU", { kMeterFace.getRight() - 9.0f, kMeterFace.getY() + 9.0f },
                     serifFont (8.2f), kInk);

    // 表盘内品牌字
    drawCentredText (g, "TOiRE", { c.x, 96.5f }, serifFont (7.4f), juce::Colour (0xff3a3022));

    // 三行铭牌（Kings 不可用，用系统衬线 Georgia 代替）
    drawCentredText (g, "TOiRE-76", { c.x, 134.0f }, serifFont (15.0f, 0.08f), juce::Colour (0xffdcdcdf));
    drawCentredText (g, "Toire style", { c.x, 145.5f }, serifFont (8.6f, 0.08f), juce::Colour (0xff9a9aa0));
    drawCentredText (g, "FET COMPRESSOR / LIMITING AMPLIFIER", { c.x, 157.0f },
                     serifFont (9.4f, 0.04f), juce::Colour (0xffc6c6ca));
}

//==============================================================================
void NativePanel::paintNeedle (juce::Graphics& g)
{
    if (! bladeImage.isValid() && ! shadowImage.isValid())
        return;

    g.saveState();
    g.reduceClipRegion (kMeterWindow.toNearestInt());

    const auto rotation = juce::AffineTransform::rotation (juce::degreesToRadians (needle.cur),
                                                          kVuCentre.x, kVuCentre.y);
    g.addTransform (rotation);

    const juce::Rectangle<float> dest { kVuCentre.x - 90.0f, kVuCentre.y - 90.0f, 180.0f, 180.0f };

    if (shadowImage.isValid())
    {
        g.setOpacity (0.45f);
        g.drawImage (shadowImage, dest, juce::RectanglePlacement::stretchToFit, false);
        g.setOpacity (1.0f);
    }

    if (bladeImage.isValid())
        g.drawImage (bladeImage, dest, juce::RectanglePlacement::stretchToFit, false);

    g.restoreState();
}

void NativePanel::paintKnobs (juce::Graphics& g)
{
    for (int i = 0; i < 4; ++i)
    {
        const auto& d = kKnobDefs[i];
        const auto& strip = d.isBig ? stripBig : stripSmall;

        const int frameSize = d.isBig ? 192 : 120;
        const float destSize = d.isBig ? 96.0f : 60.0f;

        if (strip.isValid())
        {
            // frame 序列方向和指针方向相反（UAD 素材惯例），WebUI 里是 60 - round(v*60)
            const auto frame = juce::jlimit (0, 60, 60 - juce::roundToInt (knobValues[i] * 60.0f));
            const auto frameImage = strip.getClippedImage ({ 0, frame * frameSize, frameSize, frameSize });

            g.drawImage (frameImage,
                         { d.pivotX - destSize * 0.5f, d.pivotY - destSize * 0.5f, destSize, destSize },
                         juce::RectanglePlacement::stretchToFit, false);
        }

        // 小旋钮的指示点：knob_dot.png 绕轴心旋转
        if (! d.isBig && dotImage.isValid())
        {
            const auto angle = d.angleMinDeg + knobValues[i] * (d.angleMaxDeg - d.angleMinDeg);

            g.saveState();
            g.addTransform (juce::AffineTransform::rotation (juce::degreesToRadians (angle),
                                                             d.pivotX, d.pivotY));
            g.drawImage (dotImage,
                         { d.pivotX - 20.0f, d.pivotY - 20.0f, 40.0f, 40.0f },
                         juce::RectanglePlacement::stretchToFit, false);
            g.restoreState();
        }
    }
}

void NativePanel::paintButtons (juce::Graphics& g)
{
    for (int i = 0; i < 4; ++i)
    {
        // RATIO：row 0 = 20:1（ratio 值 3）…… row 3 = 4:1（ratio 值 0）；ratio==4 时四键全按下
        const auto ratioOfRow = 3 - i;
        const bool down = (ratioValue == 4) || (ratioValue == ratioOfRow);

        drawSprite (g, down ? ratioDown[i] : ratioUp[i], kRatioButtons[i].sprite);

        // METER：四档单选
        const bool meterIsDown = (meterMode == i);
        drawSprite (g, meterIsDown ? meterDown[i] : meterUp[i], kMeterButtons[i].sprite);
    }
}

void NativePanel::paintBigKnobPointers (juce::Graphics& g)
{
    // 大旋钮白线：r 24..40.5，深色底线 2.5 + 白线 1.75
    for (int i = 0; i < 2; ++i)
    {
        const auto& d = kKnobDefs[i];
        const auto angle = d.isBig ? bigKnobAngle (knobValues[i])
                                   : d.angleMinDeg + knobValues[i] * (d.angleMaxDeg - d.angleMinDeg);
        const auto rad = juce::degreesToRadians (angle - 90.0f);
        const juce::Point<float> centre { d.pivotX, d.pivotY };

        const juce::Point<float> p0 { centre.x + 24.0f * std::cos (rad), centre.y + 24.0f * std::sin (rad) };
        const juce::Point<float> p1 { centre.x + 40.5f * std::cos (rad), centre.y + 40.5f * std::sin (rad) };

        juce::Path line;
        line.startNewSubPath (p0);
        line.lineTo (p1);

        g.setColour (juce::Colour (0xff111111).withAlpha (0.85f));
        g.strokePath (line, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        g.setColour (juce::Colours::white);
        g.strokePath (line, juce::PathStrokeType (1.75f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }
}

void NativePanel::paintToggles (juce::Graphics& g)
{
    const auto rects = toggleRects;
    const char* labels[] = { "POWER", "BYPASS" };
    const bool states[] = { powerOn, bypassed };

    for (int i = 0; i < 2; ++i)
    {
        const auto r = rects[(size_t) i];
        const auto on = states[i];

        g.setColour (juce::Colour (0xff08080a).withAlpha (0.55f));
        g.fillRoundedRectangle (r, 2.0f);

        g.setColour (on ? juce::Colour (0xff8a6a2a) : juce::Colour (0xff3a3a42));
        g.drawRoundedRectangle (r, 2.0f, 1.0f);

        g.setColour (on ? juce::Colour (0xffe0b050) : juce::Colour (0xff6a6a72));
        g.setFont (narrowFont (9.0f, 0.06f));
        g.drawText (labels[i], r, juce::Justification::centred, false);
    }
}

//==============================================================================
void NativePanel::paint (juce::Graphics& g)
{
    buildCachesIfNeeded();

    const juce::Rectangle<float> canvas { 0.0f, 0.0f, canvasW, canvasH };

    // 1. 面板底图
    if (panelCache.isValid())
        g.drawImage (panelCache, canvas, juce::RectanglePlacement::stretchToFit, false);
    else
        paintPanel (g);

    // 2. VU 指针（z-auto，在 overlay 之下）
    paintNeedle (g);

    // 3. 表头玻璃（z-auto，在指针之上）
    if (glassCache.isValid())
        g.drawImage (glassCache, canvas, juce::RectanglePlacement::stretchToFit, false);
    else if (glassImage.isValid())
        g.drawImage (glassImage, canvas, juce::RectanglePlacement::stretchToFit, false);

    // 4. 旋钮 / 按键（z40 / z41）
    paintKnobs (g);
    paintButtons (g);

    // 5. overlay（CSS z-index:10 —— 盖在旋钮和指针上面）
    if (overlayCache.isValid())
        g.drawImage (overlayCache, canvas, juce::RectanglePlacement::stretchToFit, false);
    else
        paintOverlay (g);

    // 6. overlay 里会动的部分 + 最上层的 POWER/BYPASS
    paintBigKnobPointers (g);
    paintToggles (g);
}

//==============================================================================
float NativePanel::vuThetaForDb (float db) const
{
    // 由 VU 刻度标签推出的 dB -> theta_cw 映射（theta_cw = imageDeg + 90）
    static const float dbs[]  = { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -1.0f, 0.0f,  1.0f,  2.0f,  3.0f };
    static const float thetas[] = { -42.6f, -32.8f, -22.5f, -14.2f, -4.9f, 10.1f, 19.0f, 28.4f, 37.1f, 44.0f };

    if (db <= dbs[0]) return thetas[0];

    for (int i = 1; i < kNumVuLabels; ++i)
    {
        if (db <= dbs[i])
        {
            const auto f = (db - dbs[i - 1]) / (dbs[i] - dbs[i - 1]);
            return thetas[i - 1] + f * (thetas[i] - thetas[i - 1]);
        }
    }

    return thetas[kNumVuLabels - 1];
}

float NativePanel::needleTargetForState() const
{
    // 标准 VU 停位：GR 停 0；+8/+4（没有电平数据时）与 OFF 都停最左 -20
    if (! powerOn)
        return vuThetaForDb (-20.0f);

    if (meterMode == 0)
        return vuThetaForDb (0.0f);

    return vuThetaForDb (-20.0f);
}

void NativePanel::applyMeterMode (bool force)
{
    const auto target = needleTargetForState();

    if (force || meterMode != needle.mode)
    {
        needle.mode = meterMode;

        if (meterMode == 0)                 { needle.k = 700.0f; needle.c = 38.0f; }
        else if (meterMode == 1 || meterMode == 2) { needle.k = 420.0f; needle.c = 30.0f; }
        else                                { needle.k = 300.0f; needle.c = 28.0f; }

        needle.target = target;
        needle.animating = true;
    }
    else if (! needle.animating)
    {
        // 同档内的数值变化直接落位（真实弹道以后由 DSP 驱动）
        needle.target = target;
        needle.cur = target;
        needle.vel = 0.0f;
    }
}

void NativePanel::timerCallback()
{
    if (! needle.animating)
        return;

    constexpr float dt = 1.0f / 60.0f;
    const auto accel = -needle.k * (needle.cur - needle.target) - needle.c * needle.vel;
    needle.vel += accel * dt;
    needle.cur += needle.vel * dt;

    if (std::abs (needle.vel) < 0.02f && std::abs (needle.cur - needle.target) < 0.02f)
    {
        needle.cur = needle.target;
        needle.vel = 0.0f;
        needle.animating = false;
    }

    // 指针只在表窗里可见（刀身超出部分被裁剪）
    repaint (kMeterWindow.expanded (2.0f, 2.0f).toNearestInt());
}

//==============================================================================
int NativePanel::hitTestKnob (juce::Point<float> pos) const
{
    for (int i = 0; i < 4; ++i)
    {
        const auto& d = kKnobDefs[i];
        const auto half = d.isBig ? 48.0f : 30.0f;
        const juce::Rectangle<float> hit { d.pivotX - half, d.pivotY - half, half * 2.0f, half * 2.0f };

        if (hit.contains (pos))
            return i;
    }

    return -1;
}

void NativePanel::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.position;

    if (toggleRects[0].contains (pos))
    {
        powerOn = ! powerOn;
        applyMeterMode (true);
        repaint();
        return;
    }

    if (toggleRects[1].contains (pos))
    {
        bypassed = ! bypassed;
        repaint();
        return;
    }

    for (int i = 0; i < 4; ++i)
    {
        if (kRatioButtons[i].sprite.contains (pos))
        {
            ratioValue = e.mods.isShiftDown() ? 4 : (3 - i);
            repaint();
            return;
        }
    }

    for (int i = 0; i < 4; ++i)
    {
        if (kMeterButtons[i].sprite.contains (pos))
        {
            meterMode = i;
            applyMeterMode (false);
            repaint();
            return;
        }
    }

    if (const auto knob = hitTestKnob (pos); knob >= 0)
    {
        draggingKnob = knob;
        dragLastY = pos.y;
    }
}

void NativePanel::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingKnob < 0)
        return;

    const auto dy = dragLastY - e.position.y;
    dragLastY = e.position.y;

    knobValues[draggingKnob] = juce::jlimit (0.0f, 1.0f, knobValues[draggingKnob] + dy / 180.0f);
    repaint();
}

void NativePanel::mouseUp (const juce::MouseEvent&)
{
    draggingKnob = -1;
}

void NativePanel::mouseMove (const juce::MouseEvent& e)
{
    const auto pos = e.position;

    for (const auto& b : kRatioButtons)
        if (b.sprite.contains (pos)) { setMouseCursor (juce::MouseCursor::PointingHandCursor); return; }

    for (const auto& b : kMeterButtons)
        if (b.sprite.contains (pos)) { setMouseCursor (juce::MouseCursor::PointingHandCursor); return; }

    if (toggleRects[0].contains (pos) || toggleRects[1].contains (pos))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        return;
    }

    setMouseCursor (hitTestKnob (pos) >= 0 ? juce::MouseCursor::UpDownResizeCursor
                                           : juce::MouseCursor::NormalCursor);
}

void NativePanel::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto knob = hitTestKnob (e.position);

    if (knob < 0)
        return;

    // JUCE 的 deltaY > 0 = 向上滚；WebUI 里向上滚 = 加值
    const auto delta = (wheel.deltaY > 0.0f ? 1.0f : -1.0f) * 0.02f;
    knobValues[knob] = juce::jlimit (0.0f, 1.0f, knobValues[knob] + delta);
    repaint();
}
