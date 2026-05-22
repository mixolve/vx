#include "shell.EditorParameterControls.h"

void VxAudioProcessorEditor::setupShellControls()
{
    clipControl = std::make_unique<LocalParameterControl>("CLIP",
                                                          0,
                                                          0.0,
                                                          1.0,
                                                          1.0,
                                                          0.0);
    clipControl->setInteractionEnabled(false);
    clipControl->setValue(audioProcessor.getGlobalClipIndicator(), false);
    clipControl->setTitleLongPressAction([this]
    {
        audioProcessor.resetGlobalClipIndicator();

        if (clipControl != nullptr)
            clipControl->setValue(0.0, false);

        if (shellGlobalHeader != nullptr)
            shellGlobalHeader->setLeadingDotLevel(0.0f);

        clearKeyboardFocus(*this);
    });
    addAndMakeVisible(*clipControl);

    outGainControl = std::make_unique<ParameterControl>(valueTreeState,
                                                        VxAudioProcessor::paramOutGainId,
                                                        "GAIN.OUT",
                                                        2);
    outGainControl->setTitleLongPressAction([this]
    {
        if (outGainControl != nullptr)
            outGainControl->setValue(0.0, true);

        clearKeyboardFocus(*this);
    });
    addAndMakeVisible(*outGainControl);

    gainLControl = std::make_unique<ParameterControl>(valueTreeState,
                                                      VxAudioProcessor::paramGlobalGainLId,
                                                      "GAIN.L",
                                                      2);
    gainLControl->setTitleLongPressAction([this]
    {
        if (gainLControl != nullptr)
            gainLControl->setValue(0.0, true);

        clearKeyboardFocus(*this);
    });
    addAndMakeVisible(*gainLControl);

    gainRControl = std::make_unique<ParameterControl>(valueTreeState,
                                                      VxAudioProcessor::paramGlobalGainRId,
                                                      "GAIN.R",
                                                      2);
    gainRControl->setTitleLongPressAction([this]
    {
        if (gainRControl != nullptr)
            gainRControl->setValue(0.0, true);

        clearKeyboardFocus(*this);
    });
    addAndMakeVisible(*gainRControl);

    wideControl = std::make_unique<ParameterControl>(valueTreeState,
                                                     VxAudioProcessor::paramGlobalWideId,
                                                     "WIDE",
                                                     0);
    wideControl->setTitleLongPressAction([this]
    {
        if (wideControl != nullptr)
            wideControl->setValue(100.0, true);

        clearKeyboardFocus(*this);
    });
    addAndMakeVisible(*wideControl);

    globalBypassButton = std::make_unique<BoxTextButton>(uiAccent);
    globalBypassButton->setButtonText("BYPASS");
    globalBypassButton->setTextJustification(juce::Justification::centred);
    globalBypassButton->setClickingTogglesState(true);
    globalBypassAttachment = std::make_unique<ButtonAttachment>(valueTreeState,
                                                                 VxAudioProcessor::paramGlobalBypassId,
                                                                 *globalBypassButton);
    globalBypassButton->onClick = [this]
    {
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*globalBypassButton);

    globalBypassOutGainOnlyButton = std::make_unique<BoxTextButton>(uiAccent);
    globalBypassOutGainOnlyButton->setButtonText("BYPASS.WT-GAIN");
    globalBypassOutGainOnlyButton->setTextJustification(juce::Justification::centred);
    globalBypassOutGainOnlyButton->setClickingTogglesState(true);
    globalBypassOutGainOnlyAttachment = std::make_unique<ButtonAttachment>(valueTreeState,
                                                                            VxAudioProcessor::paramGlobalBypassOutGainOnlyId,
                                                                            *globalBypassOutGainOnlyButton);
    globalBypassOutGainOnlyButton->onClick = [this]
    {
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*globalBypassOutGainOnlyButton);

    moduleCloseButton = std::make_unique<BoxTextButton>(uiAccent);
    moduleCloseButton->setButtonText("CLOSE-MODULE");
    moduleCloseButton->setTextJustification(juce::Justification::centred);
    moduleCloseButton->onClick = [this]
    {
        clearKeyboardFocus(*this);
    };
    moduleCloseButton->setLongPressAction([this]
    {
        closeActiveModule();
        clearKeyboardFocus(*this);
    }, 500, "SURE?");
    globalContent.addAndMakeVisible(*moduleCloseButton);

    speModuleCloseButton = std::make_unique<BoxTextButton>(uiAccent);
    speModuleCloseButton->setButtonText("CLOSE-MODULE");
    speModuleCloseButton->setTextJustification(juce::Justification::centred);
    speModuleCloseButton->onClick = [this]
    {
        clearKeyboardFocus(*this);
    };
    speModuleCloseButton->setLongPressAction([this]
    {
        closeActiveModule();
        clearKeyboardFocus(*this);
    }, 500, "SURE?");
    globalContent.addAndMakeVisible(*speModuleCloseButton);

    clearFiltersButton = std::make_unique<BoxTextButton>(uiAccent);
    clearFiltersButton->setButtonText("DELETE-ALL");
    clearFiltersButton->setTextJustification(juce::Justification::centred);
    clearFiltersButton->onClick = [this]
    {
        clearKeyboardFocus(*this);
    };
    clearFiltersButton->setLongPressAction([this]
    {
        clearAllFilters();
        clearKeyboardFocus(*this);
    }, 500, "SURE?");
    globalContent.addAndMakeVisible(*clearFiltersButton);

    undoButton = std::make_unique<BoxTextButton>(uiGrey500);
    undoButton->setButtonText("UNDO");
    undoButton->setTextJustification(juce::Justification::centred);
    undoButton->onClick = [this]
    {
        performUndo();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*undoButton);

    redoButton = std::make_unique<BoxTextButton>(uiGrey500);
    redoButton->setButtonText("REDO");
    redoButton->setTextJustification(juce::Justification::centred);
    redoButton->onClick = [this]
    {
        performRedo();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*redoButton);

    sortPlaceButton = std::make_unique<BoxTextButton>(uiGrey500);
    sortPlaceButton->setButtonText("SORT.PLACE");
    sortPlaceButton->setTextJustification(juce::Justification::centred);
    sortPlaceButton->onClick = [this]
    {
        sortBellSectionsByPlace();
        clearKeyboardFocus(*this);
    };
    globalContent.addAndMakeVisible(*sortPlaceButton);

    sortFreqButton = std::make_unique<BoxTextButton>(uiGrey500);
    sortFreqButton->setButtonText("SORT.FREQ");
    sortFreqButton->setTextJustification(juce::Justification::centred);
    sortFreqButton->onClick = [this]
    {
        sortBellSectionsByFrequency();
        clearKeyboardFocus(*this);
    };
    globalContent.addAndMakeVisible(*sortFreqButton);

    sortDuoButton = std::make_unique<BoxTextButton>(uiGrey500);
    sortDuoButton->setButtonText("SORT.DUO");
    sortDuoButton->setTextJustification(juce::Justification::centred);
    sortDuoButton->onClick = [this]
    {
        sortBellSectionsByDuo();
        clearKeyboardFocus(*this);
    };
    globalContent.addAndMakeVisible(*sortDuoButton);

    filtersHeader = std::make_unique<BoxTextButton>(uiAccent);
    filtersHeader->setButtonText("FILTERS");
    filtersHeader->setClickingTogglesState(true);
    filtersHeader->onClick = [this]
    {
        openFiltersSection();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*filtersHeader);

    visualizerHeader = std::make_unique<BoxTextButton>(uiAccent);
    visualizerHeader->setButtonText("VISUALIZER");
    visualizerHeader->setClickingTogglesState(true);
    visualizerHeader->onClick = [this]
    {
        openVisualizerSection();
        clearKeyboardFocus(*this);
    };
    addAndMakeVisible(*visualizerHeader);
}
