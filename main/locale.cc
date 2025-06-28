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
 * locale.cc - revision 35 (10/7/98)
 * Implementation of locale module
 */

#include "locale.hh"
#include "data_file.hh"
#include "labels.hh"
#include "employee.hh"
#include "settings.hh"
#include "system.hh"
#include "terminal.hh"
#include "manager.hh"
#include <cstdio>
#include <cstring>
#include <cctype>
#include <clocale>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

void StartupLocalization()
{
    if (setlocale(LC_ALL, "") == nullptr)
    {
	    (void) fprintf(stderr, "Cannot set locale.\n");
	    exit(1);
	}
}


/**** Global Data ****/
Locale *MasterLocale = nullptr;


/**** Phrase Data (default U.S. English) ****/
#define PHRASE_SUNDAY    0
#define PHRASE_MONDAY    1
#define PHRASE_TUESDAY   2
#define PHRASE_WEDNESDAY 3
#define PHRASE_THURSDAY  4
#define PHRASE_FRIDAY    5
#define PHRASE_SATURDAY  6

#define PHRASE_SUN       7
#define PHRASE_MON       8
#define PHRASE_TUE       9
#define PHRASE_WED       10
#define PHRASE_THU       11
#define PHRASE_FRI       12
#define PHRASE_SAT       13

#define PHRASE_JANUARY   14
#define PHRASE_FEBRUARY  15
#define PHRASE_MARCH     16
#define PHRASE_APRIL     17
#define PHRASE_MAY       18
#define PHRASE_JUNE      19
#define PHRASE_JULY      20
#define PHRASE_AUGUST    21
#define PHRASE_SEPTEMBER 22
#define PHRASE_OCTOBER   23
#define PHRASE_NOVEMBER  24
#define PHRASE_DECEMBER  25

#define PHRASE_M1        26
#define PHRASE_M2        27
#define PHRASE_M3        28
#define PHRASE_M4        29
#define PHRASE_M5        30
#define PHRASE_M6        31
#define PHRASE_M7        32
#define PHRASE_M8        33
#define PHRASE_M9        34
#define PHRASE_M10       35
#define PHRASE_M11       36
#define PHRASE_M12       37

#define PHRASE_YES               38
#define PHRASE_NO                39
#define PHRASE_ON                40
#define PHRASE_OFF               41

static const genericChar* AMorPM[] = { "am", "pm"};

