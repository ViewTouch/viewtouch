/*
 * Copyright ViewTouch, Inc., 1995, 1996, 1997, 1998  *  All Rights Reserved
 * Confidential and Proprietary Information
 *
 * zone.hh - revision 224 (9/16/98)
 * Functions for managing zones on a view
 */

#ifndef _ZONE_HH
#define _ZONE_HH

#include "utility.hh"
#include "list_utility.hh"


/**** Definitions ****/
// Page types
#define PAGE_SYSTEM       0   // Hidden, normally unmodifiable System Page
#define PAGE_TABLE        1   // Table layout page
#define PAGE_INDEX        2   // Top level menu page
#define PAGE_ITEM         3   // Menu Item ordering page
#define PAGE_SCRIPTED3    4   // Yet another modifier page
#define PAGE_SCRIPTED     5   // Page in a modifier script
#define PAGE_SCRIPTED2    6   // Alternate modifier page
#define PAGE_TEMPLATE     7   // Viewable System page
#define PAGE_LIBRARY      8   // user page for storing zones
#define PAGE_ITEM2        9   // Alternate item ordering page
#define PAGE_TABLE2       10  // Table page with check detail
#define PAGE_CHECKS       12  // Check list system page
#define PAGE_KITCHEN_VID  13  // List of checks for the cooks
#define PAGE_KITCHEN_VID2 14  // Secondary list of checks for cooks
#define PAGE_BAR1         15  // Bar mode page.
#define PAGE_BAR2         16  // Second bar mode page.

#define PAGE_ID_SETTLEMENT  -20  // Where the Settlement page is in the list.
#define PAGE_ID_TABSETTLE   -85  // Where the Tab Settlement page is

// Zone Selection Behaviors
enum behaviors {
	BEHAVE_NONE    , // Zone doesn't change when selected
	BEHAVE_TOGGLE  , // Zone toggles with each selection
	BEHAVE_BLINK   , // Zone depresses then resets itself
	BEHAVE_SELECT  , // Once selected stay selected
	BEHAVE_DOUBLE  , // Touch twice within time period
	BEHAVE_MISS      // Touch misses zone & hits zones underneath
};

// Zone Frame Appearence
#define ZF_UNCHANGED 0  // no change (or default)
#define ZF_DEFAULT   1
#define ZF_HIDDEN    2  // frame, texture & text all hidden
#define ZF_NONE      3  // no frame
#define ZF_RAISED    10 // raised single frame (auto select best type)
#define ZF_RAISED1   11 // medium raised single frame
#define ZF_RAISED2   12 // lit raised single frame
#define ZF_RAISED3   13 // dark raised single frame
#define ZF_INSET     20 // inset single frame (auto select best type)
#define ZF_INSET1    21 // medium inset single frame
#define ZF_INSET2    22 // lit inset single frame
#define ZF_INSET3    23 // dark inset single frame
#define ZF_DOUBLE    30 // double raised frame (auto select best type)
#define ZF_DOUBLE1   31 // medium double raised frame
#define ZF_DOUBLE2   32 // lit double raised frame
#define ZF_DOUBLE3   33 // dark double raised frame

#define ZF_BORDER            40 // raised & inset frames filled with 'texture'
#define ZF_CLEAR_BORDER      41 // raised & inset frames
#define ZF_SAND_BORDER       42 // raised & inset frames filled with sand
#define ZF_LIT_SAND_BORDER   43 // raised & inset frames filled with lit sand
#define ZF_INSET_BORDER      44 // inset board fill with dark sand
#define ZF_PARCHMENT_BORDER  45 // raised & inset frames filled with parchment
#define ZF_DOUBLE_BORDER     50 // raised twice & inset frames filled with sand
#define ZF_LIT_DOUBLE_BORDER 51 // double_border with lit sand instead

// Page Class Definitions
#define PAGECLASS_NONE   0
#define PAGECLASS_SYSTEM 1
#define PAGECLASS_TABLE  2
#define PAGECLASS_MENU   4
#define PAGECLASS_ALL    7

