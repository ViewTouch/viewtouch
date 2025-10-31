# ViewTouch User Images Directory

This directory contains user-provided images that can be used with Image Button zones in ViewTouch.

## Supported Formats
- XPM (.xpm) - Currently fully supported
- PNG (.png)
- JPEG (.jpg, .jpeg)
- GIF (.gif)
- Other common image formats (framework ready for extension)

## Usage
Images placed in this directory will automatically appear in the "Image File" dropdown when creating or editing Image Button zones. Users can select from available images instead of manually typing filenames.

The images will be automatically scaled to fit the button dimensions while maintaining aspect ratio.

## Default Images
The following images are installed by default:
- `coffee.xpm` - Coffee cup icon
- `burger.xpm` - Hamburger icon
- `pizza.xpm` - Pizza icon

## Adding Custom Images
1. Copy your image files to `/usr/viewtouch/imgs/`
2. Supported formats will automatically appear in the Image Button selection dropdown
3. For best results, use square images or images with similar width/height ratios

## Permissions
Only Editor and Super User roles can create and edit Image Button zones.