PhraseEntry PhraseData[] = {
    // Days of Week (0 - 6)
    {0, "Sunday"},
    {0, "Monday"},
    {0, "Tuesday"},
    {0, "Wednesday"},
    {0, "Thursday"},
    {0, "Friday"},
    {0, "Saturday"},
    // Abrv. Days of Week (7 - 13)
    {1, "Sun"},
    {1, "Mon"},
    {1, "Tue"},
    {1, "Wed"},
    {1, "Thu"},
    {1, "Fri"},
    {1, "Sat"},
    // Months (14 - 25)
    {2, "January"},
    {2, "February"},
    {2, "March"},
    {2, "April"},
    {2, "May"},
    {2, "June"},
    {2, "July"},
    {2, "August"},
    {2, "September"},
    {2, "October"},
    {2, "November"},
    {2, "December"},
    // Abrv. Months (26 - 37)
    {3, "Jan"},
    {3, "Feb"},
    {3, "Mar"},
    {3, "Apr"},
    {3, "May"},
    {3, "Jun"},
    {3, "Jul"},
    {3, "Aug"},
    {3, "Sep"},
    {3, "Oct"},
    {3, "Nov"},
    {3, "Dec"},
    // General (38 - 41)
    {4, "Yes"},
    {4, "No"},
    {4, "On"},
    {4, "Off"},
    {4, "Page"},
    {4, "Table"},
    {4, "Guests"},
    {4, "Okay"},
    {4, "Cancel"},
    {4, "Take Out"},
    {4, "TO GO"},
    {4, "Catering"},
    {4, "Cater"},
    {4, "Delivery"},
    {4, "Deliver"},
    {4, "PENDING"},
    // Greetings (42 - 43)
    {5, "Welcome"},
    {5, "Hello"},
    // Statements (44 - 45)
    {6, "Starting Time Is"},
    {6, "Ending Time Is"},
    {6, "Pick A Job For This Shift"},
    // Commands (46 - 48)
    {7, "Please Enter Your User ID"},
    {7, "Press START To Enter"},
    {7, "Please Try Again"},
    {7, "Contact a manager to be reactivated"},
    // Errors (49 - 56)
    {8, "Password Incorrect"},
    {8, "Unknown User ID"},
    {8, "You're Using Another Terminal"},
    {8, "You're Not On The Clock"},
    {8, "You're Already On The Clock"},
    {8, "You Don't Use The Clock"},
    {8, "You Still Have Open Checks"},
    {8, "You Still Have An Assigned Drawer"},
    {8, "Your Record Is Inactive"},
    // Index Pages
    {9, "General"},
    {9, "Breakfast"},
    {9, "Brunch"},
    {9, "Lunch"},
    {9, "Early Dinner"},
    {9, "Dinner"},
    {9, "Late Night"},
    {9, "Bar"},
    {9, "Wine"},
    {9, "Cafe"},
    // Jobs
    {10, "No Job"},
    {10, "Dishwasher"},
    {10, "Busperson"},
    {10, "Line Cook"},
    {10, "Prep Cook"},
    {10, "Chef"},
    {10, "Cashier"},
    {10, "Server"},
    {10, "Server/Cashier"},
    {10, "Bartender"},
    {10, "Host/Hostess"},
    {10, "Bookkeeper"},
    {10, "Supervisor"},
    {10, "Assistant Manager"},
    {10, "Manager"},
    // Families
    {11, FamilyName[0]},
    {11, FamilyName[1]},
    {11, FamilyName[2]},
    {11, FamilyName[3]},
    {11, FamilyName[4]},
    {11, FamilyName[5]},
    {11, FamilyName[6]},
    {11, FamilyName[7]},
    {11, FamilyName[8]},
    {11, FamilyName[9]},
    {11, FamilyName[10]},
    {11, FamilyName[11]},
    {11, FamilyName[12]},
    {11, FamilyName[13]},
    {11, FamilyName[14]},
    {11, FamilyName[15]},
    {12, FamilyName[16]},
    {12, FamilyName[17]},
    {12, FamilyName[18]},
    {12, FamilyName[19]},
    {12, FamilyName[20]},
    {12, FamilyName[21]},
    {12, FamilyName[22]},
    {12, FamilyName[23]},
    {12, FamilyName[24]},
    {12, FamilyName[25]},
    {12, FamilyName[26]},
    {12, FamilyName[27]},
    {12, FamilyName[28]},
    {12, FamilyName[29]},
    {12, FamilyName[30]},

    {13, "Pre-Authorize"},
    {13, "Authorize"},
    {13, "Void"},
    {13, "Refund"},
    {13, "Add Tip"},
    {13, "Cancel"},
    {13, "Undo Refund"},
    {13, "Manual Entry"},
    {13, "Done"},
    {13, "Credit"},
    {13, "Debit"},
    {13, "Swipe"},
    {13, "Clear"},
    {13, "Card Number"},
    {13, "Expires"},
    {13, "Holder"},
    {14, "Charge Amount"},
    {14, "Tip Amount"},
    {14, "Total"},
    {14, "Void Successful"},
    {14, "Refund Successful"},
    {14, "Please select card type."},
    {14, "Please select card entry method."},
    {14, "Please swipe the card"},
    {14, "or select Manual Entry"},
    {14, "PreAuthorizing"},
    {14, "Authorizing"},
    {14, "Voiding"},
    {14, "Refunding"},
    {14, "Cancelling Refund"},
    {14, "Please Swipe Card"},
    {14, "Please Wait"},

    {15, "Check"},
    {15, "Checks"},
    {15, "All Cash & Checks"},
    {15, "Total Check Payments"},
    {15, "Pre-Auth Complete"},
    {15, "Fast Food"},

    {-1, nullptr}
};

/*********************************************************************
 * PhraseInfo Class
 ********************************************************************/

PhraseInfo::PhraseInfo()
{
    FnTrace("PhraseInfo::PhraseInfo()");
    next = nullptr;
    fore = nullptr;
}

PhraseInfo::PhraseInfo(const char* k, const genericChar* v)
{
    FnTrace("PhraseInfo::PhraseInfo(const char* , const char* )");
    next = nullptr;
    fore = nullptr;
    key.Set(k);
    value.Set(v);
}

