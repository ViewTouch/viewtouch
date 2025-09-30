//
// INI file IO library
// Derived from Gary McNickle's ConfFile code
//
// adapted by Brian Kowolowski, 20060126
//

#include <fstream>
#include <stdarg.h>
#include <stdexcept>
#include <cassert>

#include "conf_file.hh"

#include <iostream> // temp
#include <sstream> // stringstream
#include <string.h>
#include <algorithm> // find_if
#include <locale> // always use "C" locale, ini file needs to be language
                  // independent
#include <iterator> // std::advance

namespace
{
// Utility Functions ////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

// Trim
// Trims whitespace from both sides of a string.
// algorithm from g++ manual
void Trim(std::string& str)
{
    static const std::string trimChars = WhiteSpace + EqualIndicators;

    // trim left
    const auto first = str.find_first_not_of(trimChars);
    if (first == std::string::npos) {
        str.clear();
        return;
    }
    str.erase(0, first);

    // trim right
    const auto last = str.find_last_not_of(trimChars);
    if (last != std::string::npos) {
        str.erase(last + 1);
    }
}
} // end utility namespace

// init, attempt to load file
ConfFile::ConfFile(const std::string &name, const bool load) :
    fileName(name),
    dirty(false)
{
    if (load)
    {
        if (!Load())
        {
            throw std::runtime_error(
                        std::string("ConfFile: error loading file: ") + name);
        }
    }
}

ConfFile::~ConfFile()
{
    if (dirty) {
        Save();
    }
    dirty = false;
}

void ConfFile::set_dirty(const bool value) noexcept
{
    dirty = value;
}

// Attempts to load in the text file. If successful it will populate the
// Section list with the key/value pairs found in the file.
bool ConfFile::Load()
{
    // don't create new files when attempting to read a non-existent one
    //fstream File(fileName.c_str(), ios::in|ios::nocreate);
    std::ifstream File(fileName);

    if (! File.is_open()) {
        return false;
    }

    bool done = false;

    section_names.clear();
    section_names.push_back("");

    data.clear();
    data.push_back({});
    
    std::string line;
    std::string section_name = "";

    while (std::getline(File, line)) {
        Trim(line);
        done = false; // Reset done flag since we're using getline differently

        // throw out comments
        if (line.find_first_of(CommentIndicators) ==
            line.find_first_not_of(WhiteSpace)) {
            continue;
        }

        if (!line.empty() && line[0] == '[') {
            line.erase(0, 1);
            // erase to eol - check for valid closing bracket
            const auto closing_bracket = line.find_first_of(']');
            if (closing_bracket != std::string::npos) {
                line.erase(closing_bracket);
                section_name = line;
                CreateSection(section_name);
            }
        } else if (!line.empty()) {
            std::string value;
            const auto split = line.find_first_of(EqualIndicators);

            if (split != std::string::npos && split > 0) {
                // copy, erase identifier
                value = line;
                value.erase(0, split + 1); // +1 to skip the equal sign

                // erase value part from line
                line.erase(split);

                Trim(value);
            }
            // trim "identifier" portion; no value associated
            Trim(line);

            if (!line.empty()) {
                SetValue(value, line, section_name);
            }
        }
    }

    // Check for any stream errors after the loop
    if (File.bad()) {
        return false;
    }

    File.close();

    return true;
}

// attempt to save a data file from this config structure.
bool ConfFile::Save()
{
    if ((KeyCount() == 0) && (SectionCount() == 0)) {
        // nothing to save
        return false; 
    }

    // overwrite old config file if it exists
    std::ofstream File(fileName, std::ios::out | std::ios::trunc);

    if (! File.is_open()) {
        std::cerr << "ConfFile::Save:  file open failure (" << fileName << ")" << std::endl;
        return false;
    }

    assert(section_names.size() == data.size());
    for (size_t i = 0; i < section_names.size(); ++i)
    {
        assert(i < data.size());
        const auto& section_name = section_names[i];
        const auto& entries = data[i];

        if (!section_name.empty())
        {
            File << "\n[" << section_name << "]" << std::endl;
        }

        for (const auto& elem : entries)
        {
            const auto& key = elem.first;
            const auto& value = elem.second;

            assert(!key.empty());

            File << key
                 << EqualIndicators[0]
                 << value
                 << std::endl;
        }
    }
        
    dirty = false;
    
    return true;
}

// set the given value.  if key doesn't exist, create it.
bool ConfFile::SetValue(const std::string &value, const std::string &keyName, const std::string &sectName)
{
    if (value.empty())
    {
        return false;
    }
    if (keyName.empty())
    {
        return false;
    }
    if (!this->contains(sectName))
    {
        // try to create new section
        if (! CreateSection(sectName))
        {
            return false;
        }
    }

    try {
        // get section, throws out_of_range if no section is available
        SectionEntries &section = this->at(sectName);

        for (auto& entry : section)
        {
            const auto& entry_key = entry.first;
            auto& entry_value = entry.second;
            if (entry_key == keyName)
            {
                entry_value = value;
                dirty = true;
                return true;
            }
        }

        // if the key does not exist in that section, and the value passed
        // is not "" then add the new key.
        dirty = true;
        section.push_back(std::make_pair(keyName, value));
        return true;

    } catch (std::out_of_range &) {
        // this should not happen
        return false;
    }
}

// set the given value.  if key doesn't exist, create it.
bool ConfFile::SetValue(const double value, const std::string &key, const std::string &section)
{
    std::ostringstream ss;
    ss.imbue(std::locale::classic()); // use "C" locale
    ss << value;
    return SetValue(ss.str(), key, section);
}

