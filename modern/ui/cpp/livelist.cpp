#include "livelist.hh"

#include <QVariantMap>

using namespace Qt::StringLiterals;

namespace {
constexpr int ModelDataRole = Qt::UserRole + 1;
}

QString LiveList::keyOf(const QVariant &row, int index) const
{
    // A row without the key (a line still waiting for the server): only
    // ever the same row as the one at its place.
    const QVariant k = row.toMap().value(key_);
    return k.isValid() && !k.toString().isEmpty() ? k.toString() : u"#"_s + QString::number(index);
}

void LiveList::setKey(const QString &key)
{
    if (key == key_)
        return;
    key_ = key;
    emit keyChanged();
    const QVariantList again = rows_;
    beginResetModel();
    rows_.clear();
    keys_.clear();
    endResetModel();
    setRows(again);
}

void LiveList::setRows(const QVariantList &next)
{
    QList<QString> wanted;
    wanted.reserve(next.size());
    for (qsizetype i = 0; i < next.size(); ++i)
        wanted.append(keyOf(next[i], int(i)));
    const int before = int(rows_.size());

    // Gone: removed, from the end so the places before stay.
    for (int i = int(keys_.size()) - 1; i >= 0; --i)
        if (!wanted.contains(keys_[i])) {
            beginRemoveRows({}, i, i);
            rows_.removeAt(i);
            keys_.removeAt(i);
            endRemoveRows();
        }
    // Then place by place: the same row (changed or not), one from further
    // down moved up, or a new one.
    for (int i = 0; i < int(wanted.size()); ++i) {
        if (i < int(keys_.size()) && keys_[i] == wanted[i]) {
            if (rows_[i] != next[i]) {
                rows_[i] = next[i];
                emit dataChanged(index(i), index(i), {ModelDataRole});
            }
            continue;
        }
        const int from = int(keys_.indexOf(wanted[i], i));
        if (from > i) {
            beginMoveRows({}, from, from, {}, i);
            rows_.move(from, i);
            keys_.move(from, i);
            endMoveRows();
            if (rows_[i] != next[i]) {
                rows_[i] = next[i];
                emit dataChanged(index(i), index(i), {ModelDataRole});
            }
        } else {
            beginInsertRows({}, i, i);
            rows_.insert(i, next[i]);
            keys_.insert(i, wanted[i]);
            endInsertRows();
        }
    }
    emit rowsChanged();
    if (int(rows_.size()) != before)
        emit countChanged();
}

int LiveList::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(rows_.size());
}

QVariant LiveList::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= rows_.size() || role != ModelDataRole)
        return {};
    return rows_[index.row()];
}

QHash<int, QByteArray> LiveList::roleNames() const
{
    return {{ModelDataRole, "modelData"}};
}
