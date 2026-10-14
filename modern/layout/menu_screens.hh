#pragma once

#include "layout/layout.hh"

namespace vt::layout {

// A meal page (kind "index") whose category buttons were placed by hand:
// buttons that go to an item page with no self-filling menu on it.
bool hasHandBuiltMenu(const Layout &layout);

// The same layout with self-filling menu screens: on each such meal page,
// its hand-placed category buttons become one Menu categories panel where
// they were (the rest of the page stays), and a menu page (role "menu") is
// added if there's none. The hand-built item pages stay, as pages of their
// own. Unchanged when there's nothing to switch.
Layout withSelfFillingMenu(const Layout &layout);

} // namespace vt::layout
