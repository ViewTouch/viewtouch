#pragma once

#include <QJsonObject>
#include <QLocale>
#include <QString>
#include <QStringList>

#include <functional>

// Languages. The screens' words come from phrase files (i18n/<lang>.json,
// English phrase -> translation; en.json only has English plural forms), applied by a translator installed on the
// app, so qsTr() in QML and tr() in C++ work as usual. A store can add or
// change phrases in <data dir>/translations/<lang>.json (its own button
// labels, say), which win over the built-in ones.
//
// Who sees which language:
// - Each screen: the logged-in person's language, else the store's.
// - On a server, each terminal's session runs its operations and makes its
//   state in that terminal's language (a Scope around them).
// - Guests (the customer display, receipts, kitchen tickets): the store's.
namespace vt::i18n {

struct Language {
    QString code;   // "en"
    QString name;   // in itself: "English", "Español"
};
// English first.
QList<Language> languages();

// Install the translator on the application (once), with the built-in
// phrase files and the store's own from `overridesDir` (may be empty).
void install(const QString &overridesDir = {});

// The language words are looked up in now ("en": none).
QString current();
// This screen's language. Returns whether it changed.
bool setLanguage(const QString &code);
// Dates, weekdays and numbers in a language (the region stays the
// computer's: es in the US writes 3:15 p.m. and 1,234.50). Empty: current().
QLocale locale(const QString &code = {});
// The customer display, receipts and tickets: the store's language.
QString guestLanguage();
void setGuestLanguage(const QString &code);

// For a while (a terminal's operation on the server): another language.
// `language` is asked at each lookup (someone logging in switches it).
class Scope {
public:
    explicit Scope(const QString &code);
    explicit Scope(std::function<QString()> language);
    ~Scope();
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;

private:
    std::function<QString()> previous_;
};

// `english` in `code`, or empty if there is no translation.
QString lookup(const QString &code, const QString &english, int n = -1);
// Text that may be a phrase (a report's cells, a printout's words) in
// `code`: the whole of it, or the words before " (" ("Voided items (3)");
// else as is (amounts, names).
QString phrase(const QString &code, const QString &english);

} // namespace vt::i18n
