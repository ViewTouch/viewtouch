#include "net/protocol.hh"

#include <QJsonDocument>
#include <QMetaObject>
#include <QTcpSocket>

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
    // Only what came since the last look: a big message arriving in many
    // pieces isn't searched again from its start each time.
    for (qsizetype nl = buffer_.indexOf('\n', searched_); nl >= 0; nl = buffer_.indexOf('\n', start)) {
        const QJsonDocument doc = QJsonDocument::fromJson(buffer_.sliced(start, nl - start));
        if (doc.isObject())
            out.append(doc.object());
        start = nl + 1;
    }
    buffer_.remove(0, start);
    searched_ = buffer_.size();
    const bool tooLong = buffer_.size() > MaxMessageBytes;
    if (overflow)
        *overflow = tooLong;
    else if (tooLong) {
        buffer_.clear();
        searched_ = 0;
        // Nothing sends a line that long. Cut off once the caller is done
        // with this answer (closing can delete the channel it is using).
        QMetaObject::invokeMethod(socket_, &QTcpSocket::abort, Qt::QueuedConnection);
    }
    return out;
}

} // namespace vt::net
