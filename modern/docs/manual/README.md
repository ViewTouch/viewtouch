# User manual

`manual.html` is the manual, and `shots/` its pictures, kept in git as JPEG; the PDF and the zip of every screen are made from them and not kept in git.

```sh
# 1. Screenshots of every screen (a demo-filled store), into shots/
VTM_SHOTS=$PWD/docs/manual/shots QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=rhi QSG_RHI_BACKEND=opengl build/tests/vtm_tests "[manual]"
VTM_SHOTS=/tmp/extra QT_QPA_PLATFORM=offscreen build/tests/vtm_tests   # everything else: the UI tests' own pictures, under their test names
# 1b. Saved as JPEG (what the manual and git use): the tests write PNG
find docs/manual/shots -name '*.png' | while read f; do magick "$f" -strip -quality 85 "${f%.png}.jpg" && rm "$f"; done
# 2. The PDF
cd docs/manual && soffice --headless --convert-to "pdf:writer_web_pdf_Export" manual.html
```

`[manual]` also takes every page of the starter store in Classic and in Modern, on a standard screen and on a phone, into `shots/pages/{classic,modern,phone-classic,phone-modern}/<page>.jpg` (after step 1b); they go in `ViewTouch-Screens.zip` beside the PDF.

Many of the manual's pictures (m64-m149) come from the UI tests in step 1's second line, under their own names: `51-find-item.png` is `m100-find-item.jpg`, `10-display-welcome.png` is `m70-display-welcome.jpg`, and so on (same name after the number). `m122-handhelds` is `m122a`-`m122d` side by side, `m99-settle-layouts` the five `50-settle-*` layouts three across.

The PDF is published on the `modern-manual-<version>` GitHub release (linked from the main README). The README's screenshots, in `docs/screenshots/`, are small JPEGs made from these:

```sh
magick docs/manual/shots/m02-tables.jpg -resize 1200x -strip -quality 82 docs/screenshots/floor-plan.jpg
```
