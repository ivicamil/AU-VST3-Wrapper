/*
 ==============================================================================
 
 Copyright 2024 Ivica Milovanovic (excluding JUCE framework code)
 
 This file is part of AU-VST3-Wrapper
 
 Foobar is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 Foobar is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 You should have received a copy of the GNU General Public License along with Foobar. If not, see <https://www.gnu.org/licenses/>.
 
 ==============================================================================
 */

#pragma once

#include <JuceHeader.h>

using namespace juce;

class VST3FileBrowserComponent : public juce::FileBrowserComponent
{
public:
    using FileBrowserComponent::FileBrowserComponent;
    
    static inline const juce::String vst3Extension = "vst3";
    
    bool isVST3FileSelected()
    {
        return getSelectedFile(0).hasFileExtension(vst3Extension);
    }
    
    void fileDoubleClicked(const File& file) override
    {
        if (file.hasFileExtension(vst3Extension))
        {
            return;
        }
        
        FileBrowserComponent::fileDoubleClicked(file);
    }
    
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::returnKey)
        {
            if (isVST3FileSelected())
            {
                return true;
            }
        }
        
        return FileBrowserComponent::keyPressed(key);
    }
};
