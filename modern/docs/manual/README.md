# User manual

`manual.html` is the manual; the PDF, the screenshots and their zip are made from it and not kept in git.

```sh
# 1. Screenshots of every screen (a demo-filled store), into shots/
VTM_SHOTS=$PWD/docs/manual/shots QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=rhi QSG_RHI_BACKEND=opengl build/tests/vtm_tests "[manual]"
VTM_SHOTS=/tmp/extra QT_QPA_PLATFORM=offscreen build/tests/vtm_tests "[display][ui]"   # customer display (m70-m73)
# 2. The PDF
cd docs/manual && soffice --headless --convert-to "pdf:writer_web_pdf_Export" manual.html
```

The PDF is published on the `modern-manual-<version>` GitHub release (linked from the main README). The README's screenshots, in `docs/screenshots/`, are small JPEGs made from these:

```sh
magick docs/manual/shots/m02-tables.png -resize 1200x -strip -quality 82 docs/screenshots/floor-plan.jpg
```