int PhraseInfo::Read(InputDataFile &df, int version)
{
    FnTrace("PhraseInfo::Read()");
    int error = 0;
    error += df.Read(key);
    error += df.Read(value);
    return error;
}

int PhraseInfo::Write(OutputDataFile &df, int version)
{
    FnTrace("PhraseInfo::Write()");
    int error = 0;
    error += df.Write(key);
    error += df.Write(value);
    return error;
}


/*********************************************************************
 * POEntry Class
 ********************************************************************/

POEntry::POEntry()
{
    key[0] = '\0';
    value[0] = '\0';
    next = nullptr;
}

POEntry::POEntry(const char* newkey, const char* newvalue)
{
    if (strlen(newkey) < STRLONG && strlen(newvalue) < STRLONG)
    {
        strcpy(key, newkey);
        strcpy(value, newvalue);
    }
    else
    {
        key[0] = '\0';
        value[0] = '\0';
    }

    next = nullptr;
}


/*********************************************************************
 * POFile Class
 *  For now all searching will be done direct to harddrive, so it's
 *  very inefficient.  In the end, I want to cache everything, but
 *  right now I don't have time to do it properly.  Hopefully, at
 *  least the interface is right so no methods outside this class
 *  will need to be rewritten.
 ********************************************************************/

POFile::POFile()
{
    FnTrace("POFile::POFile()");
    lang        = LANG_NONE;
    loaded      = 0;
    filename[0] = '\0';
    infile      = nullptr;
    entry_head  = nullptr;
    entry_tail  = nullptr;
}

POFile::POFile(int po_lang)
{
    FnTrace("POFile::POFile(int)");
    lang        = po_lang;
    loaded      = 0;
    filename[0] = '\0';
    infile      = nullptr;
    entry_head  = nullptr;
    entry_tail  = nullptr;

    ReadPO();
}

int POFile::FindPOFilename()
{
    FnTrace("POFile::FindPOFilename()");
    int retval = 1;
    char pathinfo[] = "/dat/languages/viewtouch.po_";

    if (lang == LANG_ENGLISH)
        snprintf(filename, STRLONG, "%s%s%s", VIEWTOUCH_PATH, pathinfo, "EN");
    else if (lang == LANG_FRENCH)
        snprintf(filename, STRLONG, "%s%s%s", VIEWTOUCH_PATH, pathinfo, "FR");

    if (filename[0] != '\0')
        retval = 0;

    return retval;
}

/****
 * ReadPO:  Returns 1 if the file is successfully opened and read,
 *   0 otherwise.
 ****/
int POFile::ReadPO()
{
    FnTrace("POFile::ReadPO()");
    int retval = 0;
    char keybuff[STRLONG];
    char valbuff[STRLONG];

    if (filename[0] == '\0')
        FindPOFilename();

    if (filename[0] != '\0')
    {
        infile = new KeyValueInputFile(filename);
        if (infile->Open())
        {
            while(infile->Read(keybuff, valbuff, STRLONG))
            {
                if (strlen(keybuff) > 0)
                    Add(keybuff, valbuff);
            }

            loaded = 1;
            retval = 1;
        }
        
    }

    return retval;
}

/****
 * Add:  Returns 1 if the key/value pair is added, 0 otherwise.
 ****/
int POFile::Add(const char* newkey, const char* newvalue)
{
    FnTrace("POFile::Add()");
    int retval = 0;
    POEntry *entry;
    
    entry = new POEntry(newkey, newvalue);
    if (entry != nullptr)
    {
        if (entry_head == nullptr)
        {
            entry_head = entry;
            entry_tail = entry;
        }
        else
        {
            entry_tail->next = entry;
            entry_tail = entry;
        }
    }

    return retval;
}

/****
 * Find:  Returns 0 on failure, 1 on success.
 ****/
int POFile::Find(char* dest, const char* str, int po_lang)
{
    FnTrace("POFile::Find()");
    int retval = 0;
    POEntry *po_entry = entry_head;

    if (po_lang == lang)
    {
        while (po_entry != nullptr)
        {
            if (strcmp(str, po_entry->Key()) == 0)
            {
                strcpy(dest, po_entry->Value());
                po_entry = nullptr;  // exit condition
                retval = 1; 
            }
            else
                po_entry = po_entry->next;
        }
    }

    return retval;
}


/*********************************************************************
 * POFileList Class
 ********************************************************************/

