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
    
    /**
     * @brief This method tries to load a VST3 plugin instance from file at provided path.
     *        A change broadcaster message is send on the main thread when the method finishes.
     *        If loading is succesful, `isHostedPluginLoaded()` will return true.
     *        If loading fails, `isHostedPluginLoaded()` will return false and `getHostedPluginLoadingError()` will contain the reason of the failure.
     *        VST3 instance is created asynchronously, but VST3 file scanning happens on the main thread as some plugins crash when scanned from a background thread. 
     *
     * @warning The method will delete previously hosted plugin, if any.
     *          Make sure that the editor of previously hosted plugin is deleted before calling this method.
     *
     * @param pluginPath The path of a VST3 file.
     */
    void loadPlugin(const juce::String& pluginPath);
    
    /**
     * @brief This method closes currently loaded plugin and resets processor's state.
     *
     * @warning Make sure that the editor of the currently loaded plugin is deleted before calling this method.
     */
    void closeHostedPlugin();
    
    /// Return an error description if `loadPlugin` returns `false`
    juce::String getHostedPluginLoadingError();
    
    /// Return `true` if the hosted plugin has a  sidechain bus which is either the first input bus for VST3 instruments or the second input bus for VST3 effects
    bool hostedPluginSupportsSidechaining();
    
    /// Returns  "Manufacturer - Name" string for the hosted plugin, or an empty string if `isHostedPluginLoaded` is `false`
    juce::String getHostedPluginName();
    /**
     * @brief Calls `createEditorIfNeeded` on the hosted plugin instance and returns the result.
     *
     * @warning The caller must take ownership of the returned editor and delete it before calling `loadPluginInstance`
     *
     * @param pluginPath The path of a VST3 file
     * @return A pointer to `AudioProcessorEditor` of the hosted plugin or `nullptr` if `isHostedPluginLoaded` is `false` or if the loaded plugin doesn't have an editor
     */
    juce::AudioProcessorEditor* createHostedPluginEditorIfNeeded();

private:
    juce::CriticalSection innerMutex;
    //==============================================================================
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList pluginList;
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
    /// If the hosted plugin is nullptr, the method will not call provided operation and will return the default value of `T` instead
    T safelyPerform(std::function<T(const std::unique_ptr<juce::AudioPluginInstance>&)> operation) const
    {
        const juce::ScopedLock sl (innerMutex);
        
        if (hostedPluginInstance == nullptr) { return T(); }
        
        return operation(hostedPluginInstance);
    }
    
    //==============================================================================
    using PluginLoadingCallback = std::function<void(std::unique_ptr<juce::AudioPluginInstance> pluginInstance)>;
    
    void removePrevioslyHostedPluginIfNeeded(bool unsetError);
    void loadPluginFromFile(const juce::String& pluginPath, PluginLoadingCallback callback);
    bool setHostedPluginLayout();
    bool prepareHostedPluginForPlaying();
    static inline const juce::String unexpectedPluginLoadingError = "Unexpected error while loading the plugin";
    //==============================================================================
    bool isLoading;
    juce::String hostedPluginLoadingError;
    bool hostedPluginHasSidechainInput;
    juce::String hostedPluginName;
    juce::String targetLayoutDescription;
    
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
    
    void setTargetLayoutDescription(juce::String value)
    {
        const juce::ScopedLock sl(innerMutex);
        targetLayoutDescription = value;
    }
    
    juce::String getTargetLayoutDescription()
    {
        const juce::ScopedLock sl(innerMutex);
        return targetLayoutDescription;
    }
    
    //==============================================================================
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VST3WrapperAudioProcessor)
};
