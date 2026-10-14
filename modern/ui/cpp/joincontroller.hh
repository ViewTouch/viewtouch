#pragma once

#include "net/discovery.hh"
#include "net/pairing.hh"

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// The "Join a store" screen of a terminal that is not paired yet: finds the
// servers on the network, and pairs with one using the code a manager
// starts in Manager -> Terminals.
class JoinController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Made by main()")

    // [{id, name, machine, host, port}]
    Q_PROPERTY(QVariantList servers READ servers NOTIFY serversChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString suggestedName READ suggestedName CONSTANT)
    Q_PROPERTY(QString address READ address CONSTANT)
    Q_PROPERTY(QString problem READ problem CONSTANT)

public:
    // `address`: a server given on the command line (host[:port]), if any.
    // `problem`: why joining is needed again (e.g. this device was unpaired).
    JoinController(QString suggestedName, QString address, QString problem, QObject *parent = nullptr);

    QVariantList servers() const;
    bool busy() const { return pairer_.busy(); }
    QString error() const { return error_; }
    QString suggestedName() const { return suggestedName_; }
    QString address() const { return address_; }
    QString problem() const { return problem_; }
    const vt::net::Credentials &credentials() const { return credentials_; }

    // Look again (it also looks every few seconds on its own).
    Q_INVOKABLE void search();
    // Pair with the server at `address` (host or host:port) using `code`.
    Q_INVOKABLE void join(const QString &address, const QString &code, const QString &name);

signals:
    void serversChanged();
    void busyChanged();
    void errorChanged();
    void joined();

private:
    void setError(const QString &e);

    QString suggestedName_;
    QString address_;
    QString problem_;
    QString error_;
    vt::net::ServerFinder finder_;
    vt::net::Pairer pairer_;
    QTimer searchTimer_;
    vt::net::Credentials credentials_;
};
