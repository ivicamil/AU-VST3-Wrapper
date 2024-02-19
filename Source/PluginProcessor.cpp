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
VST3WrapperAudioProcessor::VST3WrapperAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
: AudioProcessor (BusesProperties()
                  // We must have an output even for MIDI FX AU
                  // or else the hosted VST3 won't output any MIDI
                  .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
                  .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
#endif
                  .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)
                  .withOutput ("Aux 1", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 2", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 3", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 4", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 5", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 6", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 7", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 8", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 9", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 10", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 11", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 12", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 13", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 14", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 15", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 16", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 17", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 18", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 19", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 20", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 21", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 22", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 23", juce::AudioChannelSet::stereo(), false)
                  .withOutput ("Aux 24", juce::AudioChannelSet::stereo(), false)
#endif
                  )
#endif
{
}

VST3WrapperAudioProcessor::~VST3WrapperAudioProcessor()
{
}

//==============================================================================

const juce::String VST3WrapperAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

//==============================================================================
// Public API
//==============================================================================

bool VST3WrapperAudioProcessor::isHostedPluginLoaded()
{
    return safelyPerform<bool>([](auto& p) { return p != nullptr; });
}

void VST3WrapperAudioProcessor::loadPlugin(const juce::String& pluginPath)
{
    if (isCurrentlyLoading()) { return; }
    
    removePrevioslyHostedPluginIfNeeded(true);

    setIsLoading(true);
            
    auto callback = [&, pluginPath](auto pluginInstance)
    {
        if (pluginInstance == nullptr)
        {
            setIsLoading(false);
            juce::MessageManager::callAsync([&]() { sendChangeMessage(); });
            return;
        }
        
        const auto desc = pluginInstance->getPluginDescription();
        const auto pluginName = desc.manufacturerName + " - " + desc.name;
        
        setHostedPluginInstance(std::move(pluginInstance));
        
        auto successfullyConfigured = true;
        successfullyConfigured &= setHostedPluginLayout();
        successfullyConfigured &= prepareHostedPluginForPlaying();
        setHostedPluginState();
        setHostedPluginPath(pluginPath);

#if JucePlugin_IsMidiEffect
        setHostedPluginName(pluginName);
#else
        setHostedPluginName(pluginName + " (" + getTargetLayoutDescription() + ")");
#endif
        
        if (!successfullyConfigured)
        {
            removePrevioslyHostedPluginIfNeeded(false);
        }
        
        setIsLoading(false);
        
        juce::MessageManager::callAsync([&]() { sendChangeMessage(); });
    };
    
    loadPluginFromFile(pluginPath, std::move(callback));
}

bool  VST3WrapperAudioProcessor::isCurrentlyLoading()
{
    const juce::ScopedLock sl (innerMutex);
    return isLoading;
}

void VST3WrapperAudioProcessor::closeHostedPlugin()
{
    if (isCurrentlyLoading()) { return; }
    
    removePrevioslyHostedPluginIfNeeded(true);
}

juce::String VST3WrapperAudioProcessor::getHostedPluginLoadingError()
{
    const juce::ScopedLock sl (innerMutex);
    return hostedPluginLoadingError;
}

bool VST3WrapperAudioProcessor::hostedPluginSupportsSidechaining()
{
    const juce::ScopedLock sl (innerMutex);
    return hostedPluginHasSidechainInput;
}

juce::String VST3WrapperAudioProcessor::getHostedPluginName()
{
    const juce::ScopedLock sl (innerMutex);
    return hostedPluginName;
}

juce::AudioProcessorEditor* VST3WrapperAudioProcessor::createHostedPluginEditorIfNeeded()
{
    return safelyPerform<juce::AudioProcessorEditor*>([] (auto& p) { return p->createEditorIfNeeded(); });
}

//==============================================================================
// Plugin loading
//==============================================================================

void VST3WrapperAudioProcessor::removePrevioslyHostedPluginIfNeeded(bool unsetError)
{
    safelyPerform<void>([](auto& p)
    {
        // Plugin's editor must be deleted before deleting its processor
        jassert(p->getActiveEditor() == nullptr);
    });
   
    setHostedPluginInstance(nullptr);
    setHostedPluginPath("");
    setHostedPluginStateMemoryBlock(juce::MemoryBlock());
    if (unsetError) { setHostedPluginLoadingError(""); }
    setIsLoading(false);
    setTargetLayoutDescription("");
    setHostedPluginHasSidechainInput(false);
    setHostedPluginName("");
}

