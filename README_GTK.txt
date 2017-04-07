This README_GTK.txt explains how to install the GTK3+ version of the viewtouch launcher "vtpos".
This commit included 5 files:
- README_GTK.txt (this file)
- CMakeListsGTK.txt 
- loader/loader_main_GTK.cc
- vtlogo.xpm
- vtpos.css

The purpose of this commit was to demonstrate some features of using the modern graphics toolkit, GTK3+, compared to Xlib.  Implementing viewtouch with GTK3+ in its entirety is an enormous project with tremendous rewards.  Some obvious benefits are the ability to easily add graphics to any widget, e.g. a menu button for the calamari would be a picture of the restaurant's plated creation instead of a textured pattern.  Also, all the true type fonts are easily available with GTK3+ and not possible using Xlib.  A great benefit is the use of CSS by GTK3+.  With CSS, styling files can be used and read into the viewtouch POS.  This allows graphically styling changes such as updating the picture on a specific button without having to recompile any code.

The vtpos executable is certainly the most compact of the viewtouch executables and was fairly easy to convert to GTK3+.  I assume you already know how to install viewtouch if you are reading this - if not checkout the Github viewtouch wiki.  The CSS can be embedded in the source code or included in an external file.  The embedded CSS is coded into loader_main_GTK.cc but is commented out.  To see that either way works identically, remove the comment markers from the embedded code and comment out the line to import from the file.

1. Rename your existing CMakeLists.txt as something else like CMakeListsOrig.txt
2. Rename CMakeListsGTK.txt as CMakeLists.txt
3. Ensure the loader_main_GTK.cc source code is in the viewtouch/loader/ folder.
4. Place the official viewtouch logo, vtlogo.xpm, and the cascading styling sheet file, vtpos.css, in the usr/viewtouch/dat/ folder.
5. Rebuild viewtouch.  
