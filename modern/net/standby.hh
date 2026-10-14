#pragma once

#include "net/discovery.hh"
#include "net/pairing.hh"

#include <QObject>
#include <QSslServer>
#include <QSslSocket>
#include <QTimer>

#include <memory>

namespace vt::storage { class AsyncWriter; }

// The store's standby server: a second computer with a live copy of the
// database, ready to take over if the main one stops.
//
// - ReplicaClient keeps the copy: it connects to the main server with the
//   store's server key ("r:<store id>"), receives the whole database once,
//   then every change as it is saved.
// - StandbyListener lets a screen ask it to take over: a paired terminal
//   (its key is in the copy) sends a manager's PIN. Taking over is a
//   person's decision on purpose: a standby can't tell a dead main server
//   from a broken cable, and two servers taking orders would split the
//   store's sales.
namespace vt::net {

class ReplicaClient : public QObject {
    Q_OBJECT

public:
    ReplicaClient(QString databasePath, Credentials credentials, QObject *parent = nullptr);
    ~ReplicaClient() override;

    void start();
    // For good (on taking over): saves what arrived, and disconnects.
    void stop();
    // Wait until the changes received so far are in the copy.
    void flush();
    bool inSync() const { return inSync_; }
    // Milliseconds since the main server was last heard from (0: never).
    qint64 lastHeard() const { return lastHeard_; }
    QString status() const { return status_; }
    const Credentials &credentials() const { return credentials_; }

signals:
    void statusChanged();
    // The first full copy (or a fresh one after reconnecting) is in place.
    void synced();
    // The main server moved (found again by the store's id).
    void credentialsChanged(const vt::net::Credentials &credentials);

private:
    void connectNow();
    void onReadyRead();
    void handle(const QJsonObject &m);
    void installSnapshot();
    void setStatus(const QString &status);

    QString db_;
    Credentials credentials_;
    QSslSocket socket_;
    std::unique_ptr<LineChannel> channel_;
    std::unique_ptr<storage::AsyncWriter> writer_;
    ServerFinder finder_;
    QTimer retry_;
    QByteArray snapshot_;
    int failures_ = 0;
    bool inSync_ = false;
    bool stopped_ = false;
    qint64 lastHeard_ = 0;
    QString status_;
};

// Answers "take over" from a paired screen with a manager's PIN.
class StandbyListener : public QObject {
    Q_OBJECT

public:
    StandbyListener(QString databasePath, QString storeId, QObject *parent = nullptr);

    bool listen(quint16 port);
    quint16 port() const { return server_.serverPort(); }
    QString errorString() const { return server_.errorString(); }
    // Whether takeover is allowed now (the copy is complete).
    void setReady(bool ready) { ready_ = ready; }

signals:
    // A manager asked; reply when the takeover has begun.
    void takeOverRequested(const QString &by);

private:
    QString db_;
    QString storeId_;
    QSslServer server_;
    bool ready_ = false;
};

// A screen asks the standby at host:port to take over, with a manager's
// PIN. `finished` says whether it agreed.
class TakeOverRequest : public QObject {
    Q_OBJECT

public:
    TakeOverRequest(const Credentials &terminal, QString host, quint16 port, QString pin, QObject *parent = nullptr);

signals:
    void finished(bool ok, const QString &message);

private:
    QSslSocket socket_;
    std::unique_ptr<LineChannel> channel_;
    QString pin_;
    bool done_ = false;
};

} // namespace vt::net
