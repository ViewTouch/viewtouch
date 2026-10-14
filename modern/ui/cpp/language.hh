#pragma once

class QQmlEngine;
class LayoutController;

namespace vt::ui {

// The screen speaks the language of whoever is logged in (or the store's):
// switches the translator and re-translates the QML on each change.
void followLanguage(QQmlEngine *engine, LayoutController *controller);

} // namespace vt::ui
