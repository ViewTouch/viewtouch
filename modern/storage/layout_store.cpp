#include "storage/layout_store.hh"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

using namespace Qt::StringLiterals;

namespace vt::storage {

namespace {

bool exec(QSqlQuery &q, const QString &sql, QString *error)
{
    if (q.exec(sql))
        return true;
    if (error)
        *error = u"%1: %2"_s.arg(sql.left(60), q.lastError().text());
    return false;
}

QString compact(const QJsonObject &o)
{
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

} // namespace

LayoutStore::LayoutStore(QString databasePath)
    : path_(std::move(databasePath))
    , connection_(u"vt-layout-"_s + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

LayoutStore::~LayoutStore()
{
    {
        QSqlDatabase db = QSqlDatabase::database(connection_, false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(connection_);
}

bool LayoutStore::open(QString *error)
{
    QDir().mkpath(QFileInfo(path_).absolutePath());
    QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, connection_);
    db.setDatabaseName(path_);
    if (!db.open()) {
        if (error)
            *error = db.lastError().text();
        return false;
    }

    QSqlQuery q(db);
    // WAL returns a row; exec() is still fine.
    if (!exec(q, u"PRAGMA journal_mode=WAL"_s, error) || !exec(q, u"PRAGMA synchronous=NORMAL"_s, error))
        return false;

    if (!exec(q, u"CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT NOT NULL)"_s, error))
        return false;
    int version = 0;
    if (q.exec(u"SELECT value FROM meta WHERE key = 'schema_version'"_s) && q.next())
        version = q.value(0).toInt();
    if (version > DbSchemaVersion) {
        if (error)
            *error = u"database schema %1 is newer than this program (%2)"_s.arg(version).arg(DbSchemaVersion);
        return false;
    }

    // Migrations: add a step per version, never edit old ones.
    if (version < 1) {
        if (!db.transaction()) {
            if (error)
                *error = db.lastError().text();
            return false;
        }
        const bool ok =
            exec(q, u"CREATE TABLE layout_theme (id INTEGER PRIMARY KEY CHECK (id = 1), json TEXT NOT NULL)"_s, error)
            && exec(q, u"CREATE TABLE layout_pages (id TEXT PRIMARY KEY, position INTEGER NOT NULL, "
                        "json TEXT NOT NULL)"_s, error)
            && exec(q, u"INSERT OR REPLACE INTO meta (key, value) VALUES ('schema_version', '1')"_s, error);
        if (!ok || !db.commit()) {
            db.rollback();
            return false;
        }
    }
    return true;
}

LayoutStore::StarterState LayoutStore::starterState() const
{
    StarterState s;
    QSqlQuery q(QSqlDatabase::database(connection_));
    if (!q.exec(u"SELECT value FROM meta WHERE key = 'starter_pages'"_s) || !q.next())
        return s;
    const QJsonObject o = QJsonDocument::fromJson(q.value(0).toByteArray()).object();
    for (const QJsonValue &v : o.value(u"seen").toArray())
        s.seen << v.toString();
    const QJsonObject installed = o.value(u"installed").toObject();
    for (auto it = installed.begin(); it != installed.end(); ++it)
        s.installed.insert(it.key(), it.value().toString());
    return s;
}

bool LayoutStore::setStarterState(const StarterState &state, QString *error)
{
    QJsonObject installed;
    for (auto it = state.installed.cbegin(); it != state.installed.cend(); ++it)
        installed.insert(it.key(), it.value());
    const QJsonObject o{{u"seen"_s, QJsonArray::fromStringList(state.seen)}, {u"installed"_s, installed}};
    QSqlQuery q(QSqlDatabase::database(connection_));
    q.prepare(u"INSERT OR REPLACE INTO meta (key, value) VALUES ('starter_pages', ?)"_s);
    q.addBindValue(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
    if (!q.exec()) {
        if (error)
            *error = q.lastError().text();
        return false;
    }
    return true;
}

bool LayoutStore::hasLayout() const
{
    QSqlQuery q(QSqlDatabase::database(connection_));
    return q.exec(u"SELECT COUNT(*) FROM layout_pages"_s) && q.next() && q.value(0).toInt() > 0;
}

std::optional<layout::Layout> LayoutStore::load(QStringList *errors) const
{
    QSqlDatabase db = QSqlDatabase::database(connection_);
    QSqlQuery q(db);
    layout::Layout l;

    if (q.exec(u"SELECT json FROM layout_theme WHERE id = 1"_s) && q.next()) {
        const QJsonObject theme = QJsonDocument::fromJson(q.value(0).toByteArray()).object();
        if (!layout::Layout::checkSchema(theme, u"theme"_s, errors))
            return std::nullopt;
        l.theme = layout::Theme::fromJson(theme);
    }

    if (!q.exec(u"SELECT id, json FROM layout_pages ORDER BY position"_s)) {
        if (errors)
            errors->append(q.lastError().text());
        return std::nullopt;
    }
    while (q.next()) {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(q.value(1).toByteArray(), &err);
        const QString what = u"page '%1'"_s.arg(q.value(0).toString());
        if (!doc.isObject()) {
            if (errors)
                errors->append(u"%1: %2"_s.arg(what, err.errorString()));
            continue;
        }
        if (layout::Layout::checkSchema(doc.object(), what, errors))
            l.pages.append(layout::Page::fromJson(doc.object()));
    }
    if (l.pages.isEmpty())
        return std::nullopt;
    l.syncVariants();   // phone versions' shared zones filled in from their pages
    return l;
}

bool LayoutStore::save(const layout::Layout &layout, QString *error)
{
    QSqlDatabase db = QSqlDatabase::database(connection_);
    if (!db.transaction()) {
        if (error)
            *error = db.lastError().text();
        return false;
    }

    auto fail = [&](const QSqlQuery &q) {
        if (error)
            *error = q.lastError().text();
        db.rollback();
        return false;
    };

    QSqlQuery q(db);
    QJsonObject theme = layout.theme.toJson();
    theme.insert(u"schemaVersion", layout::Layout::SchemaVersion);
    q.prepare(u"INSERT OR REPLACE INTO layout_theme (id, json) VALUES (1, ?)"_s);
    q.addBindValue(compact(theme));
    if (!q.exec())
        return fail(q);

    if (!q.exec(u"DELETE FROM layout_pages"_s))
        return fail(q);
    q.prepare(u"INSERT INTO layout_pages (id, position, json) VALUES (?, ?, ?)"_s);
    int position = 0;
    for (const layout::Page &p : layout.pages) {
        QJsonObject o = p.toJson();
        o.insert(u"schemaVersion", layout::Layout::SchemaVersion);
        q.addBindValue(p.id);
        q.addBindValue(position++);
        q.addBindValue(compact(o));
        if (!q.exec())
            return fail(q);
    }

    if (!db.commit()) {
        if (error)
            *error = db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

} // namespace vt::storage
