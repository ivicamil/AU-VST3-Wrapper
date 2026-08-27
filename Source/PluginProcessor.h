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

class VST3WrapperAudioProcessor  : public juce::AudioProcessor, public juce::ChangeBroadcaster
{
public:
    struct HostedPluginPreset
    {
        enum class Source
        {
            Factory,
            User
        };

        juce::String name;
        juce::File file;
        juce::String relativeFolder;
        Source source = Source::Factory;
    };

    //==============================================================================
    VST3WrapperAudioProcessor();
    ~VST3WrapperAudioProcessor() override;
    
    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    bool canAddBus(bool isInput) const override;
    bool canRemoveBus(bool isInput) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<double>&,juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    //==============================================================================
    // Public interface of VST3MIDIEffectAudioProcessor
    //==============================================================================
    
    /// Returns `true` if the callee is currently trying to load the plugin asynchronously.
    bool isCurrentlyLoading();
    
    /// Returns `true` if a plugin is currently loaded and `false` otherwise
    bool isHostedPluginLoaded();
    
    void loadPlugin(const juce::String& pluginPath, const juce::PluginDescription& pluginDescription);

    void findLoadablePluginDescriptions(const juce::String& pluginPath,
                                        std::function<void(juce::Array<juce::PluginDescription>)> callback);

    /**
     * @brief This method closes currently loaded plugin (if there is one) and resets processor's state.
     *
     * @warning Make sure that the editor of the currently loaded plugin is deleted before calling this method.
     */
    void closeHostedPlugin();
    
    /// Returns an error description if `loadPlugin` fails, or an empty string otherwise.
    juce::String getHostedPluginLoadingError();
    
    /// Returns `true` if the hosted plugin has a sidechain bus, which is either the first input bus of a VST3 instruments or the second input bus of a VST3 effect.
    bool hostedPluginSupportsSidechaining();
    
    /// Returns  "Manufacturer - Name (Channel Layout Description)" string of the hosted plugin, or an empty string if no plugin is currently loaded.
    juce::String getHostedPluginName();

    /// Returns the .vstpreset files found in the standard VST3 preset folders for the currently loaded plugin.
    juce::Array<HostedPluginPreset> getHostedPluginPresets();

    /// Loads a .vstpreset file into the currently hosted VST3 plugin.
    bool loadHostedPluginPreset(const juce::File& presetFile);

    /**
     * @brief Calls `createEditorIfNeeded` on the hosted plugin instance and returns the result.
     *
     * @warning The caller must take ownership of the returned editor and delete it before calling `loadPlugin` or `closeHostedPlugin`.
     *
     * @return A pointer to `AudioProcessorEditor` of the hosted plugin or `nullptr` if no plugin is loaded. If the loaded plugin doesn't have its own editor, a `GenericAudioProcessorEditor` is returned.
     */
    juce::AudioProcessorEditor* createHostedPluginEditorIfNeeded();

private:
    juce::CriticalSection innerMutex;
    //==============================================================================
    juce::VST3PluginFormat vst3Format;
    //==============================================================================
    std::unique_ptr<juce::AudioPluginInstance> hostedPluginInstance;
    
    void setHostedPluginInstance(std::unique_ptr<juce::AudioPluginInstance> pluginInstance)
    {
        const juce::ScopedLock sl (innerMutex);
        
        hostedPluginInstance.reset();
        
        if (pluginInstance != nullptr)
        {
            hostedPluginInstance = std::move(pluginInstance);
        }
    }
    
    template <typename T>
    /// If the hosted plugin is nullptr, the method will not call provided operation and will return the default value of `T`.
    T safelyPerform(std::function<T(const std::unique_ptr<juce::AudioPluginInstance>&)> operation) const
    {
        const juce::ScopedLock sl (innerMutex);
        
        if (hostedPluginInstance == nullptr) { return T(); }
        
        return operation(hostedPluginInstance);
    }
    
    //==============================================================================
    using PluginLoadingCallback = std::function<void(std::unique_ptr<juce::AudioPluginInstance> pluginInstance)>;
    using PluginLoadOperation = std::function<void(PluginLoadingCallback callback)>;

    void removePrevioslyHostedPluginIfNeeded(bool unsetError);
    void loadPluginFromLegacyState(const juce::String& pluginPath);
    void loadPluginInternal(const juce::String& pluginPath, PluginLoadOperation loadOperation);
    juce::Array<juce::PluginDescription> findLoadablePluginDescriptionsInFile(const juce::String& pluginPath);
    juce::Array<juce::PluginDescription> findPluginDescriptionsInFile(const juce::String& pluginPath);
    juce::Array<juce::PluginDescription> filterLoadablePluginDescriptions(const juce::Array<juce::PluginDescription>& pluginDescriptions);
    void createPluginInstance(const juce::PluginDescription& pluginDescription, PluginLoadingCallback callback);
    bool setHostedPluginLayout();
    bool prepareHostedPluginForPlaying();
    void setHostedPluginState();
    juce::Array<HostedPluginPreset> findPresetsForPlugin(const juce::PluginDescription& pluginDescription) const;
    juce::StringArray getPresetManufacturerFolderNames(const juce::PluginDescription& pluginDescription) const;
    juce::StringArray getPresetPluginFolderNames(const juce::PluginDescription& pluginDescription) const;
    template<typename FloatType>
    void processBlockInternal(juce::AudioBuffer<FloatType>& buffer, juce::MidiBuffer& midiMessages, bool setPlayhead);
    //==============================================================================
    static constexpr const char* innerStateTag = "inner_state";
    static constexpr const char* pluginPathTag = "plugin_path";
    static constexpr const char* pluginDescriptionTag = "PLUGIN";
    static inline const juce::String unexpectedPluginLoadingError = "An unexpected error has occurred while loading the plugin";
    bool isLoading;
    juce::String hostedPluginLoadingError;
    juce::String hostedPluginPath;
    bool hostedPluginHasSidechainInput;
    juce::String hostedPluginName;
    juce::PluginDescription hostedPluginDescription;
    juce::String targetLayoutDescription;
    juce::MemoryBlock hostedPluginState;
    
    void setIsLoading(bool value)
    {
        const juce::ScopedLock sl(innerMutex);
        isLoading = value;
    }

    void setHostedPluginLoadingError(juce::String value)
    {
        const juce::ScopedLock sl(innerMutex);
        hostedPluginLoadingError = value;
    }
    
    void setHostedPluginPath(juce::String value)
    {
        const juce::ScopedLock sl(innerMutex);
        hostedPluginPath = value;
    }
    
    juce::String getHostedPluginPath()
    {
        const juce::ScopedLock sl(innerMutex);
        return hostedPluginPath;
    }
    
    void setHostedPluginStateMemoryBlock(juce::MemoryBlock value)
    {
        const juce::ScopedLock sl(innerMutex);
        hostedPluginState = value;
    }
    
    juce::MemoryBlock getHostedPluginStateMemoryBlock()
    {
        const juce::ScopedLock sl(innerMutex);
        return hostedPluginState;
    }

    void setHostedPluginHasSidechainInput(bool value)
    {
        const juce::ScopedLock sl(innerMutex);
        hostedPluginHasSidechainInput = value;
    }

    void setHostedPluginName(juce::String value)
    {
        const juce::ScopedLock sl(innerMutex);
        hostedPluginName = value;
    }

    void setHostedPluginDescription(juce::PluginDescription value)
    {
        const juce::ScopedLock sl(innerMutex);
        hostedPluginDescription = value;
    }
    
    //==============================================================================
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VST3WrapperAudioProcessor)
};