void VST3WrapperAudioProcessor::loadPluginFromFile(const juce::String& pluginPath, PluginLoadingCallback vst3FileLoadingCompleted)
{
    // Some plugins crash if they are scanned from a background thread
    juce::MessageManager::callAsync([=]() {
        
        juce::OwnedArray<juce::PluginDescription> descs;
        vst3Format.findAllTypesForFile(descs, pluginPath);
            
        if (descs.isEmpty())
        {
            setHostedPluginLoadingError("No valid VST3 found in selected file");
            vst3FileLoadingCompleted(nullptr);
            return;
        }
        
        auto descIndex = -1;
        
        auto validDescription = [](const juce::PluginDescription* d)
        {
    #if JucePlugin_IsMidiEffect || JucePlugin_IsSynth
            return d->isInstrument;
    #else
            return !d->isInstrument;
    #endif
        };
        
        for (int i = 0; i < descs.size(); ++i)
        {
            if (validDescription(descs[i]))
            {
                descIndex = i;
                break;
            }
        }
        
        if (descIndex == -1)
        {
    #if JucePlugin_IsMidiEffect || JucePlugin_IsSynth
            setHostedPluginLoadingError("Selected VST3 is not an instrument");
    #else
            setHostedPluginLoadingError("Selected VST3 is not an effect");
    #endif
            vst3FileLoadingCompleted(nullptr);
            return;
        }
        
        const auto pluginDescription = *descs[descIndex];
        
        auto callback = [=](auto pluginInstance, const auto& errorMessage)
        {
            
            if (pluginInstance == nullptr)
            {
                setHostedPluginLoadingError(errorMessage.isEmpty() ? unexpectedPluginLoadingError : errorMessage);
                vst3FileLoadingCompleted(nullptr);
                return;
            }
            
        #if JucePlugin_IsMidiEffect
            if (!pluginInstance->acceptsMidi())
            {
                setHostedPluginLoadingError("Selected VST3 Plugin Does Not Accept MIDI");
                vst3FileLoadingCompleted(nullptr);
                return;
            }
            
            if (!pluginInstance->producesMidi())
            {
                setHostedPluginLoadingError("Selected VST3 Plugin Does Not Produce MIDI");
                vst3FileLoadingCompleted(nullptr);
                return;
            }
        #endif
            
            vst3FileLoadingCompleted(std::move(pluginInstance));
        };
        
        vst3Format.createPluginInstanceAsync(pluginDescription, getSampleRate(), getBlockSize(), std::move(callback));
    });
}

bool VST3WrapperAudioProcessor::setHostedPluginLayout()
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    
#if JucePlugin_IsSynth
    const auto sideChainBusIndex = 0;
#else
    const auto sideChainBusIndex = 1;
#endif
    
    const auto currentLayout = getBusesLayout();
    const auto hostedPluginDefaultLayout = safelyPerform<juce::AudioProcessor::BusesLayout>([](auto& p) { return p->getBusesLayout(); });
    const auto inputBusesOfHostedPlugin = hostedPluginDefaultLayout.inputBuses;
    const auto outputBusesOfHostedPlugin = hostedPluginDefaultLayout.outputBuses;
    
    juce::AudioProcessor::BusesLayout targetLayout;
    
    for (auto i = 0; i < inputBusesOfHostedPlugin.size(); ++i)
    {
        targetLayout.inputBuses.add(i < currentLayout.inputBuses.size() ? currentLayout.inputBuses[i] : juce::AudioChannelSet::disabled());
    }
    
    for (auto i = 0; i < outputBusesOfHostedPlugin.size(); ++i)
    {
        targetLayout.outputBuses.add(i < currentLayout.outputBuses.size() ? currentLayout.outputBuses[i] : juce::AudioChannelSet::disabled());
    }
    
    juce::String layoutDescription;
    
#if JucePlugin_IsSynth
    if (targetLayout.outputBuses.size() == 1)
    {
        layoutDescription = targetLayout.getChannelSet(false, 0).getDescription();
    }
    else if (targetLayout.outputBuses.size() > 1)
    {
        layoutDescription = "Multioutput";
    }
#else
    if (targetLayout.inputBuses.size() >= 1)
    {
        layoutDescription += targetLayout.getChannelSet(true, 0).getDescription();
    }
    
    layoutDescription += "->";
    
    if (targetLayout.outputBuses.size() >= 1)
    {
        layoutDescription += targetLayout.getChannelSet(false, 0).getDescription();
    }
#endif
    
    setTargetLayoutDescription(layoutDescription);
    
    const auto layoutSuccesfullySet = safelyPerform<bool>([&](auto& p) { return p->setBusesLayout(targetLayout); });
    
    if (!layoutSuccesfullySet)
    {
        juce::String layoutNotSupportedError = "Selected plugin doesn't support current channel layout";
        setHostedPluginLoadingError(layoutNotSupportedError + " (" + layoutDescription + ")");
    }
    
    setHostedPluginHasSidechainInput(layoutSuccesfullySet && targetLayout.inputBuses.size() == sideChainBusIndex + 1);
    
    return layoutSuccesfullySet;
#endif
}

