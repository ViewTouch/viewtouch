//
// INI file IO library
// Derived from Gary McNickle's ConfFile code
//
// adapted by Brian Kowolowski, 20060126
//

#include <fstream>
#include <stdarg.h>

#include "conf_file.hh"

#include <iostream> // temp
#include <string.h>
#include <algorithm> // find_if

// init, attempt to load file
ConfFile::ConfFile(const std::string &name, const bool load) :
    fileName(name),
    dirty(false)
{
    sections.push_back(t_Section{});

    if (load)
        Load();
}

ConfFile::~ConfFile()
{
    if (dirty) {
        Save();
    }
    dirty = false;
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

    sections.clear();
    
    std::string line;
    char buffer[MAX_BUFFER_LEN]; 
    std::string section_name = "";

    while (!done) {
        memset(buffer, 0, MAX_BUFFER_LEN);
        File.getline(buffer, MAX_BUFFER_LEN);

        line = buffer;
        Trim(line);

        done = (File.eof() || File.bad() || File.fail());

        // throw out comments
        if (line.find_first_of(CommentIndicators) ==
            line.find_first_not_of(WhiteSpace)) {
            continue;
        }

        if (line[0] == '[') {
            line.erase(0, 1);
            // erase to eol
            line.erase(line.find_first_of(']'));

            section_name = line;
            CreateSection(section_name);
        } else if (line.size() > 0) {
            std::string value;
            int split = line.find_first_of(EqualIndicators);

            if (split > 0) {
                // copy, erase identifier
                value = line;
                value.erase(0, split);

                // erase value
                line.erase(split + 1);

                Trim(value);
            }
            // trim "identifier" portion; no value associated
            Trim(line);

            if (line.size() > 0) {
                SetValue(value, line, section_name);
            }
        }
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

    std::ofstream File(fileName, std::ios::out | std::ios::trunc);

    if (! File.is_open()) {
        std::cerr << "ConfFile::Save:  file open failure (" << fileName << ")" << std::endl;
        return false;
    }

    for (const t_Section &section : sections)
    {
        if (section.name.size() > 0) {
            File << "\n[" << section.name << "]" << std::endl;
        }

        for (const t_Key &elem : section.keys)
        {
            if (elem.key.size() > 0)
            {
                File << elem.key
                     << EqualIndicators[0]
                     << elem.value
                     << std::endl;
            }
        }
    }
        
    dirty = false;
    
    File.flush();
    File.close();

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
    t_Section* section = GetSection(sectName);

    if (section == nullptr) {
        if (! CreateSection(sectName))
            return false;

        section = GetSection(sectName);
    }

    // Sanity check...
    if ( section == nullptr )
        return false;

    // if the key does not exist in that section, and the value passed 
    // is not "" then add the new key.
    for (t_Key &entry : section->keys)
    {
        if (entry.key == keyName)
        {
            entry.value = value;
            return true;
        }
    }

    // key does not exist, add new one
    t_Key elem;

    elem.key = keyName;
    elem.value = value;

    dirty = true;
    section->keys.push_back(elem);
    return true;
}

// set the given value.  if key doesn't exist, create it.
bool ConfFile::SetValue(const Flt value, const std::string &key, const std::string &section)
{
    return SetValue(std::to_string(value), key, section);
}

// set the given value.  if key doesn't exist, create it.
bool ConfFile::SetValue(const int value, const std::string &key, const std::string &section)
{
    return SetValue(std::to_string(value), key, section);
}

// get value for named key in named section.  if return value is false,
//  key not found, contents of value is undefined.
bool ConfFile::GetValue(std::string &value, const std::string &keyName, const std::string &sectName) const
{
    const t_Section* section = GetSection(sectName);
    // Since our default section has a name value of "" this should
    // always return a valid section, wether or not it has any keys in it is
    // another matter.
    if (section == nullptr)
        return false;

    for (const t_Key &entry : section->keys)
    {
        if (entry.key == keyName)
        {
            value = entry.value;
            return true;
        }
    }

    // key not found
    return false;
}

bool ConfFile::GetValue(Flt &value, const std::string &key, const std::string &section) const
{
    std::string val;

    if (! GetValue(val, key, section))
        return false;

    if (val.empty())
        return false;

    value = std::stof(val);
    return true;
}

bool ConfFile::GetValue(int &value, const std::string &key, const std::string &section) const
{
    std::string val;

    if (! GetValue(val, key, section))
        return false;

    if (val.empty())
        return false;

    value = std::stoi(val);
    return true;
}

// delete a section -- "true" ==> deletion successful
bool ConfFile::DeleteSection(const std::string &section)
{
    auto is_section = [section](const t_Section &elem)->bool
    {
        return (strcasecmp(elem.name.c_str(), section.c_str()) == 0);
    };
    const auto iter = std::find_if(sections.begin(), sections.end(), is_section);
    if (iter != sections.end())
    {
        sections.erase(iter);
        return true;
    }

    return false;
}

// delete a key in a section -- "true" ==> deletion successful
bool ConfFile::DeleteKey(const std::string &key, const std::string &sectName)
{
    t_Section* section = GetSection(sectName);
    if (section == nullptr)
        return false;

    auto is_elem = [key](const t_Key &elem)->bool
    {
        return (strcasecmp(elem.key.c_str(), key.c_str()) == 0);
    };
    const auto iter = std::find_if(section->keys.begin(), section->keys.end(), is_elem);
    if (iter != section->keys.end())
    {
        section->keys.erase(iter);
        return true;
    }

    return false;
}

// create a section -- returns true if created, false if already existed
bool ConfFile::CreateSection(const std::string &sectName)
{
    if (GetSection(sectName)) {
        // already exists
        return false;
    }

    t_Section section;
    section.name = sectName;

    sections.push_back(section);
    dirty = true;

    return true;
}

// number of sections in list
int ConfFile::SectionCount() const
{ 
    return sections.size(); 
}

// number of keys in all sections
int ConfFile::KeyCount() const
{
    int count = 0;

    for (const t_Section &section : sections)
    {
        count += section.keys.size();
    }

    return count;
}


// Protected Member Functions ///////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

// search for a section object by name
const t_Section* ConfFile::GetSection(const std::string &sectName) const
{
    for (const t_Section &sec : sections)
    {
        if (sec.name == sectName)
        {
            return &sec;
        }
    }
    return nullptr;
}
// search for a section object by name
t_Section* ConfFile::GetSection(const std::string &sectName)
{
    for (t_Section &sec : sections)
    {
        if (sec.name == sectName)
        {
            return &sec;
        }
    }
    return nullptr;
}

void ConfFile::SetFilename(const std::string &name)
{
    if (fileName.size() != 0 && strcasecmp(name.c_str(), fileName.c_str()) != 0) {
        dirty = true;
    }

    fileName = name;
    sections.clear();
}




// Utility Functions ////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

// Trim
// Trims whitespace from both sides of a string.
// algorithm from g++ manual
void Trim(std::string& str)
{
    const std::string trimChars = WhiteSpace + EqualIndicators;

    // trim left
    str.erase(0, str.find_first_not_of(trimChars));

    // trim right
    str.erase(1 + str.find_last_not_of(trimChars));
}

/* done */