// Render Update messages (update_flag)
#define RENDER_REDRAW  0  // just redraw zone
#define RENDER_REFRESH 1  // recalculate current data & redraw
#define RENDER_NEW     2  // initialize data view & redraw

// Page Sizes (resolutions)
#define SIZE_640x480   1 
#define SIZE_800x600   2
#define SIZE_1024x600  3
#define SIZE_1024x768  4  // default
#define SIZE_1280x800  5
#define SIZE_1280x1024 6
#define SIZE_1366x768  7
#define SIZE_1440x900  8
#define SIZE_1600x900 9
#define SIZE_1680x1050  10
#define SIZE_1920x1080 11
#define SIZE_1920x1200 12
#define SIZE_2560x1440 13
#define SIZE_2560x1600  14

/**** Types ****/
class Zone;
class Page;
class ZoneDB;
class Terminal;
class SalesItem;
class ItemDB;
class Report;
class InputDataFile;
class OutputDataFile;
class Check;

class Zone : public RegionInfo
{
public:
    // Calculated/State variables
    Zone *next;
    Zone *fore;        // linked list pointers
    Page *page;        // parent page
    short border;      // button edge border pixel width
    short header;      // space at top of zone text
    short footer;      // space at bottom of zone text
    Uchar edit;        // boolean - is zone marked for editing?
    Uchar active;      // boolean - is zone active?
    Uchar update;      // boolean - does zone info need to be updated
    Uchar stay_lit;    // boolean - leave button lit

    // Zone Properties
    short key;         // keyboard shortcut for zone
    Str   name;        // zone name (usually displayed)
    short group_id;    // group id value of zone
    short shadow;      // thickness of shadow
    Uchar behave;      // selection behavior
    Uchar font;        // font id for zone text;
    Uchar frame[3];    // normal/selected/alternate frames
    Uchar texture[3];  // major texture of zone
    Uchar color[3];    // text color
    Uchar image[3];    // image id to appear on button
    short shape;       // shape of zone
    short iscopy;      // whether this zone is a copy

    // Constructor
    Zone();
    // Destructor
    virtual ~Zone() { }

    // Member Functions
    int Draw(Terminal *t, int update_flag);
    int CopyZone(Zone *target);
    int RenderZone(Terminal *t, const genericChar* text, int update_flag);
    // current zone appearance
    int ChangeJumpID(int old_id, int new_id);
    int RenderShadow(Terminal *t);
    int AlterSize(Terminal *t, int wchange, int hchange,
                  int move_x = 0, int move_y = 0);
    int AlterPosition(Terminal *t, int xchange, int ychange);

    virtual int          State(Terminal *t);
    virtual int          Type() = 0;
    virtual Zone        *Copy() = 0;
    virtual int          AcceptSignals() { return 1; }
    virtual int          RenderInit(Terminal *t, int update_flag);
    virtual RenderResult Render(Terminal *t, int update_flag);
    virtual int          RenderInfo(Terminal *t);
    virtual SignalResult Signal(Terminal *t, const genericChar* message);
    virtual SignalResult Keyboard(Terminal *t, int key, int state);
    virtual SignalResult Touch(Terminal *t, int tx, int ty);
    virtual SignalResult Mouse(Terminal *t, int action, int mx, int my);
    virtual int          Update(Terminal *t, int update_message, const genericChar* value);
    virtual int          ShadowVal(Terminal *t);
    virtual const genericChar* TranslateString(Terminal *t);
    virtual int          SetSize(Terminal *t, int width, int height);
    virtual int          SetPosition(Terminal *t, int pos_x, int pos_y);

    // The focus methods allow various zones to switch back to unfocused mode
    // when another zone is touched.  The SearchZone is the primary example of
    // this.  Unlike most methods, these do not return 1 on error.  GainFocus
    // returns 1 if it is willing to take focus, and LoseFocus returns 1 if it
    // is willing to give up focus.  GainFocus normally should only return 1 or 0,
    // but can also do any necessary preparation for taking focus.  LoseFocus
    // should do whatever is necessary to ensure that the zone loses focus
    // (e.g. SearchZone must call Draw(term, 1)).
    virtual int          GainFocus(Terminal *term, Zone *oldfocus) { return 1; }
    virtual int          LoseFocus(Terminal *term, Zone *newfocus) { return 1; }

