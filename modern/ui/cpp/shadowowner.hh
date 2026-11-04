#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

// Qt's RectangularShadow (6.9 to at least 6.12) makes the shader effect it
// draws with before it knows itself, so the effect has no owner and outlives
// it: every shadow taken down (each button of each page left) kept one, about
// 2.7 KB a button, for good. Put inside a RectangularShadow, this gives the
// effect to the shadow, and it goes with it.
//
//     RectangularShadow { id: soft; ShadowOwner { shadow: soft } }
class ShadowOwner : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickItem *shadow READ shadow WRITE setShadow NOTIFY shadowChanged FINAL)

public:
    using QObject::QObject;

    QQuickItem *shadow() const { return shadow_; }
    void setShadow(QQuickItem *shadow);

signals:
    void shadowChanged();

private:
    QPointer<QQuickItem> shadow_;
};
