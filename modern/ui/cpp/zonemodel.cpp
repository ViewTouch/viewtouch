#include "zonemodel.hh"

int ZoneModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(rows_.size());
}

QVariant ZoneModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    return rows_[index.row()].value(role);
}

QHash<int, QByteArray> ZoneModel::roleNames() const
{
    return {
        {ZoneIdRole, "zoneId"},
        {KindRole, "kind"},
        {ZoneNameRole, "zoneName"},
        {LabelRole, "label"},
        {ZoneXRole, "zoneX"},
        {ZoneYRole, "zoneY"},
        {ZoneWRole, "zoneW"},
        {ZoneHRole, "zoneH"},
        {ShapeRole, "shape"},
        {BehaviorRole, "behavior"},
        {ZoneEnabledRole, "zoneEnabled"},
        {InheritedRole, "inherited"},
        {CurrentRole, "current"},
        {HotkeyRole, "hotkey"},
        {GroupRole, "group"},
        {ImagePathRole, "imagePath"},
        {StyleNormalRole, "styleNormal"},
        {StyleSelectedRole, "styleSelected"},
        {StyleDisabledRole, "styleDisabled"},
        {PropsRole, "props"},
        {SoldOutRole, "soldOut"},
        {ShowWhenRole, "showWhen"},
        {ItemIdRole, "itemId"},
        {RotationRole, "zoneRotation"},
    };
}

void ZoneModel::setRows(QList<Row> rows)
{
    // The same zones (a highlight or sold-out mark changed): updated in
    // place. A reset rebuilds every zone on the page, and its panels lose
    // where they were scrolled to (the Choose page jumped back to the top
    // when a No/Extra key went off).
    const auto same = [&] {
        if (rows.size() != rows_.size())
            return false;
        for (qsizetype i = 0; i < rows.size(); ++i)
            if (rows[i].value(ZoneIdRole) != rows_[i].value(ZoneIdRole) || rows[i].value(KindRole) != rows_[i].value(KindRole))
                return false;
        return true;
    };
    if (!same()) {
        beginResetModel();
        rows_ = std::move(rows);
        endResetModel();
        return;
    }
    for (qsizetype i = 0; i < rows.size(); ++i) {
        if (rows[i] == rows_[i])
            continue;
        QList<int> roles;
        for (auto it = rows[i].cbegin(); it != rows[i].cend(); ++it)
            if (rows_[i].value(it.key()) != it.value())
                roles << it.key();
        rows_[i] = std::move(rows[i]);
        emit dataChanged(index(int(i)), index(int(i)), roles);
    }
}
