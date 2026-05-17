#include <JuceHeader.h>
#include "Editor.h"

class HostContent final : public juce::Component
{
public:
    HostContent()
    {
        processor = std::make_unique<EqeAudioProcessor>();
        editor = std::make_unique<EqeAudioProcessorEditor>(*processor);
        editor->setResizable(false, false);
        addAndMakeVisible(*editor);
        setOpaque(true);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);
    }

    void resized() override
    {
        if (editor != nullptr)
            editor->setBounds(getLocalBounds());
    }

private:
    std::unique_ptr<EqeAudioProcessor> processor;
    std::unique_ptr<EqeAudioProcessorEditor> editor;
};

class HostWindow final : public juce::DocumentWindow
{
public:
    explicit HostWindow(juce::String name)
        : juce::DocumentWindow(std::move(name), juce::Colours::black, 0)
    {
        setUsingNativeTitleBar(false);
        setTitleBarButtonsRequired(0, false);
        setTitleBarHeight(0);
        setResizable(true, false);
        setContentOwned(new HostContent(), true);
        centreWithSize(juce::jmin(juce::Desktop::getInstance().getDisplays().getMainDisplay().userArea.getWidth(), 420),
                       juce::jmin(juce::Desktop::getInstance().getDisplays().getMainDisplay().userArea.getHeight(), 260));
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

class VxIosHostApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "Vx Host"; }
    const juce::String getApplicationVersion() override    { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise(const juce::String&) override
    {
        juce::Desktop::getInstance().setOrientationsEnabled(juce::Desktop::upright);
        mainWindow = std::make_unique<HostWindow>(getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String&) override {}

private:
    std::unique_ptr<HostWindow> mainWindow;
};

START_JUCE_APPLICATION(VxIosHostApplication)
