/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998  
  
 *   This program is free software: you can redistribute it and/or modify 
 *   it under the terms of the GNU General Public License as published by 
 *   the Free Software Foundation, either version 3 of the License, or 
 *   (at your option) any later version.
 * 
 *   This program is distributed in the hope that it will be useful, 
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of 
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 *   GNU General Public License for more details. 
 * 
 *   You should have received a copy of the GNU General Public License 
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. 
 *
 * locale.hh - revision 24 (8/13/98)
 * Phrase lookup/translation & local conventions module
 */

#pragma once

#include "data_file.hh"
#include "utility.hh"
#include "list_utility.hh"
#include <map>

#define LANG_NONE                -1  // uninitialized POFile class
#define LANG_PHRASE               0  // for old defaults; do not search POFile classes
#define LANG_ENGLISH              1
#define LANG_FRENCH               2
#define LANG_GREEK                3
#define LANG_SPANISH              4
#define LANG_GERMAN               5
#define LANG_ITALIAN              6
#define LANG_PORTUGUESE           7
#define LANG_DUTCH                8


/*********************************************************************
 * Prototypes
 ********************************************************************/
void StartupLocalization();  // DW, 15 May 2002


class Settings;
class InputDataFile;
class OutputDataFile;

/*********************************************************************
 * Internal Classes -- do not use these outside of locale.cc
 ********************************************************************/
class PhraseInfo
{
public:
    PhraseInfo *next;
    PhraseInfo *fore;
    Str key;
    Str value;

    // Constructors
    PhraseInfo();
    PhraseInfo(const char* k, const genericChar* v);

    // Member Functions
    int Read(InputDataFile &df, int version);
    int Write(OutputDataFile &df, int version);
};

class POEntry
{
    char key[STRLONG];
    char value[STRLONG];

public:
    POEntry *next;

    POEntry();
    POEntry(const char* newkey, const char* newvalue);
    const char* Key() { return key; }
    const char* Value() { return value; }
};

class POFile
{
    int lang;
    int loaded;
    char filename[STRLONG];
    KeyValueInputFile *infile;
    POEntry *entry_head;
    POEntry *entry_tail;

    int FindPOFilename();
    int ReadPO();
    int Add(const char* newkey, const char* newvalue);

public:
    POFile *next;
    POFile();
    POFile(int po_lang);
    int IsLang(int language) { return (lang == language); }
    int Find( char* dest, const char* str, int po_lang );
};

class POFileList
{
    POFile *head;

public:
    POFileList();
    POFile *FindPOFile(int lang);
    const char* FindPOString(const char* str, int lang, int clear = 0);
    void ClearPOFiles();  // Clear all loaded PO files to force reload
};


/*********************************************************************
 * External Classes
 ********************************************************************/

class Locale
{
    DList<PhraseInfo> phrase_list;
    POFileList        pofile_list;

public:
    Locale *next;
    Locale *fore;
    Str name;
    Str filename;
    PhraseInfo **search_array;
    int array_size;

    // Constructor
    Locale();
    // Destructor
    ~Locale() { Purge(); }

    // Member Functions
    PhraseInfo *PhraseList()    { return phrase_list.Head(); }
    PhraseInfo *PhraseListEnd() { return phrase_list.Tail(); }
    int         PhraseCount()   { return phrase_list.Count(); }

    int Save();
    int Load(const char* file);
    int Add(PhraseInfo *ph);
    int Remove(PhraseInfo *ph);
    int Purge();

    int BuildSearchArray();
    PhraseInfo *Find(const char* key);
    const char* Translate(const char* str, int lang = 0, int clear = 0 );
    const genericChar* TranslatePO(const char* str, int lang = LANG_ENGLISH, int clear = 0);
    int NewTranslation(const char* str, const genericChar* value);
    void ClearPOFiles();  // Clear PO files to force reload

    const char* TimeDate(Settings* s, const TimeInfo &timevar, int format, int lang, genericChar* str = 0);
    char* Page( int current, int page_max, int lang, genericChar* str );

    // UI Data Translation Methods (for po_file/vt_data content)
    const genericChar* TranslateUIData(const genericChar* key);
    int NewUIDataTranslation(const genericChar* key, const genericChar* value);
    int LoadUIDataTranslations();
    int SaveUIDataTranslations();
    void ClearUIDataTranslations();

private:
    // UI Data Translation Storage
    std::map<std::string, std::string> ui_data_translations;
    std::string ui_data_translation_file;
};


/*********************************************************************
 * Global Data
 ********************************************************************/
extern Locale *MasterLocale;

struct PhraseEntry
{
    int page;
    const genericChar* text;
};

extern PhraseEntry PhraseData[];
