#pragma once

#include <QJsonArray>
#include <QString>
#include <QStringList>

namespace vt::layout::schema {

// Editable-field descriptions that drive the editor's inspector. Each field is
// a JSON object:
//
//   path         dotted path into the zone/page/theme JSON ("style.normal.fill")
//   label        human-readable name
//   type         string|text|int|bool|enum|color|texture|font|page|pageList|actions
//   group        section heading in the inspector
//   options      [{value, text}] for enum
//   min, max     for int
//   inheritable  true when "unset" means "inherit from page/template/theme"
//   hint         short help text
//
// Adding a property to the UI means adding a line here; no QML changes.

// Kinds that render as plain elements; everything else is a widget panel.
QStringList basicKinds();
QStringList widgetKinds();
QStringList allKinds();
bool isWidgetKind(const QString &kind);

QStringList pageKinds();
QStringList pageRoles();
// [{value, text}]: the kinds and roles by their names on screen (English).
QJsonArray pageKindOptions();
QJsonArray pageRoleOptions();

QJsonArray zoneFields(const QString &kind);

// A widget's own buttons (id, label), for showing, hiding and renaming them
// (props.buttons.<id>.hide / .label); each also has a command, so a regular
// button can do the same anywhere on the page.
struct BuiltIn {
    QString id;
    QString label;
    QString command;   // the action command that does the same ("" = none)
    // Can be moved among the buttons of its row (props.buttons.<id>.order).
    bool orderable = false;
};
QList<BuiltIn> builtInButtons(const QString &kind);
QJsonArray pageFields();
QJsonArray themeFields();

// Action types for the action-list editor: [{type, label, fields: [...]}].
QJsonArray actionTypes();

} // namespace vt::layout::schema