    virtual int Read(InputDataFile &df, int version) { return 1; }
    // reads zone from file
    virtual int Write(OutputDataFile &df, int version) { return 1; }
    // writes zone to file
    virtual int CanSelect(Terminal *t) { return 1; }
    // boolean - can zone be selected in edit mode?
    virtual int CanEdit(Terminal *t) { return 1; }
    // boolean - can font/appearence/color be changed?
    virtual int CanCopy(Terminal *t) { return 1; }
    // boolean - can zone be copied/moved/deleted?

    virtual int ZoneStates() { return 2; }
    virtual SalesItem *Item(ItemDB *db) { return NULL; }

    // Interface for zone settings (FIX - should be moved to pos_zone module)
    virtual int   *Amount()          { return NULL; }   // generic amount setting
    virtual Str   *Expression()      { return NULL; }   // enable expression
    virtual Str   *FileName()        { return NULL; }   // filename
    virtual Str   *ItemName()        { return NULL; }   // name in ItemDB
    virtual int   *JumpType()        { return NULL; }   // jump type
    virtual int   *JumpID()          { return NULL; }   // target page id
    virtual Str   *Message()         { return NULL; }   // broadcast message
    virtual int   *QualifierType()   { return NULL; }   // qualifier type
    virtual int   *ReportType()      { return NULL; }   // report type
    virtual int   *ReportPrint()     { return NULL; }   // report print option
    virtual Str   *Script()          { return NULL; }   // page script
    virtual Flt   *Spacing()         { return NULL; }   // line spacing
    virtual int   *SwitchType()      { return NULL; }   // setting switch type
    virtual int   *TenderType()      { return NULL; }   // type of tender
    virtual int   *TenderAmount()    { return NULL; }   // cash amount
    virtual int   *Columns()         { return NULL; }   // columns to display
    virtual int   *CustomerType()    { return NULL; }   // customer transaction type
    virtual Check *GetCheck()      { return NULL; }   // check belonging to zone
    virtual int   *CheckDisplayNum() { return NULL; }   // which check to display on kitchen video
    virtual int   *VideoTarget()     { return NULL; }   // which kitchen video target to use
    virtual int   *DrawerZoneType()  { return NULL; }   // pull/balance or drawer select
    virtual int   *Confirm()         { return NULL; }   // for touch,click confirmations
    virtual Str   *ConfirmMsg()      { return NULL; }   // message for confirmation dialogs
};

class Page
{
	DList<Zone> zone_list;

public:
	// Calculated/State Variables
	Page *next, *fore;    // Linked list pointers
	Page *parent_page;    // All parent zones become part of this page
	short width, height;
	int   changed;
	TimeInfo last_update; // time page was last updated

	// Page Properties
	Str   name;        // name of page
	int   id;          // page id
	int   parent_id;   // id of parent page
	short image;       // background image
	short title_color; // titlebar color
	short type;        // page type
	short index;       // index type page belongs to
	short size;        // page size (screen resolution)

	// Default Settings
	short default_font;
	short default_shadow;
	short default_spacing;
	Uchar default_frame[3];
	Uchar default_texture[3];
	Uchar default_color[3];

	// Constructor
	Page();
	virtual ~Page() { }

	// Member Functions
	Zone *ZoneList()    { return zone_list.Head(); }
	Zone *ZoneListEnd() { return zone_list.Tail(); }
	int   ZoneCount()   { return zone_list.Count(); }

