/*
 ==============================================================================
 
 Copyright 2024 Ivica Milovanovic (excluding JUCE framework code)
 
 This file is part of AU-VST3-Wrapper
 
 AU-VST3-Wrapper is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 AU-VST3-Wrapper is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 You should have received a copy of the GNU General Public License along with AU-VST3-Wrapper. If not, see <https://www.gnu.org/licenses/>.
 
 ==============================================================================
 */


#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "VST3FileBrowser.h"

//==============================================================================
/**
*/

class VST3WrapperAudioProcessorEditor  : 
public juce::AudioProcessorEditor,
public juce::ChangeListener,
public juce::Button::Listener,
public juce::FileBrowserListener,
public juce::ComponentListener,
private juce::Timer
{
public:
    VST3WrapperAudioProcessorEditor (VST3WrapperAudioProcessor&);
    ~VST3WrapperAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void buttonClicked(juce::Button*) override;
    void selectionChanged() override;
    void fileClicked (const juce::File& file, const juce::MouseEvent& e) override;
    void fileDoubleClicked (const juce::File& file) override;
    void browserRootChanged (const juce::File& newRoot) override;
    void componentMovedOrResized (Component& component, bool wasMoved, bool wasResized) override;

private:
    class SemiTransparentComponent : public juce::Component
    {
    public:
        
        SemiTransparentComponent()
        {
        }
        
        ~SemiTransparentComponent()
        {
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colours::white.withAlpha(0.1f));
        }
    };

    class ClearSearchButton : public juce::Button
    {
    public:
        ClearSearchButton()
        : juce::Button("Clear search")
        {
        }

        void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
        {
            auto bounds = getLocalBounds().toFloat().reduced(1.0f);
            g.setColour(findColour(juce::TextButton::buttonColourId)
                            .brighter(shouldDrawButtonAsHighlighted ? 0.15f : 0.0f)
                            .darker(shouldDrawButtonAsDown ? 0.15f : 0.0f));
            g.fillRoundedRectangle(bounds, 2.0f);

            const auto crossBounds = bounds.reduced(8.0f);
            g.setColour(findColour(juce::TextButton::textColourOffId));
            g.drawLine(crossBounds.getX(), crossBounds.getY(), crossBounds.getRight(), crossBounds.getBottom(), 1.6f);
            g.drawLine(crossBounds.getRight(), crossBounds.getY(), crossBounds.getX(), crossBounds.getBottom(), 1.6f);
        }
    };
    
    void timerCallback() override;
    
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    VST3WrapperAudioProcessor& audioProcessor;
    juce::ThreadPool threadPool {1};
    std::unique_ptr<juce::AudioProcessorEditor> hostedPluginEditor;
    void startLoadingPlugin(std::function<void()> loadOperation);
    void loadPlugin(const juce::String& filePath, const juce::PluginDescription& pluginDescription);
    void loadSelectedPluginFile();
    void showPluginSelectionDialog(const juce::String& filePath, const juce::Array<juce::PluginDescription>& pluginDescriptions);
    void closePlugin();
    void setHostedPluginEditorIfNeeded();
    
    std::unique_ptr<VST3FileBrowserComponent> pluginFileBrowser;
    juce::TextEditor pluginSearchBox;
    ClearSearchButton clearSearchButton;
    SemiTransparentComponent pluginFileBrowserCover;
    juce::TextButton loadPluginButton;
    juce::TextButton closePluginButton;
    juce::Label statusLabel;
    std::unique_ptr<juce::AlertWindow> pluginSelectionDialog;
    void setLoadingState();
    void processorStateChanged(bool shouldShowPluginLoadingError);
    void drawSidechainArrow(juce::Graphics& g);
    
    static inline const juce::String noPluginLoadedMessage = "No plugin loaded";
    static inline const juce::String pluginSelectionComboBoxName = "plugin";
    static constexpr int defaultEditorWidth = 650;
    static constexpr int margin = 10;
    static constexpr int browserHeight = 400;
    static constexpr int searchBoxHeight = 28;
    static constexpr int searchBoxSpacing = 6;
    static constexpr int clearSearchButtonWidth = 28;
    static constexpr int labelHeight = 30;
    static constexpr int buttonHeight = 30;
    static constexpr int buttonTopspacing = 5;
    
    int getEditorWidth()
    {
        return hostedPluginEditor == nullptr ? defaultEditorWidth : hostedPluginEditor->getWidth();
    }
    
    int getHostedPluginEditorOrPluginListHeight()
    {
        return hostedPluginEditor == nullptr ? browserHeight : hostedPluginEditor->getHeight();
    }
    
    int getEditorHeight()
    {
        const auto buttonViewHeight = buttonHeight + buttonTopspacing;
        return  getHostedPluginEditorOrPluginListHeight() + buttonViewHeight + labelHeight;
    }
    
    int getButtonOriginY()
    {
        return getHostedPluginEditorOrPluginListHeight() + buttonTopspacing;
    }

    int getFileBrowserOriginY()
    {
        return margin + searchBoxHeight + searchBoxSpacing;
    }

    int getFileBrowserHeight()
    {
        return browserHeight - getFileBrowserOriginY();
    }
    
    int getLabelriginY()
    {
        return getButtonOriginY() + buttonHeight;
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VST3WrapperAudioProcessorEditor)
};