POFileList::POFileList()
{
    FnTrace("POFileList::POFileList()");
    head = nullptr;
}

POFile *POFileList::FindPOFile(int lang)
{
    FnTrace("POFileList::FindPOFile()");
    POFile *po_file = head;
    POFile *retval  = nullptr;

    while (po_file != nullptr && retval == nullptr)
    {
        if (po_file->IsLang(lang))
            retval = po_file;
        else
            po_file = po_file->next;
    }

    return po_file;
}

const char* POFileList::FindPOString(const char* str, int lang, int clear)
{
    FnTrace("POFileList::FindPOString()");
    char buffer[STRLONG];
    static char retstr[STRLONG];
    POFile *po_file = nullptr;

    if (clear)
        retstr[0] = '\0';
    else
        strcpy(retstr, str);

    po_file = FindPOFile(lang);
    if (po_file == nullptr)
    {
        po_file = new POFile(lang);
        po_file->next = head;
        head = po_file;
    }

    if (po_file != nullptr)
    {
        if (po_file->Find(buffer, str, lang))
            strcpy(retstr, buffer);
    }

    return retstr;
}


/*********************************************************************
 * Locale Class
 ********************************************************************/
Locale::Locale()
{
    FnTrace("Locale::Locale()");
    next = nullptr;
    fore = nullptr;
    search_array = nullptr;
    array_size = 0;
}

int Locale::Load(const char* file)
{
    FnTrace("Locale::Load()");
    if (file)
        filename.Set(file);

    int version = 0;
    InputDataFile df;
    if (df.Open(filename.Value(), version))
        return 1;

    // VERSION NOTES
    // 1 (5/17/97) Initial version

    genericChar str[256];
    if (version < 1 || version > 1)
    {
        snprintf(str, sizeof(str), "Unknown locale file version %d", version);
        ReportError(str);
        return 1;
    }

    int tmp;
    Purge();
    df.Read(name);
    df.Read(tmp);
    df.Read(tmp);
    df.Read(tmp);
    df.Read(tmp);

    int count = 0;
    df.Read(count);
    for (int i = 0; i < count; ++i)
    {
        PhraseInfo *ph = new PhraseInfo;
        ph->Read(df, 1);
        Add(ph);
    }
    return 0;
}

int Locale::Save()
{
    FnTrace("Locale::Save()");
    if (filename.size() <= 0)
        return 1;

    BackupFile(filename.Value());

    // Save Version 1
    OutputDataFile df;
    if (df.Open(filename.Value(), 1, 1))
        return 1;

    df.Write(name);
    df.Write(0);
    df.Write(0);
    df.Write(0);
    df.Write(0);

    df.Write(PhraseCount());
    for (PhraseInfo *ph = PhraseList(); ph != nullptr; ph = ph->next)
        ph->Write(df, 1);
    return 0;
}

int Locale::Add(PhraseInfo *ph)
{
    FnTrace("Locale::Add()");
    if (ph == nullptr)
        return 1;

    if (search_array)
    {
	free(search_array);
        search_array = nullptr;
        array_size = 0;
    }

    // start at end of list and work backwords
    const genericChar* n = ph->key.Value();
    PhraseInfo *ptr = PhraseListEnd();
    while (ptr && StringCompare(n, ptr->key.Value()) < 0)
        ptr = ptr->fore;

    // Insert ph after ptr
    return phrase_list.AddAfterNode(ptr, ph);
}

int Locale::Remove(PhraseInfo *ph)
{
    FnTrace("Locale::Remove()");
    if (ph == nullptr)
        return 1;

    if (search_array)
    {
	free(search_array);
        search_array = nullptr;
        array_size = 0;
    }
    return phrase_list.Remove(ph);
}

int Locale::Purge()
{
    FnTrace("Locale::Purge()");
    phrase_list.Purge();

    if (search_array)
    {
	free(search_array);
        search_array = nullptr;
        array_size = 0;
    }
    return 0;
}

/****
 * BuildSearchArray: constructs binary search array (other functions
 * call this automatically)
 ****/
int Locale::BuildSearchArray()
{
    FnTrace("Locale::BuildSearchArray()");
    if (search_array)
    {
	free(search_array);
    }

    array_size = PhraseCount();
    search_array = (PhraseInfo **)calloc(sizeof(PhraseInfo *), (array_size + 1));
    if (search_array == nullptr)
        return 1;

    PhraseInfo *ph = PhraseList();
    for (int i = 0; i < array_size; ++i)
    {
        search_array[i] = ph;
        ph = ph->next;
    }
    return 0;
}

