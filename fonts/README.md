# ViewTouch Bundled Fonts

This directory contains fonts bundled with ViewTouch to ensure consistent appearance across different systems.

## Font Files

### Sans-Serif Fonts (DejaVu)
- **DejaVuSans.ttf** - Main sans-serif font for UI elements
- **DejaVuSans-Bold.ttf** - Bold variant for emphasis
- **DejaVuSansMono.ttf** - Monospace font for code and fixed-width text
- **DejaVuSansMono-Bold.ttf** - Bold monospace variant

### Serif Fonts (URW Bookman)
- **URWBookman-Light.otf** - Light serif font for elegant text
- **URWBookman-Demi.otf** - Demi-bold variant for emphasis

### Times Replacement (Nimbus Roman)
- **NimbusRoman-Regular.otf** - Times New Roman replacement
- **NimbusRoman-Bold.otf** - Bold Times replacement

## Font Configuration

The `fonts.conf` file contains Fontconfig rules that:
- Add this directory to the font search path
- Create aliases for common font names (Times New Roman → Nimbus Roman)
- Enable subpixel rendering and antialiasing for better quality

## Installation

Fonts are automatically installed to `/usr/viewtouch/fonts/` when ViewTouch is built and installed.

## Font Usage in ViewTouch

- **FONT_TIMES_***: Uses Nimbus Roman (Times New Roman replacement)
- **FONT_COURIER_***: Uses DejaVu Sans Mono (monospace)
- **FONT_GARAMOND_***: Uses URW Bookman (elegant serif)
- **FONT_DEJAVU_***: Uses DejaVu Sans (modern sans-serif)

## License

All fonts in this directory are free and open source:
- DejaVu fonts: Public Domain / Bitstream Vera License
- URW fonts: GPL / Adobe License
- Nimbus fonts: GPL / Adobe License 