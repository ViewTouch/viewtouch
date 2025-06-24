//
// INI file IO library
// Derived from Gary McNickle's ConfFile code
//
// adapted by Brian Kowolowski, 20060126
//


#ifndef CONF_FILE_HH
#define CONF_FILE_HH

#include <string>
#include <vector>
#include <utility> // std::pair
#include "basic.hh"  // REFACTOR: Added for Flt type definition

// MAX_BUFFER_LEN
// Used simply as a max size of some internal buffers. Determines the maximum
// length of a line that will be read from or written to the file or the
// report output.
constexpr size_t MAX_BUFFER_LEN = 1024;

const std::string CommentIndicators = ";#";
const std::string EqualIndicators   = "=:";
const std::string WhiteSpace = " \t\n\r";

class ConfFile
{
public:
    // a section is a list of key-value-pairs
    typedef std::pair<std::string, std::string> SectionEntry;
    typedef std::vector<SectionEntry> SectionEntries;

    // create config-file, if load is true, try to load config-file into memory
    // throws std::runtime_error if Load fails
                ConfFile(const std::string &fileName, const bool load = false);
                ~ConfFile();

    // set dirty flag, when dirty is set the config file will be written
    void        set_dirty(const bool value=true);

    bool        Load();
    bool        Save();

                // get a key's value;  overloaded for type conversion convenience
    bool        GetValue(std::string &value, const std::string &key, const std::string &section = "") const;
    bool        GetValue(double &value, const std::string &key, const std::string &section = "") const;
    bool        GetValue(int &value, const std::string &key, const std::string &section = "") const;
    bool        GetValue(Flt &value, const std::string &key, const std::string &section = "") const;      // REFACTOR: Added overload for Flt (float) type
                // set a key's value; created if doesn't exist
    bool        SetValue(const std::string &value, const std::string &key, const std::string &section = "");
    bool        SetValue(const double value, const std::string &key, const std::string &section = "");
    bool        SetValue(const int value, const std::string &key, const std::string &section = "");

                // remove a key from a section
    bool        DeleteKey(const std::string &key, const std::string &section = "");

                // remove a section from the file
    bool        DeleteSection(const std::string &section);
                // CreateSection: Creates the new section if it does not allready
                // exist. Section is created with no keys.
    bool        CreateSection(const std::string &section);

                // returns number of sections in file
    size_t      SectionCount() const;
                // returns number of keys across all sections
    size_t      KeyCount() const;

    // get a list of section names
    const std::vector<std::string> &getSectionNames() const;
    // get a list of keys of the specified section
    // throws std::out_of_range if section does not exist
    std::vector<std::string> keys(const std::string &section = "") const;

    // GetSection: Returns the requested section (if found), otherwise std::out_of_range
    const SectionEntries &at(const std::string &section) const;
    // check if section is contained
    bool contains(const std::string &section) const;

protected:
    // GetSection: Returns the requested section (if found), otherwise std::out_of_range
    SectionEntries &at(const std::string &section);

private:
    const std::string fileName; // The filename to write to
    std::vector<std::string> section_names = {""};
    std::vector<SectionEntries> data = {{}};
    bool dirty;    // Tracks whether or not data has changed.
};

#endif // CONF_FILE_HH
