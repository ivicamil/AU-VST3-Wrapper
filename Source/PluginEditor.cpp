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

#include <map>

namespace
{
    using HostedPluginPreset = VST3WrapperAudioProcessor::HostedPluginPreset;
    using PresetSource = HostedPluginPreset::Source;

    juce::String normalisePresetMenuPath(juce::String path)
    {
        return path.replaceCharacter('\\', '/').trimCharactersAtStart("/").trimCharactersAtEnd("/");
    }

    bool presetHasSource(const HostedPluginPreset& preset, PresetSource source)
    {
        return preset.source == source;
    }

    bool hasPresetsForSource(const juce::Array<HostedPluginPreset>& presets, PresetSource source)
    {
        for (const auto& preset : presets)
            if (presetHasSource(preset, source))
                return true;

        return false;
    }

    juce::String getImmediateChildFolder(const juce::String& presetFolder, const juce::String& currentFolder)
    {
        const auto folder = normalisePresetMenuPath(presetFolder);
        const auto current = normalisePresetMenuPath(currentFolder);

        if (folder.isEmpty() || folder == current)
            return {};

        if (current.isNotEmpty())
        {
            const auto currentPrefix = current + "/";

            if (!folder.startsWith(currentPrefix))
                return {};

            const auto remainder = folder.substring(currentPrefix.length());
            return remainder.upToFirstOccurrenceOf("/", false, false);
        }

        return folder.upToFirstOccurrenceOf("/", false, false);
    }

    void addPresetsToMenu(juce::PopupMenu& menu,
                          const juce::Array<HostedPluginPreset>& presets,
                          PresetSource source,
                          const juce::String& currentFolder,
                          int& nextMenuItemId,
                          std::map<int, HostedPluginPreset>& presetByMenuItemId)
    {
        juce::StringArray childFolders;

        for (int i = 0; i < presets.size(); ++i)
        {
            const auto& preset = presets.getReference(i);

            if (!presetHasSource(preset, source))
                continue;

            const auto childFolder = getImmediateChildFolder(preset.relativeFolder, currentFolder);

            if (childFolder.isNotEmpty())
                childFolders.addIfNotAlreadyThere(childFolder);
        }

        childFolders.sort(true);

        for (const auto& childFolder : childFolders)
        {
            juce::PopupMenu submenu;
            const auto submenuFolder = normalisePresetMenuPath(currentFolder).isEmpty()
                                           ? childFolder
                                           : normalisePresetMenuPath(currentFolder) + "/" + childFolder;

            addPresetsToMenu(submenu, presets, source, submenuFolder, nextMenuItemId, presetByMenuItemId);
            menu.addSubMenu(childFolder, submenu);
        }

        for (int i = 0; i < presets.size(); ++i)
        {
            const auto& preset = presets.getReference(i);

            if (!presetHasSource(preset, source) || normalisePresetMenuPath(preset.relativeFolder) != normalisePresetMenuPath(currentFolder))
                continue;

            const auto menuItemId = nextMenuItemId++;
            presetByMenuItemId[menuItemId] = preset;
            menu.addItem(menuItemId, preset.name);
        }
    }
}

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
    pluginFileBrowser->setLookAndFeel(&pluginFileBrowserLookAndFeel);

    pluginSearchBox.setTextToShowWhenEmpty("Search VST3 plugins", juce::Colours::grey);
    pluginSearchBox.onTextChange = [this]()
    {
        pluginFileBrowser->setSearchText(pluginSearchBox.getText());
        loadPluginButton.setEnabled(pluginFileBrowser->isVST3FileSelected());

        const auto shouldShowClearSearchButton = pluginSearchBox.getText().isNotEmpty();

        if (clearSearchButton.isVisible() != shouldShowClearSearchButton)
        {
            clearSearchButton.setVisible(shouldShowClearSearchButton);
            resized();
        }
    };

    clearSearchButton.setVisible(false);
    clearSearchButton.onClick = [this]()
    {
        pluginSearchBox.clear();
        pluginFileBrowser->setSearchText({});
        loadPluginButton.setEnabled(pluginFileBrowser->isVST3FileSelected());
        clearSearchButton.setVisible(false);
        resized();
    };

    presetMenuButton.setButtonText("Presets");
    presetMenuButton.addListener(this);
    
    audioProcessor.addChangeListener(this);
    
    addAndMakeVisible(pluginSearchBox);
    addAndMakeVisible(clearSearchButton);
    addChildComponent(presetMenuButton);
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
    pluginFileBrowser->setLookAndFeel(nullptr);
    audioProcessor.removeChangeListener (this);
    stopTimer();
}

//==============================================================================

