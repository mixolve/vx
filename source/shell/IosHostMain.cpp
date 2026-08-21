#include <JuceHeader.h>

#include "module.eql.ProcessorSupport.h"
#include "shell.Editor.h"

class HostContent final : public juce::Component
{
public:
    HostContent()
    {
        processor = std::make_unique<AvaAudioProcessor>();
        editor = std::make_unique<AvaAudioProcessorEditor>(*processor);
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
    std::unique_ptr<AvaAudioProcessor> processor;
    std::unique_ptr<AvaAudioProcessorEditor> editor;
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
        setResizable(false, false);
        setContentOwned(new HostContent(), true);

        const auto& displays = juce::Desktop::getInstance().getDisplays();
        const auto* primaryDisplay = displays.getPrimaryDisplay();
        const auto displayBounds = primaryDisplay != nullptr ? primaryDisplay->userArea : displays.getTotalBounds(true);
        centreWithSize(displayBounds.getWidth(), displayBounds.getHeight());
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

class AvaIosHostApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "ava"; }
    const juce::String getApplicationVersion() override { return "0.1.2"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        syncEqlPresetStorageWithSharedContainer();
        juce::Desktop::getInstance().setOrientationsEnabled(juce::Desktop::upright);
        mainWindow = std::make_unique<HostWindow>(getApplicationName());
    }

    void resumed() override
    {
        syncEqlPresetStorageWithSharedContainer();
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

START_JUCE_APPLICATION(AvaIosHostApplication)