	int Init(ZoneDB *zone_db);
	// Initializes page data
	int Add(Zone *tz);
	// Adds zone to page (end of list)
	int AddFront(Zone *tz);
	// Adds zone to front of zone list
	int Remove(Zone *tz);
	// Removes zone from zone list (does not delete)
	int Purge();
	// Removes and deletes all zones from page
	Zone *FindZone(Terminal *t, int x, int y);
	// Finds zone given position
	Zone *FindEditZone(Terminal *t, int x, int y);
	// Finds valid zone for editing given position
	Zone *FindTranslateZone(Terminal *t, int x, int y);
	// Finds valid zone for translating
	int IsZoneOnPage(Zone *tz);
	// Boolean - checks to see if given zone is on this page
	RenderResult Render(Terminal *t, int update_flag, int no_parent = 0);
	// Renders all zones on page
	RenderResult Render(Terminal *t, int update_flag,
                        int x, int y, int w, int h);
	// Renders all zones that are in the given area
	SignalResult Signal(Terminal *t, const genericChar* message, int group_id);
	// Passes signal to all zones on page
	SignalResult Keyboard(Terminal *t, int key, int state);
	// Passes keypress to all zones on page
	int Update(Terminal *t, int update_message, const genericChar* value);
	// Passes update message to all zones on page
	int Class();
	// returns page class (see definitions above)
    int IsStartPage();
	int IsTable();
	// Boolean - is this a table page?
    int IsKitchen();
    int IsBar();

	// Virtual Functions
	virtual Page *Copy() = 0;
	// Returns copy of page and all its zones
	virtual int Read(InputDataFile &df, int version) { return 1; }
	// Reads page data from file
	virtual int Write(OutputDataFile &df, int version) { return 1; }
	// Writes page data to file
};

class ZoneDB
{
    DList<Page> page_list;

public:
    Str   page_path;
    int   table_pages;

    // Application Defaults
    short default_font;
    short default_shadow;
    short default_spacing;
    Uchar default_frame[3];
    Uchar default_texture[3];
    Uchar default_color[3];

    // Constructor
    ZoneDB();

    // Member Functions
    Page *PageList()    { return page_list.Head(); }
    Page *PageListEnd() { return page_list.Tail(); }
    int   PageCount()   { return page_list.Count(); }

    ZoneDB *Copy();
    // Returns copy of ZoneDB with all pages
    int Init();
    int Load(const char* filename);
    int Save(const char* filename, int section);
    int LoadPages(const char* path);
    int SaveChangedPages();
    int ImportPage(const char* filename);
    int ImportPages();
    int ExportPage(Page *page);
    int Add(Page *p);
    int AddUnique(Page *p);
    int Remove(Page *p);
    int Purge();
    int ChangePageID(Page *p, int new_id);
    int IsPageDefined(int page_id, int size);
    int ClearEdit(Terminal *t);
    // Unmarks all zones
    int SizeEdit(Terminal *t, int wchange, int hchange,
                 int move_x = 0, int move_y = 0);
    // changes size of marked zones
    // position is adjusted accordingly if move_x or move_y are non-zero
    int PositionEdit(Terminal *t, int xchange, int ychange);
    // changes position of marged zones
    int CopyEdit(Terminal *t, int modify_x = 0, int modify_y = 0);
    // copies all marked zones to page
    int RelocateEdit(Terminal *t);
    // moves all marked zone to page
    int DeleteEdit(Terminal *term);
    int ToggleEdit(Terminal *t, int toggle);
    // toggles edit flag on all zones
    int ToggleEdit(Terminal *t, int toggle, int rx, int ry, int rw, int rh);
    // toggles zones in region

    Page *FindByID(int id, int max_size = SIZE_1280x1024);
    Page *FindByType(int type, int period = -1, int max_size = SIZE_1280x1024);
    Page *FindByTerminal(int term_type, int period = -1, int max_size = SIZE_1280x1024);
    Page *FirstTablePage(int max_size = SIZE_1280x1024);

    int References(Page *p, int *list, int max, int &count);
    int PageListReport(Terminal *t, int show_system, Report *r);
    int ChangeItemName(const char* old_name, const genericChar* new_name);

    int PrintZoneDB(const char* dest = NULL, int brief = 0);  // for debugging only
};

#endif