void VST3WrapperAudioProcessorEditor::startLoadingPlugin(std::function<void()> loadOperation)
{
    // If present, the old hosted plugin editor is now deleted and set to nullptr so that
    // the main audio processor can safely delete its processor
    hostedPluginEditor.reset();
    setLoadingState();

    threadPool.addJob([loadOperation = std::move(loadOperation)]()
    {
        // Give UI some time to update
        juce::Thread::sleep(5);
        loadOperation();
    });
}

void VST3WrapperAudioProcessorEditor::loadPlugin(const juce::String& filePath, const juce::PluginDescription& pluginDescription)
{
    startLoadingPlugin([this, filePath, pluginDescription]()
    {
        audioProcessor.loadPlugin(filePath, pluginDescription);
    });
}

void VST3WrapperAudioProcessorEditor::loadSelectedPluginFile()
{
    const auto file = pluginFileBrowser->getSelectedFile(0);

    if (!file.exists() || !file.hasFileExtension(VST3FileBrowserComponent::vst3Extension))
    {
        return;
    }

    const auto filePath = file.getFullPathName();
    juce::Component::SafePointer<VST3WrapperAudioProcessorEditor> editor(this);

    hostedPluginEditor.reset();
    setLoadingState();

    audioProcessor.findLoadablePluginDescriptions(filePath, [editor, filePath](const auto pluginDescriptions)
    {
        if (editor == nullptr)
        {
            return;
        }

        if (pluginDescriptions.isEmpty())
        {
            editor->processorStateChanged(true);
            return;
        }

        if (pluginDescriptions.size() == 1)
        {
            editor->loadPlugin(filePath, pluginDescriptions.getReference(0));
            return;
        }

        editor->showPluginSelectionDialog(filePath, pluginDescriptions);
    });
}

void VST3WrapperAudioProcessorEditor::showPluginSelectionDialog(const juce::String& filePath, const juce::Array<juce::PluginDescription>& pluginDescriptions)
{
    pluginSelectionDialog = std::make_unique<juce::AlertWindow>("Select plugin",
                                                                "This VST3 contains multiple plugins. Select the one you want to load.",
                                                                juce::MessageBoxIconType::NoIcon,
                                                                this);
    pluginSelectionDialog->setAlwaysOnTop(true);

    juce::StringArray pluginNames;

    for (const auto& desc : pluginDescriptions)
    {
        auto pluginName = desc.manufacturerName.isEmpty() ? desc.name : desc.manufacturerName + " - " + desc.name;

        if (!desc.category.isEmpty())
            pluginName += " (" + desc.category + ")";

        pluginNames.add(pluginName);
    }

    pluginSelectionDialog->addComboBox(pluginSelectionComboBoxName, pluginNames);

    if (auto* comboBox = pluginSelectionDialog->getComboBoxComponent(pluginSelectionComboBoxName))
    {
        comboBox->setSelectedId(1, juce::dontSendNotification);
    }

    pluginSelectionDialog->addButton("Load", 1, juce::KeyPress(juce::KeyPress::returnKey));
    pluginSelectionDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    addAndMakeVisible(pluginSelectionDialog.get());
    pluginSelectionDialog->setCentrePosition(getLocalBounds().getCentre());
    pluginSelectionDialog->toFront(true);

    juce::Component::SafePointer<VST3WrapperAudioProcessorEditor> editor(this);

    pluginSelectionDialog->enterModalState(true, juce::ModalCallbackFunction::create([editor, filePath, pluginDescriptions](const int result)
    {
        if (editor == nullptr)
        {
            return;
        }

        if (result != 1)
        {
            editor->pluginSelectionDialog.reset();
            editor->processorStateChanged(false);
            return;
        }

        auto selectedDescriptionIndex = 0;

        if (auto* comboBox = editor->pluginSelectionDialog->getComboBoxComponent(pluginSelectionComboBoxName))
            selectedDescriptionIndex = comboBox->getSelectedId() - 1;

        editor->pluginSelectionDialog.reset();

        if (!juce::isPositiveAndBelow(selectedDescriptionIndex, pluginDescriptions.size()))
        {
            return;
        }

        editor->loadPlugin(filePath, pluginDescriptions.getReference(selectedDescriptionIndex));
    }), false);
}

void VST3WrapperAudioProcessorEditor::closePlugin()
{
    // If present, the old hosted plugin editor is now deleted and set to nullptr so that
    // the main audio processor can safely delete its processor
    hostedPluginEditor.reset();
    audioProcessor.closeHostedPlugin();
    processorStateChanged(false);
}

