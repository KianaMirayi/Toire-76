#include <JuceHeader.h>

#include "NativePanel.h"

/**
 * TOiRE-76 原生渲染 POC 的宿主窗口。
 *
 * 只做一件事：开一个固定 950x175 的窗口，把 NativePanel 放进去。
 * 不做音频、不做 DSP —— 这是纯 UI 渲染对比用的 demo。
 */
class NativeDemoApplication final : public juce::JUCEApplication
{
public:
    NativeDemoApplication() = default;

    //==============================================================================
    const juce::String getApplicationName() override    { return "TOiRE-76 Native Demo"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    //==============================================================================
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (const juce::String& title)
            : DocumentWindow (title,
                              juce::Colour (0xff0a0a0c),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);

            // true = 窗口按内容尺寸收缩（内容已经固定 950x175）
            setContentOwned (new NativePanel(), true);

            setResizable (false, false);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (NativeDemoApplication)
