/*
 ==============================================================================
 
 Copyright 2024 Ivica Milovanovic (excluding JUCE framework code)
 
 This file is part of AU-VST3-Wrapper
 
 AU-VST3-Wrapper is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 AU-VST3-Wrapper is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 You should have received a copy of the GNU General Public License along with AU-VST3-Wrapper. If not, see <https://www.gnu.org/licenses/>.
 
 ==============================================================================
 */


#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================

VST3WrapperAudioProcessorEditor::VST3WrapperAudioProcessorEditor (VST3WrapperAudioProcessor& p)
: AudioProcessorEditor (&p), audioProcessor (p)
{
    pluginFileBrowser.reset(new VST3FileBrowserComponent(juce::FileBrowserComponent::openMode
                                               | juce::FileBrowserComponent::canSelectDirectories
                                               | juce::FileBrowserComponent::filenameBoxIsReadOnly,
                                               juce::File("/Library/Audio/Plug-Ins/VST3"),
                                               nullptr,
                                               nullptr));
    
    audioProcessor.addChangeListener(this);
    
    addAndMakeVisible(pluginFileBrowser.get());
    pluginFileBrowser->addListener(this);
    // setEnabled(false) doesn't work on FileBrowserComponent,
    // so we use a cover view to disable it when needed
    addAndMakeVisible(pluginFileBrowserCover);
    
    loadPluginButton.setButtonText("Load Plugin");
    loadPluginButton.addListener(this);
    
    closePluginButton.setButtonText("Close Plugin");
    closePluginButton.addListener(this);
    
    statusLabel.setJustificationType(juce::Justification::centred);
    
    addAndMakeVisible(loadPluginButton);
    addAndMakeVisible(closePluginButton);
    addAndMakeVisible(statusLabel);

    setHostedPluginEditorIfNeeded();
    
    processorStateChanged(false);
    
    // A workaround for some AUs like Korg Triton,
    // that loose focus when their editor is reloaded
    // (see the callback for details)
    startTimer(500);
}

VST3WrapperAudioProcessorEditor::~VST3WrapperAudioProcessorEditor()
{
    audioProcessor.removeChangeListener (this);
    stopTimer();
}

//==============================================================================

void VST3WrapperAudioProcessorEditor::loadPlugin(const juce::String& filePath)
{
    // If present, the old hosted plugin editor is now deleted and set to nullptr so that
    // the main audio processor can safely delete its processor
    hostedPluginEditor.reset();
    setLoadingState();
    
    threadPool.addJob([this, filePath]()
    {
        // Give UI some time to update
        juce::Thread::sleep(5);
        audioProcessor.loadPlugin(filePath);
    });
}

void VST3WrapperAudioProcessorEditor::closePlugin()
{
    // If present, the old hosted plugin editor is now deleted and set to nullptr so that
    // the main audio processor can safely delete its processor
    hostedPluginEditor.reset();
    audioProcessor.closeHostedPlugin();
    processorStateChanged(false);
}

void VST3WrapperAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    setHostedPluginEditorIfNeeded();
    processorStateChanged(true);
}

void VST3WrapperAudioProcessorEditor::setHostedPluginEditorIfNeeded()
{
    if (!audioProcessor.isHostedPluginLoaded()) { return; }
    
    const auto newEditor = audioProcessor.createHostedPluginEditorIfNeeded();
    
    if (newEditor == nullptr) { return; }
    
    // Just in case setHostedPluginEditorIfNeeded is called more than once
    // during the lifetime of the hosted plugin
    if (hostedPluginEditor.get() != newEditor)
    {
        hostedPluginEditor.reset(newEditor);
        addAndMakeVisible(hostedPluginEditor.get());
        hostedPluginEditor.get()->addComponentListener(this);
    }
}

void VST3WrapperAudioProcessorEditor::setLoadingState()
{
    loadPluginButton.setEnabled(false);
    pluginFileBrowserCover.setVisible(true);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    statusLabel.setText("Loading...", juce::dontSendNotification);
}