/****
 * Find: find record for word to translate - returns nullptr if none
 ****/
PhraseInfo *Locale::Find(const char* key)
{
    FnTrace("Locale::Find()");
    if (key == nullptr)
        return nullptr;
    if (search_array == nullptr)
        BuildSearchArray();

    int l = 0;
    int r = array_size - 1;
    while (r >= l)
    {
        int x = (l + r) / 2;
        PhraseInfo *ph = search_array[x];

        int val = StringCompare(key, ph->key.Value());
        if (val < 0)
            r = x - 1;
        else if (val > 0)
            l = x + 1;
        else
            return ph;
    }
    return nullptr;
}

/****
 * Translate: translates string or just returns original string if no
 * translation
 ****/
const char* Locale::Translate(const char* str, int lang, int clear)
{
    FnTrace("Locale::Translate()");
    char buffer[STRLONG];

    if (lang == LANG_PHRASE)
    {
        PhraseInfo *ph = Find(str);
        if (ph == nullptr)
        {
            //if (clear)
                //str[0] = '\0';	#TODO
            return str;
        }
        else
            return ph->value.Value();
    }
    else
    {
        strcpy(buffer, str);
        return pofile_list.FindPOString(buffer, lang, clear);
    }

    return str;
}

/****
 * NewTranslation:  adds new translation to database
 ****/
int Locale::NewTranslation(const char* str, const genericChar* value)
{
    FnTrace("Locale::NewTranslation()");
    PhraseInfo *ph = Find(str);
    if (ph)
    {
        ph->value.Set(value);
        if (ph->value.size() > 0)
            return 0;

        Remove(ph);
        delete ph;

        if (search_array)
        {
	    free(search_array);
            search_array = nullptr;
            array_size = 0;
        }
        return 0;
    }

    if (value == nullptr || strlen(value) <= 0)
        return 1;

    if (search_array)
    {
	free(search_array);
        search_array = nullptr;
        array_size = 0;
    }
    return Add(new PhraseInfo(str, value));
}

/****
 * TimeDate: returns time/date nicely formated (format flags are in
 * locale.hh)
 ****/
