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

    class PluginFileBrowserLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        void drawFileBrowserRow(juce::Graphics& g, int width, int height,
                                const juce::File& file,
                                const juce::String& filename,
                                juce::Image* optionalIcon,
                                const juce::String& fileSizeDescription,
                                const juce::String& fileTimeDescription,
                                bool isDirectory,
                                bool isItemSelected,
                                int itemIndex,
                                juce::DirectoryContentsDisplayComponent& dcc) override
        {
            const auto isVST3Plugin = file.hasFileExtension(VST3FileBrowserComponent::vst3Extension);

            if (!isVST3Plugin && !isDirectory)
            {
                juce::LookAndFeel_V4::drawFileBrowserRow(g, width, height, file, filename, optionalIcon,
                                                         fileSizeDescription, fileTimeDescription,
                                                         isDirectory, isItemSelected, itemIndex, dcc);
                return;
            }

            auto* fileListComp = dynamic_cast<juce::Component*>(&dcc);

            if (isItemSelected)
            {
                g.fillAll(fileListComp != nullptr ? fileListComp->findColour(juce::DirectoryContentsDisplayComponent::highlightColourId)
                                                  : findColour(juce::DirectoryContentsDisplayComponent::highlightColourId));
            }

            const auto iconColumnWidth = 32;
            const auto iconBounds = juce::Rectangle<float>(2.0f, 2.0f, iconColumnWidth - 4.0f, static_cast<float>(height) - 4.0f)
                                        .withSizeKeepingCentre(18.0f, 18.0f);

            if (isVST3Plugin)
            {
                g.setColour(juce::Colours::seagreen.withAlpha(isItemSelected ? 1.0f : 0.85f));
                g.fillRoundedRectangle(iconBounds, 3.0f);

                g.setColour(juce::Colours::white.withAlpha(0.95f));
                const auto socketY = iconBounds.getCentreY() - 3.0f;
                g.drawLine(iconBounds.getX() + 5.0f, socketY, iconBounds.getRight() - 5.0f, socketY, 1.4f);
                g.drawLine(iconBounds.getX() + 5.0f, socketY + 6.0f, iconBounds.getRight() - 5.0f, socketY + 6.0f, 1.4f);
                g.fillEllipse(iconBounds.getX() + 4.0f, socketY - 1.5f, 3.0f, 3.0f);
                g.fillEllipse(iconBounds.getRight() - 7.0f, socketY + 4.5f, 3.0f, 3.0f);
            }
            else
            {
                const auto folderBounds = iconBounds.withTrimmedTop(2.0f).withTrimmedBottom(2.0f);
                const auto tabBounds = juce::Rectangle<float>(folderBounds.getX() + 1.0f,
                                                              folderBounds.getY(),
                                                              folderBounds.getWidth() * 0.42f,
                                                              5.0f);
                const auto bodyBounds = folderBounds.withTrimmedTop(3.0f);

                g.setColour(juce::Colours::steelblue.withAlpha(isItemSelected ? 1.0f : 0.88f));
                g.fillRoundedRectangle(tabBounds, 2.0f);
                g.fillRoundedRectangle(bodyBounds, 3.0f);

                g.setColour(juce::Colours::white.withAlpha(0.28f));
                g.drawRoundedRectangle(bodyBounds.reduced(1.0f), 2.0f, 1.0f);
            }

            g.setColour(isItemSelected
                            ? (fileListComp != nullptr ? fileListComp->findColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId)
                                                       : findColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId))
                            : (fileListComp != nullptr ? fileListComp->findColour(juce::DirectoryContentsDisplayComponent::textColourId)
                                                       : findColour(juce::DirectoryContentsDisplayComponent::textColourId)));

            g.setFont(static_cast<float>(height) * 0.7f);
            g.drawFittedText(filename, iconColumnWidth, 0, width - iconColumnWidth, height, juce::Justification::centredLeft, 1);
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
    
    PluginFileBrowserLookAndFeel pluginFileBrowserLookAndFeel;
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
