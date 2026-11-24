# VT Ai benchmark

Menus of our own, made up for the test (no real store's), each with what a
correct import finds: items, prices, sizes and choices. `tools/vtai_bench.py`
runs them through a VT Ai server and scores items found, prices right and
choices right, with the time each took.

Each `NN-name.txt` is the menu as a manager would paste it; `NN-name.json`
is what it should become: `{"items": [{"name", "price", "category",
"sizes": [{"name", "price"}], "groups": ["Group name", ...]}],
"groups": [{"name", "options": [{"name", "price"}]}]}`. Names are compared
loosely (case, punctuation, plurals); prices to the cent.
