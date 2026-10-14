#pragma once

#include <QByteArray>
#include <QString>

// Encrypted ("sealed") backup files: AES-256-GCM with a key made from the
// store's backup password (PBKDF2-HMAC-SHA256, 600,000 rounds). The crypto
// is the system's OpenSSL (libcrypto, loaded at run time).
//
// A sealed file: "VTMSEAL1" | salt (16) | nonce (12) | ciphertext | tag (16).
// The header is authenticated too, so any change to the file is caught.
namespace vt::storage {

// Whether this computer can seal and open files; `why` says what's missing.
bool sealingAvailable(QString *why = nullptr);

// The key for `password` with `salt` (16 random bytes, kept with the key).
QByteArray sealingKey(const QString &password, const QByteArray &salt);
QByteArray newSalt();

// Encrypt `in` into `out` (which must not exist).
bool sealFile(const QString &in, const QString &out, const QByteArray &key, const QByteArray &salt,
              QString *error = nullptr);
bool isSealed(const QString &file);
// Decrypt a sealed `in` into `out` with the password (or the key).
bool openSealedFile(const QString &in, const QString &out, const QString &password, QString *error = nullptr);
bool openSealedFileWithKey(const QString &in, const QString &out, const QByteArray &key, QString *error = nullptr);

} // namespace vt::storage
