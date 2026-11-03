#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

// A list from the store (a check's lines, the open checks) as a model that
// changes only what changed: given the whole list again, it matches rows
// by their key ("id") and tells the view what was added, removed, moved or
// changed, so the rows on screen stay (a long check didn't redraw every
// line for each item added). Delegates keep `modelData`, as with a list.
//
//     ListView { model: LiveList { rows: pos.lines } }
class LiveList : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList rows READ rows WRITE setRows NOTIFY rowsChanged FINAL)
    Q_PROPERTY(QString key READ key WRITE setKey NOTIFY keyChanged FINAL)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged FINAL)

public:
    using QAbstractListModel::QAbstractListModel;

    QVariantList rows() const { return rows_; }
    void setRows(const QVariantList &rows);
    QString key() const { return key_; }
    void setKey(const QString &key);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void rowsChanged();
    void keyChanged();
    void countChanged();

private:
    QString keyOf(const QVariant &row, int index) const;

    QVariantList rows_;
    QList<QString> keys_;
    QString key_ = QStringLiteral("id");
};
