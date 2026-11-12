# The original ViewTouch's data

As the legacy program downloads them on its first start (`manager.cc`):
`vt_data` (its system pages), `zone_db.dat` (the starter menu's pages),
`tables.dat` (its sample dining room) and `menu.dat`, from
www.viewtouch.com. They are kept here so ViewTouch Modern never downloads
them. ViewTouch reads them with `app/legacy/` (the same reader that brings
a store over from legacy) and makes the Legacy style's pages,
`seed/legacy/*.json`, and `seed/legacy/NOT-IN-MODERN.md`:

    build/vtmodern --legacy-pages tools/legacy/data

`../zone_fields.json`, the fields each kind of legacy button keeps, is made
from the legacy source by `../zone_fields.py`.
