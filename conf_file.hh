//
// INI file IO library
// Derived from Gary McNickle's ConfFile code
//
// adapted by Brian Kowolowski, 20060126
//


#ifndef __CONF_FILE_H__
#define __CONF_FILE_H__

#include <string>
#include <vector>

#include "basic.hh"

// MAX_BUFFER_LEN
// Used simply as a max size of some internal buffers. Determines the maximum
// length of a line that will be read from or written to the file or the
// report output.
#define MAX_BUFFER_LEN                1024

typedef std::string t_Str;
const t_Str CommentIndicators = t_Str(";#");
const t_Str EqualIndicators   = t_Str("=:"); 
const t_Str WhiteSpace = t_Str(" \t\n\r");

// st_key
// This structure stores the definition of a key. A key is a named identifier
// that is associated with a value.
typedef struct st_key
{
    t_Str        key;
    t_Str        value;

    st_key()
    {
        key = t_Str("");
        value = t_Str("");
    }

} t_Key;

typedef std::vector<t_Key> KeyList;
typedef KeyList::iterator KeyItor;

// st_section
// This structure stores the definition of a section. A section contains any number
// of keys (see st_keys).
typedef struct st_section
{
    t_Str        name;
    KeyList      keys;

    st_section()
    {
        name = t_Str("");
        keys.clear();
    }

} t_Section;

typedef std::vector<t_Section> SectionList;
typedef SectionList::iterator SectionItor;



/// General Purpose Utility Functions ///////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
void  Trim(t_Str& szStr);
int   WriteLn(std::fstream& stream, const char* fmt, ...);



class ConfFile
{
public:
                ConfFile(t_Str fileName, bool load = false);
    virtual     ~ConfFile();

    bool        Load();
    bool        Save();

                // get a key's value;  overloaded for type conversion convenience
    bool        GetValue(t_Str &value, t_Str key, t_Str section = t_Str("")); 
    bool        GetValue(Flt &value, t_Str key, t_Str section = t_Str(""));
    bool        GetValue(int &value, t_Str key, t_Str section = t_Str(""));
                // set a key's value; created if doesn't exist
    bool        SetValue(t_Str value, t_Str key, t_Str section = t_Str(""));
    bool        SetValue(Flt value, t_Str key, t_Str section = t_Str(""));
    bool        SetValue(int value, t_Str key, t_Str section = t_Str(""));

                // remove a key from a section
    bool        DeleteKey(t_Str key, t_Str section = t_Str(""));

                // remove a section from the file
    bool        DeleteSection(t_Str section);
                // CreateSection: Creates the new section if it does not allready
                // exist. Section is created with no keys.
    bool        CreateSection(t_Str section);

                // returns number of sections in file
    int         SectionCount();
                // returns number of keys across all sections
    int         KeyCount();

    void        SetFilename(t_Str name);


protected:
                // GetKey: Returns the requested key (if found) from the requested
                // Section. Returns NULL otherwise.
    t_Key*      GetKey(t_Str key, t_Str section);
                // GetSection: Returns the requested section (if found), NULL otherwise.
    t_Section*  GetSection(t_Str section);


public:

protected:
    SectionList sections; // Our list of sections
    t_Str       fileName; // The filename to write to
    bool        dirty;    // Tracks whether or not data has changed.
};


#endif
