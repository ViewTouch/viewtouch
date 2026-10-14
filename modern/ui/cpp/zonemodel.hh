#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QtQml/qqmlregistration.h>

// Resolved zones of the current page, bottom to top, for PageView's Repeater.
// Styles arrive fully resolved so QML never walks the inheritance chain.
class ZoneModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by LayoutController")

public:
    enum Role {
        ZoneIdRole = Qt::UserRole + 1,
        KindRole,
        ZoneNameRole,
        LabelRole,
        ZoneXRole,
        ZoneYRole,
        ZoneWRole,
        ZoneHRole,
        ShapeRole,
        BehaviorRole,
        ZoneEnabledRole,
        InheritedRole,
        CurrentRole,
        HotkeyRole,
        GroupRole,
        ImagePathRole,
        StyleNormalRole,
        StyleSelectedRole,
        StyleDisabledRole,
        PropsRole,
        SoldOutRole,   // orders an item that is 86'd
        ShowWhenRole,  // show/hide rules (zone "showWhen"): see ZoneItem.qml
        ItemIdRole,    // the menu item it orders (its first action), for "5 left"
        RotationRole,  // turned, in degrees clockwise (zone "rotation")
    };

    using Row = QHash<int, QVariant>;

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(QList<Row> rows);

private:
    QList<Row> rows_;
};