// set the given value.  if key doesn't exist, create it.
bool ConfFile::SetValue(const int value, const std::string &key, const std::string &section)
{
    std::ostringstream ss;
    ss.imbue(std::locale::classic()); // use "C" locale
    ss << value;
    return SetValue(ss.str(), key, section);
}

// get value for named key in named section.  if return value is false,
//  key not found, contents of value is undefined.
bool ConfFile::GetValue(std::string &value, const std::string &keyName, const std::string &sectName) const
{
    try {
        // get section, throws out_of_range if no section is available
        const SectionEntries &section = this->at(sectName);

        for (const auto& entry : section)
        {
            const auto& entry_key = entry.first;
            const auto& entry_value = entry.second;
            if (entry_key == keyName)
            {
                value = entry_value;
                return true;
            }
        }

        return false;

    } catch (std::out_of_range &) {
        // section not found
        return false;
    }
}

bool ConfFile::GetValue(double &value, const std::string &key, const std::string &section) const
{
    std::string val;

    if (! GetValue(val, key, section))
        return false;

    if (val.empty())
        return false;

    std::istringstream ss(val);
    ss.imbue(std::locale::classic()); // use "C" locale
    ss >> value;
    if (ss.fail())
    {
        try
        {
            // handle 'inf' and '-inf' values
            value = stod(val);
        } catch (const std::invalid_argument &)
        {
            // can't convert, return false
            return false;
        }
    }
    return true;
}

bool ConfFile::GetValue(int &value, const std::string &key, const std::string &section) const
{
    std::string val;

    if (! GetValue(val, key, section))
        return false;

    if (val.empty())
        return false;

    std::istringstream ss(val);
    ss.imbue(std::locale::classic()); // use "C" locale
    ss >> value;
    if (ss.fail()) // indicate a parser error
        return false;
    return true;
}

// delete a section -- "true" ==> deletion successful
bool ConfFile::DeleteSection(const std::string &section)
{
    if (section.empty())
    {
        // don't delete the default section
        return false;
    }
    for (size_t i = 0; i < section_names.size(); ++i)
    {
        const auto& section_name = section_names[i];
        // compare case insensitive
        if (strcasecmp(section_name.c_str(), section.c_str()) == 0)
        {
            assert(i < data.size());
            auto data_it = std::next(data.begin(), static_cast<ptrdiff_t>(i));
            auto sect_it = std::next(section_names.begin(), static_cast<ptrdiff_t>(i));
            data.erase(data_it);
            section_names.erase(sect_it);
            dirty = true;
            return true;
        }
    }
    return false;
}

// delete a key in a section -- "true" ==> deletion successful
bool ConfFile::DeleteKey(const std::string &key, const std::string &sectName)
{
    if (key.empty())
    {
        // no empty key allowed
        return false;
    }
    try {
        // get section, throws out_of_range if no section is available
        SectionEntries &section = this->at(sectName);

        const auto is_elem = [&key](const auto& elem) -> bool
        {
            return (strcasecmp(elem.first.c_str(), key.c_str()) == 0);
        };
        for (auto it = section.begin(); it != section.end(); ++it)
        {
            if (is_elem(*it))
            {
                section.erase(it);
                dirty=true;
                return true;
            }
        }
        return false;
    } catch (std::out_of_range &) {
        // section not found
        return false;
    }
}

// create a section -- returns true if created, false if already existed
bool ConfFile::CreateSection(const std::string &sectName)
{
    if (this->contains(sectName))
    {
        // section was found, return false
        return false;
    }

    // no section found, let's create one
    section_names.push_back(sectName);
    data.push_back({});
    dirty = true;

    assert(section_names.size() == data.size());
    return true;
}

// number of sections in list
size_t ConfFile::SectionCount() const noexcept
{ 
    return section_names.size();
}

// number of keys in all sections
size_t ConfFile::KeyCount() const noexcept
{
    size_t count = 0;

    for (const auto& section : data)
    {
        count += section.size();
    }

    return count;
}


// Protected Member Functions ///////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

// search for a section object by name
const ConfFile::SectionEntries &ConfFile::at(const std::string &sectName) const
{
    for (size_t i = 0; i < section_names.size(); ++i)
    {
        if (section_names[i] == sectName)
        {
            return data[i];
        }
    }
    throw std::out_of_range(
                std::string("ConfFile: section not found: ")
                + sectName);
}
ConfFile::SectionEntries &ConfFile::at(const std::string &sectName)
{
    for (size_t i = 0; i < section_names.size(); ++i)
    {
        if (section_names[i] == sectName)
        {
            return data[i];
        }
    }
    throw std::out_of_range(
                std::string("ConfFile: section not found: ")
                + sectName);
}

const std::vector<std::string> &ConfFile::getSectionNames() const
{
    return section_names;
}

// throws std::out_of_range if section does not exist
std::vector<std::string> ConfFile::keys(const std::string &sectName) const
{
    // get section, throws out_of_range if no section is available
    const SectionEntries &section = this->at(sectName);

    std::vector<std::string> key_names;
    key_names.reserve(section.size());

    for (const auto& entry : section)
    {
        const auto& entry_key = entry.first;
        key_names.push_back(entry_key);
    }

    return key_names;
}

bool ConfFile::contains(const std::string &section) const
{
    // empty section (default) is always contained
    if (section.empty())
    {
        return true;
    }
    // check each section name
    for (const auto& sec : section_names)
    {
        if (sec == section)
        {
            return true;
        }
    }
    return false;
}




/* done */
