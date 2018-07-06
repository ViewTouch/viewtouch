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

// init, attempt to load file
ConfFile::ConfFile(t_Str name, bool load)
{
    dirty = false;
    fileName = name;
    sections.push_back(*(new t_Section));

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
    std::fstream File(fileName.c_str(), std::ios::in);

    if (! File.is_open()) {
        return false;
    }

    bool done = false;

    sections.clear();
    
    t_Str line;
    char buffer[MAX_BUFFER_LEN]; 
    t_Section* section = GetSection("");


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

        if (line.find_first_of('[') == 0) {
            line.erase(0, 1);
            // erase to eol
            line.erase(line.find_first_of(']'));

            CreateSection(line);
            section = GetSection(line);
        } else if (line.size() > 0) {
            t_Str value;
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
                SetValue(value, line, section->name);
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

    std::fstream File(fileName.c_str(), std::ios::out | std::ios::trunc);

    if (! File.is_open()) {
        std::cerr << "ConfFile::Save:  file open failure (" << fileName.c_str() << ")" << std::endl;
        return false;
    }

    SectionItor s_pos;
    KeyItor k_pos;
    t_Section section;
    t_Key key;

    for (s_pos = sections.begin(); s_pos != sections.end(); s_pos++) {
        section = (*s_pos);

        if (section.name.size() > 0) {
            WriteLn(File, "\n[%s]", section.name.c_str());
        }

        for (k_pos = section.keys.begin(); k_pos != section.keys.end(); k_pos++) {
            key = (*k_pos);

            if (key.key.size() > 0) {
                WriteLn(File, "%s%c%s", 
                    key.key.c_str(),
                    EqualIndicators[0],
                    key.value.c_str());
            }
        }
    }
        
    dirty = false;
    
    File.flush();
    File.close();

    return true;
}

// set the given value.  if key doesn't exist, create it.
bool ConfFile::SetValue(t_Str value, t_Str keyName, t_Str sectName)
{
    t_Key* key = GetKey(keyName, sectName);
    t_Section* section = GetSection(sectName);

    if (section == NULL) {
        if (! CreateSection(sectName))
            return false;

        section = GetSection(sectName);
    }

    // Sanity check...
    if ( section == NULL )
        return false;

    // if the key does not exist in that section, and the value passed 
    // is not t_Str("") then add the new key.
    if (key == NULL) {
        key = new t_Key;

        key->key = keyName;
        key->value = value;
        
        dirty = true;
        section->keys.push_back(*key);
        return true;
    }

    if (key != NULL) {
        key->value = value;
        dirty = true;
        return true;
    }

    return false;
}

// set the given value.  if key doesn't exist, create it.
bool ConfFile::SetValue(Flt value, t_Str key, t_Str section)
{
    char buf[64];

    snprintf(buf, 64, "%f", value);

    return SetValue(buf, key, section);
}

// set the given value.  if key doesn't exist, create it.
bool ConfFile::SetValue(int value, t_Str key, t_Str section)
{
    char buf[64];

    snprintf(buf, 64, "%d", value);

    return SetValue(buf, key, section);

}

// get value for named key in named section.  if return value is false,
//  key not found, contents of value is undefined.
bool ConfFile::GetValue(t_Str &value, t_Str keyName, t_Str section)
{
    t_Key* key = GetKey(keyName, section);

    if (key == NULL) {
        return false;
    }

    value = key->value;
    return true;
}

bool ConfFile::GetValue(Flt &value, t_Str key, t_Str section)
{
    t_Str val;

    if (! GetValue(val, key, section))
        return false;

    if (val.size() == 0)
        return false;

    value = (Flt)atof(val.c_str());
    return true;
}

bool ConfFile::GetValue(int &value, t_Str key, t_Str section)
{
    t_Str val;

    if (! GetValue(val, key, section))
        return false;

    if ( val.size() == 0 )
        return false;

    value = atoi(val.c_str());
    return true;
}

// delete a section -- "true" ==> deletion successful
bool ConfFile::DeleteSection(t_Str section)
{
    SectionItor pos;

    for (pos = sections.begin(); pos != sections.end(); pos++) {
        if (strcasecmp((*pos).name.c_str(), section.c_str()) == 0) {
            sections.erase(pos);
            return true;
        }
    }

    return false;
}

// delete a key in a section -- "true" ==> deletion successful
bool ConfFile::DeleteKey(t_Str key, t_Str sectName)
{
    KeyItor pos;
    t_Section* section;

    if ((section = GetSection(sectName)) == NULL)
        return false;

    for (pos = section->keys.begin(); pos != section->keys.end(); pos++) {
        if (strcasecmp((*pos).key.c_str(), key.c_str()) == 0) {
            section->keys.erase(pos);
            return true;
        }
    }

    return false;
}

// create a section -- returns true if created, false if already existed
bool ConfFile::CreateSection(t_Str sectName)
{
    t_Section* section = GetSection(sectName);

    if (section) {
        // already exists
        return false;
    }

    section = new t_Section;

    section->name = sectName;
    sections.push_back(*section);
    dirty = true;

    return true;
}

// number of sections in list
int ConfFile::SectionCount() 
{ 
    return sections.size(); 
}

// number of keys in all sections
int ConfFile::KeyCount()
{
    int count = 0;
    SectionItor pos;

    for (pos = sections.begin(); pos != sections.end(); pos++)
        count += (*pos).keys.size();

    return count;
}


// Protected Member Functions ///////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

// search for a key object by name and section
t_Key* ConfFile::GetKey(t_Str keyName, t_Str sectName)
{
    KeyItor pos;
    t_Section* section;

    // Since our default section has a name value of t_Str("") this should
    // always return a valid section, wether or not it has any keys in it is
    // another matter.
    if ((section = GetSection(sectName)) == NULL)
        return NULL;

    for (pos = section->keys.begin(); pos != section->keys.end(); pos++) {
        if (strcasecmp((*pos).key.c_str(), keyName.c_str()) == 0)
            return (t_Key*)&(*pos);
    }

    return NULL;
}

// search for a section object by name
t_Section* ConfFile::GetSection(t_Str sectName)
{
    SectionItor pos;

    for (pos = sections.begin(); pos != sections.end(); pos++) {
        if (strcasecmp((*pos).name.c_str(), sectName.c_str()) == 0) 
            return (t_Section*)&(*pos);
    }

    return NULL;
}

void ConfFile::SetFilename(t_Str name)
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
void Trim(t_Str& str)
{
    t_Str trimChars = WhiteSpace;
    
    trimChars += EqualIndicators;

    // trim left
    str.erase(0, str.find_first_not_of(trimChars));

    // trim right
    str.erase(1 + str.find_last_not_of(trimChars));
}

// fprintf for stream
int WriteLn(std::fstream& stream, const char* fmt, ...)
{
    char buf[MAX_BUFFER_LEN + 1];
    int len;

    memset(buf, 0, MAX_BUFFER_LEN + 1);
    va_list args;

    va_start(args, fmt);
    len = vsnprintf(buf, MAX_BUFFER_LEN, fmt, args);
    va_end(args);


    if (buf[len] != '\n')
        buf[len++] = '\n';


    stream.write(buf, len);

    return len;
}

/* done */
