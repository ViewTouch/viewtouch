#pragma once

// VT Ai: the small AI on the store's computer (vtmodern-ai.service, llama.cpp's
// server on 127.0.0.1:7720). This is ViewTouch talking to it: one question at
// a time, never blocking the screen, each answered once with the reply or why
// not. Nothing here goes beyond this computer.

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>

#include <functional>

namespace vt::net {

class VtAiClient : public QObject {
    Q_OBJECT

public:
    using Done = std::function<void(const QJsonObject &reply, const QString &error)>;
    explicit VtAiClient(QObject *parent = nullptr);

    // Where it listens (VTM_VTAI_URL for a test or another port).
    void setBaseUrl(const QUrl &url) { base_ = url; }
    QUrl baseUrl() const { return base_; }

    // A chat completion ({messages, response_format...}): the server's reply.
    // Long answers take minutes on a small computer: `timeoutMs`.
    void chat(const QJsonObject &body, Done done, int timeoutMs = 10 * 60 * 1000);
    // The one being answered, given up (its Done says "Canceled.").
    void cancel();
    bool busy() const { return !current_.isNull(); }

    // Is it there, and which model: checked now and every half minute.
    bool running() const { return running_; }
    QString model() const { return model_; }
    void checkNow();

signals:
    void statusChanged();

private:
    QUrl base_;
    QNetworkAccessManager net_;
    QPointer<QNetworkReply> current_;
    QTimer health_;
    bool running_ = false;
    QString model_;
};

} // namespace vt::net