bool VST3WrapperAudioProcessor::prepareHostedPluginForPlaying()
{
    setLatencySamples(safelyPerform<int>([](auto& p) { return p->getLatencySamples(); }));
    
    safelyPerform<void>([&](auto& p)
    {
#if JucePlugin_IsMidiEffect
        p->setPlayConfigDetails(0, 2, getSampleRate(), getBlockSize());
#else
        p->setRateAndBufferSizeDetails(getSampleRate(), getBlockSize());
#endif
        p->prepareToPlay(getSampleRate(), getBlockSize());
    });
    
    return true;
}

void VST3WrapperAudioProcessor::setHostedPluginState()
{
    safelyPerform<void>([&](auto& p)
    {
        if (!getHostedPluginStateMemoryBlock().isEmpty())
        {
            p->setStateInformation (getHostedPluginStateMemoryBlock().getData(), (int) getHostedPluginStateMemoryBlock().getSize());
        }
    });
    
    setHostedPluginStateMemoryBlock(juce::MemoryBlock());
}

//==============================================================================
// AudioProcessor Methods
//==============================================================================

bool VST3WrapperAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool VST3WrapperAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool VST3WrapperAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double VST3WrapperAudioProcessor::getTailLengthSeconds() const
{
    return safelyPerform<double>([](auto& p) { return p->getTailLengthSeconds(); });
}

int VST3WrapperAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
}

int VST3WrapperAudioProcessor::getCurrentProgram()
{
    return 0;
}

void VST3WrapperAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String VST3WrapperAudioProcessor::getProgramName (int index)
{
    return {};
}

void VST3WrapperAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

int preparedCount;

void VST3WrapperAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    safelyPerform<void>([&](auto& p)
    {
#if JucePlugin_IsMidiEffect
        p->setPlayConfigDetails(0, 2, sampleRate, samplesPerBlock);
#else
        p->setRateAndBufferSizeDetails(sampleRate, samplesPerBlock);
#endif
        p->prepareToPlay(sampleRate, samplesPerBlock);
    });
}

void VST3WrapperAudioProcessor::reset()
{
    safelyPerform<void>([&](auto& p)
    {
        p->reset();
    });
}

void VST3WrapperAudioProcessor::releaseResources()
{
    safelyPerform<void>([&](auto& p)
    {
        p->releaseResources();
    });
}

#ifndef JucePlugin_PreferredChannelConfigurations

bool VST3WrapperAudioProcessor::canAddBus(bool isInput) const
{
    return true;
}

bool VST3WrapperAudioProcessor::canRemoveBus(bool isInput) const
{
    return true;
}

bool VST3WrapperAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    // As we want to host any VST3, we allow Logic to instantiate us
    // with any layout it supports, and then we check if the hosted plugin supports
    // current layout after it gets loaded
    
    // Logic doesn't always show all the possible layout options supported in this method.
    // If some of the layout options are missing in Logic, try increasing plugin version
    // or changing manufacturer code in the Projucer project, and rebuild the plugin after that
    return true;
#endif
}
#endif

void VST3WrapperAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    safelyPerform<void>([&](auto& p)
    {
        p->setPlayHead(getPlayHead());
        p->processBlock(buffer, midiMessages);
    });
}

void VST3WrapperAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    safelyPerform<void>([&](auto& p)
    {
        p->processBlockBypassed(buffer, midiMessages);
    });
}

bool VST3WrapperAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* VST3WrapperAudioProcessor::createEditor()
{
    return new VST3WrapperAudioProcessorEditor (*this);
}

void VST3WrapperAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    safelyPerform<void>([&](auto& p)
    {
        XmlElement xml ("state");
        
        auto filePathElement = std::make_unique<XmlElement> (pluginPathTag);
        filePathElement->addTextElement (getHostedPluginPath());
        xml.addChildElement (filePathElement.release());
        
        xml.addChildElement ([&]
        {
            MemoryBlock innerState;
            p->getStateInformation (innerState);
            auto stateNode = std::make_unique<XmlElement> (innerStateTag);
            stateNode->addTextElement (innerState.toBase64Encoding());
            return stateNode.release();
        }());
    
        const auto text = xml.toString();
        destData.replaceAll (text.toRawUTF8(), text.getNumBytesAsUTF8());
    });
}

void VST3WrapperAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const ScopedLock sl (innerMutex);
    
    auto xml = XmlDocument::parse (String (CharPointer_UTF8 (static_cast<const char*> (data)), (size_t) sizeInBytes));

    if (auto* pluginPathNode = xml->getChildByName (pluginPathTag))
    {
        auto pluginPath = pluginPathNode->getAllSubText();

        MemoryBlock innerState;
        innerState.fromBase64Encoding (xml->getChildElementAllSubText (innerStateTag, {}));
        hostedPluginState = innerState;
        loadPlugin(pluginPath);
    }
}

// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VST3WrapperAudioProcessor();
}
