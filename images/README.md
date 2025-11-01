# ViewTouch User Images Directory

This directory contains user-provided images that can be used with Image Button zones in ViewTouch.

## Supported Formats
- XPM (.xpm) - Fully supported (native X11 format)
- PNG (.png) - Fully supported (requires libpng)
- JPEG (.jpg, .jpeg) - Fully supported (requires libjpeg)
- GIF (.gif) - Fully supported (requires libgif)

## Dependencies
The system will automatically detect and use available image libraries:
- **libpng-dev**: Required for PNG support
- **libjpeg-dev**: Required for JPEG support
- **libgif-dev**: Required for GIF support

If a library is not installed, the corresponding image format will be gracefully disabled with a warning message.

## Usage
Images placed in this directory will automatically appear in the "Image File" dropdown when creating or editing Image Button zones. Users can select from available images instead of manually typing filenames.

When selected, the image will be loaded and displayed within the button boundaries, automatically scaled to fit while maintaining aspect ratio. Images are centered within the button area.

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
