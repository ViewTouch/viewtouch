# User manual

`manual.html` is the manual; the PDF, the screenshots and their zip are made from it and not kept in git.

```sh
# 1. Screenshots of every screen (a demo-filled store), into shots/
VTM_SHOTS=$PWD/docs/manual/shots QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=rhi QSG_RHI_BACKEND=opengl build/tests/vtm_tests "[manual]"
VTM_SHOTS=/tmp/extra QT_QPA_PLATFORM=offscreen build/tests/vtm_tests   # everything else: the UI tests' own pictures, under their test names
# 2. The PDF
cd docs/manual && soffice --headless --convert-to "pdf:writer_web_pdf_Export" manual.html
```

`[manual]` also takes every page of the starter store in Classic and in Modern, on a standard screen and on a phone, into `shots/pages/{classic,modern,phone-classic,phone-modern}/<page>.png`; they go in `ViewTouch-Screens.zip` beside the PDF.

Many of the manual's pictures (m64-m149) come from the UI tests in step 1's second line, under their own names: `51-find-item.png` is `m100-find-item.png`, `10-display-welcome.png` is `m70-display-welcome.png`, and so on (same name after the number). The `.jpg` ones are their `.png` saved smaller (`magick x.png -strip -quality 85 x.jpg`); `m122-handhelds` is `m122a`-`m122d` side by side, `m99-settle-layouts` the five `50-settle-*` layouts three across.

The PDF is published on the `modern-manual-<version>` GitHub release (linked from the main README). The README's screenshots, in `docs/screenshots/`, are small JPEGs made from these:

```sh
magick docs/manual/shots/m02-tables.png -resize 1200x -strip -quality 82 docs/screenshots/floor-plan.jpg
```