const char* Locale::TimeDate(Settings *s, const TimeInfo &timevar, int format, int lang, genericChar* str)
{
    FnTrace("Locale::TimeDate()");
	// FIX - implement handler for TD_SECONDS format flag
	// Mon Oct  1 13:14:27 PDT 2001: some work done in this direction - JMK

    static genericChar buffer[256];
    static constexpr size_t TIME_DATE_BUFFER_SIZE = 256;
    if (str == nullptr)
        str = buffer;

    if (!timevar.IsSet())
    {
        snprintf(str, TIME_DATE_BUFFER_SIZE, "<NOT SET>");
        return str;
    }

    genericChar tempstr[256];
    str[0] = '\0';

    if (!(format & TD_NO_DAY))
    {
        // Show Day of Week
        int wd = timevar.WeekDay();
        if (format & TD_SHORT_DAY)
            snprintf(str, TIME_DATE_BUFFER_SIZE, "%s", Translate(ShortDayName[wd], lang));
        else
            snprintf(str, TIME_DATE_BUFFER_SIZE, "%s", Translate(DayName[wd], lang));

        if (!(format & TD_NO_TIME) || !(format & TD_NO_DATE))
            strncat(str, ", ", sizeof(str) - strlen(str) - 1);
    }

    if (!(format & TD_NO_DATE))
    {
        // Show Date
        int d = timevar.Day();
        int y = timevar.Year();
        int m = timevar.Month();
        if (format & TD_SHORT_DATE)
        {
            if (s->date_format == DATE_DDMMYY)
            {
                int temp = m;
                m = d;
                d = temp;
            }
	
            if (format & TD_PAD)
                snprintf(tempstr, sizeof(tempstr), "%2d/%2d", m, d);
            else
                snprintf(tempstr, sizeof(tempstr), "%d/%d", m, d);
            strncat(str, tempstr, sizeof(str) - strlen(str) - 1);
            if (!(format & TD_NO_YEAR))
            {
                snprintf(tempstr, sizeof(tempstr), "/%02d", y % 100);
                strncat(str, tempstr, sizeof(str) - strlen(str) - 1);
            }
        }
        else
        {
            if (format & TD_SHORT_MONTH)
            {
                if (format & TD_MONTH_ONLY)
                    snprintf(tempstr, sizeof(tempstr), "%s", Translate(ShortMonthName[m - 1], lang));
                else if (format & TD_PAD)
                    snprintf(tempstr, sizeof(tempstr), "%s %2d", Translate(ShortMonthName[m - 1], lang), d);
                else
                    snprintf(tempstr, sizeof(tempstr), "%s %d", Translate(ShortMonthName[m - 1], lang), d);
            }
            else
            {
                if (format & TD_MONTH_ONLY)
                    snprintf(tempstr, sizeof(tempstr), "%s", Translate(MonthName[m - 1], lang));
                else if (format & TD_PAD)
                    snprintf(tempstr, sizeof(tempstr), "%s %2d", Translate(MonthName[m - 1], lang), d);
                else
                    snprintf(tempstr, sizeof(tempstr), "%s %d", Translate(MonthName[m - 1], lang), d);
            }
            strncat(str, tempstr, sizeof(str) - strlen(str) - 1);

            if (!(format & TD_NO_YEAR))
            {
                snprintf(tempstr, sizeof(tempstr), ", %d", y);
                strncat(str, tempstr, sizeof(str) - strlen(str) - 1);
            }
        }

        if (! (format & TD_NO_TIME))
            strncat(str, " - ", sizeof(str) - strlen(str) - 1);
    }

    if (! (format & TD_NO_TIME))
    {
        int hr = timevar.Hour();
        int minute = timevar.Min();
        int sec = timevar.Sec();
        int pm = 0;

        if (hr >= 12)
            pm = 1;
        hr = hr % 12;
        if (hr == 0)
            hr = 12;

        if (format & TD_PAD)
        {
            if (format & TD_SHORT_TIME)
			{
				if(format & TD_SECONDS)
					snprintf(tempstr, sizeof(tempstr), "%2d:%02d:%2d%c", hr, minute, sec, AMorPM[pm][0]);
				else
					snprintf(tempstr, sizeof(tempstr), "%2d:%02d%c", hr, minute, AMorPM[pm][0]);
			}
            else
			{
				if(format & TD_SECONDS)
					snprintf(tempstr, sizeof(tempstr), "%2d:%02d:%2d %s", hr, minute, sec, AMorPM[pm]);
				else
					snprintf(tempstr, sizeof(tempstr), "%2d:%02d %s", hr, minute, AMorPM[pm]);
			}
        }
        else
        {
            if (format & TD_SHORT_TIME)
			{
				if(format & TD_SECONDS)
					snprintf(tempstr, sizeof(tempstr), "%2d:%02d:%2d%c", hr, minute, sec, AMorPM[pm][0]);
				else
					snprintf(tempstr, sizeof(tempstr), "%d:%02d%c", hr, minute, AMorPM[pm][0]);
			}
            else
			{
				if(format & TD_SECONDS)
					snprintf(tempstr, sizeof(tempstr), "%2d:%02d:%2d %s", hr, minute, sec, AMorPM[pm]);
				else
					snprintf(tempstr, sizeof(tempstr), "%d:%02d %s", hr, minute, AMorPM[pm]);
			}
        }
        strncat(str, tempstr, sizeof(str) - strlen(str) - 1);
    }

    return str;
}

/****
 * Page:  returns nicely formated & translated page numbering
 ****/
char* Locale::Page(int current, int page_max, int lang, genericChar* str)
{
    FnTrace("Locale::Page()");
    static genericChar buffer[32];
    static constexpr size_t PAGE_BUFFER_SIZE = 32;
                                                     // CRITICAL FIX: Prevents buffer overflow vulnerability
                                                     // Problem: sizeof(str) on pointer parameter returns 8 bytes, not buffer size
                                                     // Solution: Use explicit constant matching actual buffer size (32 bytes)
    if (str == nullptr)
        str = buffer;

    if (page_max <= 0)
        snprintf(str, PAGE_BUFFER_SIZE, "%s %d", Translate("Page", lang), current);
    else
        snprintf(str, PAGE_BUFFER_SIZE, "%s %d %s %d", Translate("Page", lang), current,
                Translate("of", lang), page_max);
    return str;
}
