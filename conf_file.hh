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
constexpr size_t MAX_BUFFER_LEN = 1024;

const std::string CommentIndicators = ";#";
const std::string EqualIndicators   = "=:";
const std::string WhiteSpace = " \t\n\r";

// st_key
// This structure stores the definition of a key. A key is a named identifier
// that is associated with a value.
typedef struct st_key
{
    std::string key   = "";
    std::string value = "";

    st_key()
    {}
} t_Key;

typedef std::vector<t_Key> KeyList;

// st_section
// This structure stores the definition of a section. A section contains any number
// of keys (see st_keys).
typedef struct st_section
{
    std::string  name = "";
    KeyList      keys = {};

    st_section()
    {}
} t_Section;

typedef std::vector<t_Section> SectionList;
typedef SectionList::iterator SectionItor;



/// General Purpose Utility Functions ///////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
void  Trim(std::string& szStr);



class ConfFile
{
public:
                ConfFile(const std::string &fileName, const bool load = false);
    virtual     ~ConfFile();

    bool        Load();
    bool        Save();

                // get a key's value;  overloaded for type conversion convenience
    bool        GetValue(std::string &value, const std::string &key, const std::string &section = "") const;
    bool        GetValue(Flt &value, const std::string &key, const std::string &section = "") const;
    bool        GetValue(int &value, const std::string &key, const std::string &section = "") const;
                // set a key's value; created if doesn't exist
    bool        SetValue(const std::string &value, const std::string &key, const std::string &section = "");
    bool        SetValue(const Flt value, const std::string &key, const std::string &section = "");
    bool        SetValue(const int value, const std::string &key, const std::string &section = "");

                // remove a key from a section
    bool        DeleteKey(const std::string &key, const std::string &section = "");

                // remove a section from the file
    bool        DeleteSection(const std::string &section);
                // CreateSection: Creates the new section if it does not allready
                // exist. Section is created with no keys.
    bool        CreateSection(const std::string &section);

                // returns number of sections in file
    int         SectionCount() const;
                // returns number of keys across all sections
    int         KeyCount() const;

    void        SetFilename(const std::string &name);


protected:
    // GetSection: Returns the requested section (if found), nullptr otherwise.
    const t_Section* GetSection(const std::string &section) const;
    t_Section* GetSection(const std::string &section);


public:

protected:
    SectionList sections; // Our list of sections
    std::string fileName; // The filename to write to
    bool        dirty;    // Tracks whether or not data has changed.
};


#endif
