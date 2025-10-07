# ItemListZone Filter Feature Implementation

## Overview

Enhanced the "Review & Edit Menu Item Properties" page (Admin Index > Operations) to include filtering functionality that allows users to view only specific types of menu items, rather than viewing all types at once.

## Changes Made

### Files Modified

1. **zone/inventory_zone.hh**
   - Added `filter_type` member variable to track current filter state (-1 = show all, or specific ITEM_* type)
   - Added `Signal()` method override to handle filter button interactions
   - Added `Mouse()` method override to handle mouse clicks on filter buttons
   - Added helper methods:
     - `GetItemTypeColor()` - Returns unique color for each item type
     - `GetItemTypeName()` - Returns display name for each item type

2. **zone/inventory_zone.cc**
   - **Constructor**: Initialize `filter_type` to -1 (show all items by default)
   
   - **Render()**: Enhanced to display 7 filter buttons at the top of the page:
     - All (black)
     - Items (default color)
     - Modifiers (dark blue)
     - Non-Track (dark green)
     - Item+Sub (dark red)
     - By Weight (purple)
     - Event Admission (orange)
   
   - **RecordCount()**: Modified to count only items matching the current filter
   
   - **LoadRecord()**: Modified to load the nth record of the filtered type
   
   - **ListReport()**: Modified to display only items matching the current filter, with color-coded text
   
   - **Signal()**: Handles filter change signals (kept for API compatibility)
   
   - **Mouse()**: Detects clicks on filter buttons and changes the active filter
   
   - **GetItemTypeColor()**: Maps each of the 6 item types to a unique color
   
   - **GetItemTypeName()**: Returns user-friendly names for each item type

## Feature Functionality

### Filter Buttons

Seven clickable buttons are displayed at the top of the ItemListZone:

1. **All** - Shows all menu items (default)
2. **Items** - Shows only regular menu items (ITEM_NORMAL)
3. **Modifiers** - Shows only modifiers (ITEM_MODIFIER)
4. **Non-Track** - Shows only non-tracking modifiers (ITEM_METHOD)
5. **Item+Sub** - Shows only items with substitute (ITEM_SUBSTITUTE)
6. **By Weight** - Shows only items priced by weight (ITEM_POUND)
7. **Admission** - Shows only event admission items (ITEM_ADMISSION)

### Visual Indicators

- Active filter button is highlighted/lit
- Each item type is displayed in its unique color throughout the list
- Record count reflects only the filtered items
- Status text shows the current filter and count (e.g., "Modifiers 5 of 12")

### Color Scheme

- **Regular Items**: Default/Black
- **Modifiers**: Dark Blue
- **Non-Tracking Modifiers**: Dark Green
- **Items + Substitute**: Dark Red
- **Priced By Weight**: Purple
- **Event Admission**: Orange

## Benefits

1. **Reduced Clutter**: Users can focus on specific item types without distraction
2. **Easier Navigation**: Faster to find and edit items of a specific type
3. **Visual Differentiation**: Color-coding helps quickly identify item types
4. **Maintained Compatibility**: All existing functionality remains intact when "All" filter is active
5. **Intuitive UI**: Clear button interface for switching between filters

## Testing

The implementation has been successfully compiled and is ready for testing in the ViewTouch application. To test:

1. Build and install: `sudo make install`
2. Launch ViewTouch: `sudo /usr/viewtouch/bin/vtpos`
3. Navigate to Admin Index > Operations > Review & Edit Menu Item Properties
4. Click on the filter buttons at the top to switch between different item type views
5. Verify that only items of the selected type are shown
6. Verify that the item count and record navigation work correctly for filtered views

## Implementation Notes

- The filter state is not persisted between sessions (resets to "All" when reopening the page)
- Filter buttons use standard LayoutZone button rendering for consistency
- Mouse click detection accounts for proper font metrics and positioning
- All modifications follow existing ViewTouch coding patterns and conventions