void VST3WrapperAudioProcessorEditor::updatePresetList()
{
    hostedPluginPresets.clear();
    presetMenuButton.setButtonText("Presets");

    if (audioProcessor.isHostedPluginLoaded())
        hostedPluginPresets = audioProcessor.getHostedPluginPresets();

    const auto shouldShowPresetMenuButton = hostedPluginPresets.size() > 0;
    presetMenuButton.setVisible(shouldShowPresetMenuButton);
    presetMenuButton.setEnabled(shouldShowPresetMenuButton);
}

void VST3WrapperAudioProcessorEditor::showPresetMenu()
{
    if (hostedPluginPresets.isEmpty())
        return;

    juce::PopupMenu menu;
    std::map<int, HostedPluginPreset> presetByMenuItemId;
    auto nextMenuItemId = 1;

    if (hasPresetsForSource(hostedPluginPresets, HostedPluginPreset::Source::Factory))
    {
        menu.addSectionHeader("Factory Presets");
        addPresetsToMenu(menu, hostedPluginPresets, HostedPluginPreset::Source::Factory, {}, nextMenuItemId, presetByMenuItemId);
    }

    if (hasPresetsForSource(hostedPluginPresets, HostedPluginPreset::Source::User))
    {
        if (hasPresetsForSource(hostedPluginPresets, HostedPluginPreset::Source::Factory))
            menu.addSeparator();

        menu.addSectionHeader("User Presets");
        addPresetsToMenu(menu, hostedPluginPresets, HostedPluginPreset::Source::User, {}, nextMenuItemId, presetByMenuItemId);
    }

    juce::Component::SafePointer<VST3WrapperAudioProcessorEditor> editor(this);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetMenuButton),
                       [editor, presetByMenuItemId](const int selectedMenuItemId)
                       {
                           if (editor == nullptr || selectedMenuItemId == 0)
                               return;

                           const auto preset = presetByMenuItemId.find(selectedMenuItemId);

                           if (preset == presetByMenuItemId.end())
                               return;

                           editor->loadPreset(preset->second);
                       });
}

void VST3WrapperAudioProcessorEditor::loadPreset(const HostedPluginPreset& preset)
{
    if (!preset.file.existsAsFile())
        return;

    if (!audioProcessor.loadHostedPluginPreset(preset.file))
    {
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
        statusLabel.setText("Could not load preset", juce::dontSendNotification);
        return;
    }

    presetMenuButton.setButtonText(preset.name);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    statusLabel.setText(audioProcessor.getHostedPluginName(), juce::dontSendNotification);
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
    pluginSearchBox.setEnabled(false);
    pluginFileBrowserCover.setVisible(true);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    statusLabel.setText("Loading...", juce::dontSendNotification);
}

void VST3WrapperAudioProcessorEditor::processorStateChanged(const bool shouldShowPluginLoadingError)
{
    const auto isHostedPluginLoaded = audioProcessor.isHostedPluginLoaded();
    const auto pluginLoadingError = audioProcessor.getHostedPluginLoadingError();

    updatePresetList();
    
    pluginSearchBox.setVisible(!isHostedPluginLoaded);
    pluginSearchBox.setEnabled(!isHostedPluginLoaded);
    clearSearchButton.setVisible(!isHostedPluginLoaded && pluginSearchBox.getText().isNotEmpty());
    clearSearchButton.setEnabled(!isHostedPluginLoaded);
    presetMenuButton.setVisible(isHostedPluginLoaded && hostedPluginPresets.size() > 0);
    presetMenuButton.setEnabled(isHostedPluginLoaded && hostedPluginPresets.size() > 0);
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
        loadSelectedPluginFile();
    }
    else if (button == &closePluginButton)
    {
        closePlugin();
    }
    else if (button == &presetMenuButton)
    {
        showPresetMenu();
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
        hostedPluginEditor->setTopLeftPosition(0, getPresetBarHeight());
    }
   
    const auto shouldShowClearSearchButton = clearSearchButton.isVisible();
    const auto searchBoxWidth = getEditorWidth() - 2 * margin
        - (shouldShowClearSearchButton ? clearSearchButtonWidth + searchBoxSpacing : 0);

    pluginSearchBox.setBounds(margin, margin, searchBoxWidth, searchBoxHeight);
    clearSearchButton.setBounds(getEditorWidth() - margin - clearSearchButtonWidth, margin, clearSearchButtonWidth, searchBoxHeight);
    pluginFileBrowser->setBounds(0, getFileBrowserOriginY(), getEditorWidth(), getFileBrowserHeight());
    pluginFileBrowserCover.setBounds(0, 0, getEditorWidth(), browserHeight);
    presetMenuButton.setBounds(margin,
                               (presetBarHeight - presetButtonHeight) / 2,
                               getEditorWidth() - 2 * margin,
                               presetButtonHeight);
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
