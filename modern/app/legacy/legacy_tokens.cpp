#include "app/legacy/legacy_tokens.hh"

#include "app/xlsx.hh"

#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QTimeZone>

#include <array>

namespace vt::legacy {

namespace {

QString tr(const char *s) { return QCoreApplication::translate("vt::legacy", s); }

constexpr qint64 kLimit = qint64(512) << 20;   // no legacy file is near this

// Legacy's own digits (data_file.cc new_decode_table): -1 for any other byte.
const std::array<signed char, 256> &digits()
{
    static const auto table = [] {
        std::array<signed char, 256> t{};
        t.fill(-1);
        const char *d = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; ++i)
            t[static_cast<unsigned char>(d[i])] = static_cast<signed char>(i);
        return t;
    }();
    return table;
}

bool space(char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\v' || c == '\f'; }

} // namespace

std::optional<QByteArray> gunzip(const QByteArray &file, qint64 limit)
{
    if (file.size() < 2 || uchar(file[0]) != 0x1f || uchar(file[1]) != 0x8b)
        return file.size() > limit ? std::nullopt : std::optional<QByteArray>(file);
    // RFC 1952: a 10-byte header, then what its flags say is there.
    if (file.size() < 18 || uchar(file[2]) != 8)
        return std::nullopt;
    const int flags = uchar(file[3]);
    qsizetype at = 10;
    if (flags & 4) {   // FEXTRA
        if (at + 2 > file.size())
            return std::nullopt;
        at += 2 + (uchar(file[at]) | (uchar(file[at + 1]) << 8));
    }
    for (const int bit : {8, 16}) {   // FNAME, FCOMMENT: zero-ended
        if (!(flags & bit))
            continue;
        while (at < file.size() && file[at] != '\0')
            ++at;
        ++at;
    }
    if (flags & 2)   // FHCRC
        at += 2;
    if (at >= file.size())
        return std::nullopt;
    return vt::app::inflate(file.mid(at), limit);
}

std::optional<Tokens> Tokens::open(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = tr("%1 can't be read (%2).").arg(QFileInfo(path).fileName(), f.errorString());
        return std::nullopt;
    }
    return fromBytes(f.readAll(), QFileInfo(path).fileName(), error);
}

std::optional<Tokens> Tokens::fromBytes(const QByteArray &bytes, const QString &name, QString *error)
{
    const std::optional<QByteArray> text = gunzip(bytes, kLimit);
    if (!text) {
        if (error)
            *error = tr("%1 is damaged (it doesn't unpack).").arg(name);
        return std::nullopt;
    }
    Tokens t;
    t.name_ = name;
    const char *p = text->constData();
    const qsizetype n = text->size();
    for (qsizetype i = 0; i < n;) {
        while (i < n && space(p[i]))
            ++i;
        const qsizetype start = i;
        while (i < n && !space(p[i]))
            ++i;
        if (i > start)
            t.words_.append(QByteArray(p + start, i - start));
    }
    bool ok = false;
    if (t.words_.size() >= 3 && t.words_[0] == "vtpos")
        t.version_ = t.words_[2].toInt(&ok);
    if (!ok) {
        if (error)
            *error = tr("%1 isn't a ViewTouch data file.").arg(name);
        return std::nullopt;
    }
    t.at_ = 3;
    return t;
}

void Tokens::fail(const QString &why)
{
    if (error_.isEmpty())
        error_ = tr("%1: %2 (at word %3 of %4).").arg(name_, why).arg(at_).arg(words_.size());
}

const QByteArray *Tokens::next()
{
    if (at_ >= words_.size()) {
        fail(tr("it ends early"));
        return nullptr;
    }
    return &words_[at_++];
}

std::int64_t Tokens::i64()
{
    const QByteArray *w = next();
    if (!w)
        return 0;
    quint64 v = 0;
    for (const char c : *w) {
        const int d = digits()[static_cast<unsigned char>(c)];
        if (d < 0) {
            --at_;
            fail(tr("\"%1\" isn't a number").arg(QString::fromLatin1(*w)));
            ++at_;
            return 0;
        }
        v = (v << 6) + quint64(d);
    }
    return static_cast<std::int64_t>(v);
}

int Tokens::i()
{
    // Legacy reads an int as the 64-bit value cut to 32 (-1 is "P//////////").
    return static_cast<qint32>(static_cast<quint32>(static_cast<quint64>(i64())));
}

double Tokens::f()
{
    const QByteArray *w = next();
    if (!w)
        return 0;
    bool ok = false;
    const double v = w->toDouble(&ok);
    if (!ok) {
        --at_;
        fail(tr("\"%1\" isn't a decimal").arg(QString::fromLatin1(*w)));
        ++at_;
        return 0;
    }
    return v;
}

QString Tokens::s()
{
    const QByteArray *w = next();
    if (!w || *w == "~")
        return {};
    QByteArray b = *w;
    b.replace('_', ' ');
    // Legacy wrote the bytes it was given: UTF-8 now, Latin-1 in older stores.
    const QString u = QString::fromUtf8(b);
    return u.contains(QChar::ReplacementCharacter) ? QString::fromLatin1(b) : u;
}

WallTime Tokens::t()
{
    const int seconds = i();
    const int year = i();
    if (seconds == 0 && year == 0)
        return {};
    // Legacy: local_days{year/January/1} + seconds, by the store's clock.
    const QDateTime wall = QDateTime(QDate(year, 1, 1), QTime(0, 0), QTimeZone::UTC).addSecs(seconds);
    if (!wall.isValid()) {
        fail(tr("a date that isn't one"));
        return {};
    }
    return {wall.date(), wall.time()};
}

void Tokens::skip(int words)
{
    for (int k = 0; k < words; ++k)
        next();
}

bool Tokens::finished()
{
    if (ok() && !atEnd())
        fail(tr("%1 words left over").arg(words_.size() - at_));
    return ok();
}

} // namespace vt::legacy
