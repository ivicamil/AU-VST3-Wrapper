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

using namespace juce;

class VST3FileBrowserComponent : public juce::FileBrowserComponent
{
public:
    using FileBrowserComponent::FileBrowserComponent;
    
    static inline const juce::String vst3Extension = "vst3";

    void setSearchText(const juce::String& newSearchText)
    {
        const auto newNormalisedSearchText = newSearchText.trim().toLowerCase();

        if (searchText == newNormalisedSearchText)
        {
            return;
        }

        const auto shouldRefresh = visibleEntriesWouldChange(newNormalisedSearchText);
        searchText = newNormalisedSearchText;

        if (!shouldRefresh)
        {
            return;
        }

        deselectAllFiles();
        refresh();
    }
    
    bool isVST3FileSelected()
    {
        return getSelectedFile(0).hasFileExtension(vst3Extension);
    }

    bool isFileSuitable(const juce::File& file) const override
    {
        return FileBrowserComponent::isFileSuitable(file) && matchesSearchText(file);
    }

    bool isDirectorySuitable(const juce::File& directory) const override
    {
        return !directory.hasFileExtension(vst3Extension) || matchesSearchText(directory);
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
        return key == juce::KeyPress::returnKey && isVST3FileSelected() ? true :  FileBrowserComponent::keyPressed(key);
    }

private:
    bool matchesSearchText(const juce::File& file) const
    {
        return searchText.isEmpty()
            || file.getFileNameWithoutExtension().toLowerCase().contains(searchText);
    }

    bool matchesSearchText(const juce::File& file, const juce::String& text) const
    {
        return text.isEmpty()
            || file.getFileNameWithoutExtension().toLowerCase().contains(text);
    }

    bool isVisibleWithSearchText(const juce::File& file, const juce::String& text) const
    {
        if (file.hasFileExtension(vst3Extension))
        {
            return matchesSearchText(file, text);
        }

        if (file.isDirectory())
        {
            return true;
        }

        return FileBrowserComponent::isFileSuitable(file) && matchesSearchText(file, text);
    }

    bool visibleEntriesWouldChange(const juce::String& newSearchText) const
    {
        for (const auto& entry : juce::RangedDirectoryIterator(getRoot(),
                                                               false,
                                                               "*",
                                                               juce::File::findFiles | juce::File::findDirectories))
        {
            const auto file = entry.getFile();

            if (isVisibleWithSearchText(file, searchText) != isVisibleWithSearchText(file, newSearchText))
            {
                return true;
            }
        }

        return false;
    }

    juce::String searchText;
};
