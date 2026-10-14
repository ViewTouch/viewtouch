// PosService: the Changes report: who changed what on the manager screens,
// the Menu Builder and the pages.

#include "app/pos_admin_shared.hh"

#include <QDateTime>
#include <QHash>
#include <QLocale>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTimer>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

using namespace admin;

// --- the Changes report ---------------------------------------------------------------

void PosService::logChange(const QString &area, const QString &item, const QString &field, const QString &before,
                           const QString &after)
{
    Change c{++s_->lastChangeId, now(), s_->day.id, user() ? user()->name : ss(terminal_), ss(area), ss(item),
             ss(field), ss(before.left(300)), ss(after.left(300))};
    s_->changesToday.push_back(c);
    if (s_->sink)
        s_->sink->saveChange(c);
}

bool PosService::logPageEdits(const QStringList &added, const QStringList &removed, const QStringList &changed)
{
    if (!user())
        return false;
    const QString area = tr("Pages");
    const auto some = [](const QStringList &l) {
        return l.size() <= 8 ? l.join(u", "_s) : tr("%1 and %2 more").arg(l.mid(0, 8).join(u", "_s)).arg(l.size() - 8);
    };
    if (!added.isEmpty())
        logChange(area, some(added), QString(), QString(), tr("Added"));
    if (!removed.isEmpty())
        logChange(area, some(removed), QString(), QString(), tr("Removed"));
    if (!changed.isEmpty())
        logChange(area, some(changed), QString(), QString(), tr("Edited"));
    return true;
}



void PosService::logRecordChanges(const QString &panel, int index, const QVariantList &before, const QVariantMap &saved)
{
    const QVariantList after = adminRecords(panel);
    const QString area = panelName(panel);
    QHash<QString, QVariantMap> fields;
    for (const QVariant &f : adminFields(panel))
        fields.insert(f.toMap().value(u"path"_s).toString(), f.toMap());
    const auto title = [](const QVariantMap &r) { return r.value(u"_title"_s).toString(); };
    const auto find = [&](const QVariantList &list, const QVariantMap &like, int at) -> QVariantMap {
        if (!recordKey(like).isEmpty())
            for (const QVariant &v : list)
                if (recordKey(v.toMap()) == recordKey(like))
                    return v.toMap();
        return at >= 0 && at < list.size() ? list[at].toMap() : QVariantMap();
    };

    if (index < 0 || index >= before.size())
        return;
    const QVariantMap old = before[index].toMap();
    const QVariantMap now = find(after, old, index);
    QStringList keys = old.keys();
    for (const QString &k : now.keys())
        if (!keys.contains(k))
            keys << k;
    const QString item = panel == u"store" || panel == u"taxes" ? QString() : title(now.isEmpty() ? old : now);
    for (const QString &k : keys) {
        if (k.startsWith(u'_') || k.startsWith(u"about.") || k.startsWith(u"screen."))
            continue;
        const QVariantMap f = fields.value(k);
        const QString label = f.value(u"label"_s, k).toString();
        if (k == u"pin" || k == u"stripeSecretKey" || f.value(u"type"_s) == u"password"_s) {
            // Never kept: say it was changed.
            if (!now.value(k).toString().isEmpty() || !old.value(k).toString().isEmpty())
                logChange(area, item, label, QString(), tr("(changed)"));
            continue;
        }
        const QString a = shown(old.value(k), f), b = shown(now.value(k), f);
        // Changed by the person (saving can write a value another way: not a change).
        if (a != b && (!saved.contains(k) || shown(saved.value(k), f) != a))
            logChange(area, item, label, a, b);
    }
}


} // namespace vt::app
