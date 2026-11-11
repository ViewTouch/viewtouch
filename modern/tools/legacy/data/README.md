# The original ViewTouch's data

As the legacy program downloads them on its first start (`manager.cc`):
`vt_data` (its system pages), `zone_db.dat` (the starter menu's pages),
`tables.dat` (its sample dining room) and `menu.dat`, from
www.viewtouch.com. Read by `../vtdata.py`; `../legacy_pages.py` makes
`seed/legacy/*.json` (the Legacy style) and `seed/legacy/NOT-IN-MODERN.md`
from them:

    python3 -I tools/legacy/legacy_pages.py