void VST3WrapperAudioProcessorEditor::processorStateChanged(const bool shouldShowPluginLoadingError)
{
    const auto isHostedPluginLoaded = audioProcessor.isHostedPluginLoaded();
    const auto pluginLoadingError = audioProcessor.getHostedPluginLoadingError();
    
    pluginFileBrowser.get()->setVisible(!isHostedPluginLoaded);
    pluginFileBrowserCover.setVisible(false);
    loadPluginButton.setVisible(!isHostedPluginLoaded);
    loadPluginButton.setEnabled(pluginFileBrowser->isVST3FileSelected());
    closePluginButton.setVisible(isHostedPluginLoaded);
    
    if (isHostedPluginLoaded)
    {
        juce::String labelText = audioProcessor.getHostedPluginName();
        
        // This should never happen as `hostedPluginEditor` 
        // should at least be `GenericAudioProcessorEditor`, but just in case...
        if (hostedPluginEditor == nullptr) { labelText += " (no editor)"; }
        
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        statusLabel.setText(labelText, juce::dontSendNotification);
    }
    else
    {
        const auto isShowingError = shouldShowPluginLoadingError && !pluginLoadingError.isEmpty();
        const auto labelText = isShowingError ? pluginLoadingError : noPluginLoadedMessage;
        statusLabel.setColour(juce::Label::textColourId, isShowingError ? juce::Colours::red : juce::Colours::white);
        statusLabel.setText(labelText, juce::dontSendNotification);
    }
            
    setSize(getEditorWidth(), getEditorHeight());
    repaint();
}

//==============================================================================
//
//==============================================================================

void VST3WrapperAudioProcessorEditor::selectionChanged()
{
    loadPluginButton.setEnabled(pluginFileBrowser->isVST3FileSelected());
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    statusLabel.setText(noPluginLoadedMessage, juce::dontSendNotification);
}

void VST3WrapperAudioProcessorEditor::fileClicked (const juce::File& file, const juce::MouseEvent& e)
{
    
}

void VST3WrapperAudioProcessorEditor::fileDoubleClicked (const juce::File& file)
{
    
}

void VST3WrapperAudioProcessorEditor::browserRootChanged (const juce::File& newRoot)
{
    
}

//==============================================================================

void VST3WrapperAudioProcessorEditor::buttonClicked(juce::Button* button)
{
    if (button == &loadPluginButton)
    {
        const auto file = pluginFileBrowser->getSelectedFile(0);
        
        if (file.exists() && file.hasFileExtension(VST3FileBrowserComponent::vst3Extension))
        {
            loadPlugin(file.getFullPathName());
        }
    }
    else if (button == &closePluginButton)
    {
        closePlugin();
    }
}

void VST3WrapperAudioProcessorEditor::drawSidechainArrow(juce::Graphics& g)
{
    float arrowHeight = 10.0f;
    float arrowLength = arrowHeight * sqrt(2)/2;
    float editorWidth = getWidth();
    float editorHeight = getHeight();
    float arrowLowestY = editorHeight;
    
    const auto p1 = juce::Point<float>(editorWidth, arrowLowestY);
    const auto p2 = juce::Point<float>(editorWidth,  arrowLowestY - arrowHeight);
    const auto p3 = juce::Point<float>(editorWidth - arrowLength, arrowLowestY - arrowHeight/2);
    
    juce::Path triangle;
    triangle.startNewSubPath(p1);
    triangle.lineTo(p2);
    triangle.lineTo(p3);
    triangle.closeSubPath();

    g.setColour(juce::Colours::green);
    g.fillPath(triangle);
}

void VST3WrapperAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    
    if (audioProcessor.hostedPluginSupportsSidechaining())
    {
        drawSidechainArrow(g);
    }
}

void VST3WrapperAudioProcessorEditor::componentMovedOrResized(Component &component, bool wasMoved, bool wasResized)
{
    if (&component == hostedPluginEditor.get() && wasResized) 
    {
        setSize(getEditorWidth(), getEditorHeight());
    }
}

void VST3WrapperAudioProcessorEditor::resized()
{
    if (hostedPluginEditor != nullptr)
    {
        hostedPluginEditor->setTopLeftPosition(0, 0);
    }
   
    pluginFileBrowser->setBounds(0, 0, getEditorWidth(), browserHeight);
    pluginFileBrowserCover.setBounds(0, 0, getEditorWidth(), browserHeight);
    loadPluginButton.setBounds(margin, getButtonOriginY(), getBounds().getWidth() - 2 * margin, buttonHeight);
    closePluginButton.setBounds(margin, getButtonOriginY(), getBounds().getWidth() - 2 * margin, buttonHeight);
    statusLabel.setBounds(margin, getLabelriginY(), getBounds().getWidth() - 2 * margin, labelHeight);
}

void VST3WrapperAudioProcessorEditor::timerCallback()
{
    // A workaround for some AUs like Korg Triton, that loose focus when their editor is reloaded
    // This happens in Logic on Apple Silicon, not under Rosetta.
    // Focus can be regained, if the user clicks outside affected plugin (e.g. the status label).
    // We focus the editor after a short delay, so that the user doesn't have to do it manually.
    setWantsKeyboardFocus(true);
    grabKeyboardFocus();
    stopTimer();
}
