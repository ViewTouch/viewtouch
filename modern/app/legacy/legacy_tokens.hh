#pragma once

#include <QByteArray>
#include <QDate>
#include <QList>
#include <QString>
#include <QTime>

#include <cstdint>
#include <optional>

// Reading the original ViewTouch's data files, by its own rules
// (legacy src/core/data_file.cc): a file starts "vtpos 0 <version>", may be
// gzipped, and is a stream of words split by any white space, line breaks
// included. A number is written in base 64 (A-Z a-z 0-9 + /, a negative one
// as 64 bits), a decimal as text ("0.083"), a text with "_" for each space
// ("~" when empty), and a date as two numbers: seconds into the year (by the
// store's clock) and the year.
namespace vt::legacy {

// A date and time as the store's clock showed it (legacy kept no time zone).
struct WallTime {
    QDate date;
    QTime time;
    bool isSet() const { return date.isValid(); }
    bool operator==(const WallTime &) const = default;
};

class Tokens {
public:
    // A file's words (gzipped or not); nullopt and `error` when it can't be
    // read or isn't one of legacy's ("vtpos 0 <version>" first).
    static std::optional<Tokens> open(const QString &path, QString *error);
    static std::optional<Tokens> fromBytes(const QByteArray &bytes, const QString &name, QString *error);

    int version() const { return version_; }
    QString name() const { return name_; }

    // The next word as each kind of value. A word missing, or one that
    // isn't that kind, is remembered as the error (see ok()); the reading
    // then gives zeros and empty texts, so a reader needn't stop at each.
    int i();                 // a legacy int (32 bits)
    std::int64_t i64();      // a legacy 64-bit number
    double f();              // a legacy Flt
    QString s();             // a legacy Str
    WallTime t();            // a legacy TimeInfo
    void skip(int words);    // words of no use here

    bool ok() const { return error_.isEmpty(); }
    QString error() const { return error_; }
    // Read to the end, every word used: what a reader checks last.
    bool atEnd() const { return at_ >= words_.size(); }
    qsizetype position() const { return at_; }
    qsizetype size() const { return words_.size(); }
    // Says what went wrong, at which word (the first error is kept).
    void fail(const QString &why);
    // ok() and nothing left over; else the error, or "n words left".
    bool finished();

private:
    const QByteArray *next();
    QString name_;
    QList<QByteArray> words_;
    qsizetype at_ = 0;
    int version_ = 0;
    QString error_;
};

// A gzip file's contents (a plain file as it is); nullopt when damaged or
// over `limit` bytes.
std::optional<QByteArray> gunzip(const QByteArray &file, qint64 limit);

} // namespace vt::legacy
