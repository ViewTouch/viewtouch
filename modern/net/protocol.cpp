#include "net/protocol.hh"

#include <QJsonDocument>

namespace vt::net {

void LineChannel::send(const QJsonObject &message)
{
    QByteArray line = QJsonDocument(message).toJson(QJsonDocument::Compact);
    line.append('\n');
    // Each message goes out now (no waiting to fill a packet): otherwise a
    // touch's answer can sit ~40 ms waiting for the other side's ack.
    if (!lowDelay_ && socket_->state() == QAbstractSocket::ConnectedState) {
        socket_->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        lowDelay_ = true;
    }
    socket_->write(line);
}

void LineChannel::sendRaw(const QByteArray &json)
{
    if (!lowDelay_ && socket_->state() == QAbstractSocket::ConnectedState) {
        socket_->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        lowDelay_ = true;
    }
    socket_->write(json + '\n');
}

QList<QJsonObject> LineChannel::receive(bool *overflow)
{
    buffer_.append(socket_->readAll());
    QList<QJsonObject> out;
    qsizetype start = 0;
    for (qsizetype nl = buffer_.indexOf('\n'); nl >= 0; nl = buffer_.indexOf('\n', start)) {
        const QJsonDocument doc = QJsonDocument::fromJson(buffer_.sliced(start, nl - start));
        if (doc.isObject())
            out.append(doc.object());
        start = nl + 1;
    }
    buffer_.remove(0, start);
    if (overflow)
        *overflow = buffer_.size() > MaxMessageBytes;
    return out;
}

} // namespace vt::net
