#include "app/i18n.hh"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QTranslator>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcI18n, "vt.i18n")

namespace vt::i18n {

namespace {

// phrase -> translation; a plural is {"one": ..., "other": ...}.
QHash<QString, QHash<QString, QJsonValue>> &dictionaries()
{
    static QHash<QString, QHash<QString, QJsonValue>> d;
    return d;
}

QString &screenLanguage()
{
    static QString code = u"en"_s;
    return code;
}

QString &guest()
{
    static QString code = u"en"_s;
    return code;
}

// Set by Scope; empty: the screen's.
std::function<QString()> &scoped()
{
    static std::function<QString()> language;
    return language;
}

void merge(const QString &code, const QString &file)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
        return;
    QJsonParseError error;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll(), &error).object();
    if (error.error != QJsonParseError::NoError) {
        qCWarning(lcI18n).noquote() << "ignoring" << file << ":" << error.errorString();
        return;
    }
    auto &d = dictionaries()[code];
    for (auto it = o.begin(); it != o.end(); ++it)
        d.insert(it.key(), it.value());
}

// Customer-facing QML files: always in the store's language.
bool forGuests(const char *context)
{
    return context && (qstrcmp(context, "CustomerDisplay") == 0 || qstrcmp(context, "CustomerDisplayWindow") == 0);
}

class Translator : public QTranslator {
public:
    bool isEmpty() const override { return false; }
    QString translate(const char *context, const char *sourceText, const char *, int n) const override
    {
        const QString code = forGuests(context) ? guest() : current();
        if (!sourceText)
            return {};
        return lookup(code, QString::fromUtf8(sourceText), n);
    }
};

} // namespace

QList<Language> languages()
{
    return {{u"en"_s, u"English"_s}, {u"es"_s, u"Español"_s}};
}

void install(const QString &overridesDir)
{
    static Translator *translator = nullptr;
    // English too: its plural forms ("1 guest", "2 guests").
    for (const Language &l : languages()) {
        dictionaries().remove(l.code);
        merge(l.code, u":/i18n/%1.json"_s.arg(l.code));
        if (!overridesDir.isEmpty())
            merge(l.code, QDir(overridesDir).filePath(l.code + u".json"_s));
    }
    if (!translator && QCoreApplication::instance()) {
        translator = new Translator;
        QCoreApplication::installTranslator(translator);
    }
}

QString current()
{
    if (!scoped())
        return screenLanguage();
    const QString code = scoped()();
    return code.isEmpty() ? u"en"_s : code;
}

bool setLanguage(const QString &code)
{
    const QString c = code.isEmpty() ? u"en"_s : code;
    if (c == screenLanguage())
        return false;
    screenLanguage() = c;
    // The screen's dates and weekdays too (QML's Qt.locale(), QLocale()).
    QLocale::setDefault(locale(c));
    return true;
}

QLocale locale(const QString &code)
{
    const QLocale wanted(code.isEmpty() ? current() : code);
    const QLocale system = QLocale::system();
    if (wanted.language() == system.language())
        return system;
    return QLocale(wanted.language(), system.territory());
}

QString guestLanguage()
{
    return guest();
}

void setGuestLanguage(const QString &code)
{
    guest() = code.isEmpty() ? u"en"_s : code;
}

Scope::Scope(const QString &code)
    : Scope(std::function<QString()>([code] { return code; }))
{
}

Scope::Scope(std::function<QString()> language)
    : previous_(scoped())
{
    scoped() = std::move(language);
}

Scope::~Scope()
{
    scoped() = previous_;
}

QString lookup(const QString &code, const QString &english, int n)
{
    const auto d = dictionaries().constFind(code);
    if (d == dictionaries().cend())
        return {};
    const auto it = d->constFind(english);
    if (it == d->cend())
        return {};
    if (it->isObject()) {   // plural forms
        const QJsonObject forms = it->toObject();
        return (n == 1 ? forms.value(u"one") : forms.value(u"other")).toString();
    }
    return it->toString();
}

QString phrase(const QString &code, const QString &english)
{
    if (english.isEmpty())
        return english;
    if (const QString t = lookup(code, english); !t.isEmpty())
        return t;
    if (const qsizetype open = english.indexOf(u" ("_s); open > 0) {
        if (const QString head = lookup(code, english.left(open + 2)); !head.isEmpty())
            return head + english.mid(open + 2);
    }
    return english;
}

} // namespace vt::i18n
