#include "storage/sealed.hh"

#include <QFile>
#include <QLibrary>
#include <QSaveFile>

using namespace Qt::StringLiterals;

namespace vt::storage {

namespace {

constexpr QByteArrayView Magic = "VTMSEAL1";
constexpr int SaltBytes = 16;
constexpr int NonceBytes = 12;
constexpr int TagBytes = 16;
constexpr int KeyBytes = 32;
constexpr int Rounds = 600'000;

// The few libcrypto (OpenSSL 3) functions used; their ABI is stable.
struct Crypto {
    using Ctx = void;
    using Cipher = void;
    using Md = void;
    Ctx *(*ctxNew)() = nullptr;
    void (*ctxFree)(Ctx *) = nullptr;
    const Cipher *(*aes256gcm)() = nullptr;
    const Md *(*sha256)() = nullptr;
    int (*encryptInit)(Ctx *, const Cipher *, void *, const unsigned char *, const unsigned char *) = nullptr;
    int (*encryptUpdate)(Ctx *, unsigned char *, int *, const unsigned char *, int) = nullptr;
    int (*encryptFinal)(Ctx *, unsigned char *, int *) = nullptr;
    int (*decryptInit)(Ctx *, const Cipher *, void *, const unsigned char *, const unsigned char *) = nullptr;
    int (*decryptUpdate)(Ctx *, unsigned char *, int *, const unsigned char *, int) = nullptr;
    int (*decryptFinal)(Ctx *, unsigned char *, int *) = nullptr;
    int (*ctrl)(Ctx *, int, int, void *) = nullptr;
    int (*randBytes)(unsigned char *, int) = nullptr;
    int (*pbkdf2)(const char *, int, const unsigned char *, int, int, const Md *, int, unsigned char *) = nullptr;
    QString error;

    bool ok() const { return error.isEmpty(); }

    static const Crypto &get()
    {
        static const Crypto c = [] {
            Crypto c;
            static QLibrary lib;
            for (const auto &[name, version] : {std::pair{u"crypto"_s, u"3"_s}, std::pair{u"crypto"_s, QString()}}) {
                lib.setFileNameAndVersion(name, version);
                if (lib.load())
                    break;
            }
            if (!lib.isLoaded()) {
                c.error = u"OpenSSL's libcrypto was not found."_s;
                return c;
            }
            auto get = [&](auto &fn, const char *symbol) {
                fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(lib.resolve(symbol));
                if (!fn && c.error.isEmpty())
                    c.error = u"libcrypto has no %1."_s.arg(QLatin1StringView(symbol));
            };
            get(c.ctxNew, "EVP_CIPHER_CTX_new");
            get(c.ctxFree, "EVP_CIPHER_CTX_free");
            get(c.aes256gcm, "EVP_aes_256_gcm");
            get(c.sha256, "EVP_sha256");
            get(c.encryptInit, "EVP_EncryptInit_ex");
            get(c.encryptUpdate, "EVP_EncryptUpdate");
            get(c.encryptFinal, "EVP_EncryptFinal_ex");
            get(c.decryptInit, "EVP_DecryptInit_ex");
            get(c.decryptUpdate, "EVP_DecryptUpdate");
            get(c.decryptFinal, "EVP_DecryptFinal_ex");
            get(c.ctrl, "EVP_CIPHER_CTX_ctrl");
            get(c.randBytes, "RAND_bytes");
            get(c.pbkdf2, "PKCS5_PBKDF2_HMAC");
            return c;
        }();
        return c;
    }
};

// EVP_CTRL_GCM_* (stable values from openssl/evp.h).
constexpr int CtrlGcmSetIvLen = 0x9;
constexpr int CtrlGcmGetTag = 0x10;
constexpr int CtrlGcmSetTag = 0x11;

const unsigned char *u(const QByteArray &b) { return reinterpret_cast<const unsigned char *>(b.constData()); }
unsigned char *u(QByteArray &b) { return reinterpret_cast<unsigned char *>(b.data()); }

void setError(QString *error, const QString &text)
{
    if (error)
        *error = text;
}

bool readAll(const QString &file, QByteArray *data, QString *error)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) {
        setError(error, u"Could not read %1: %2"_s.arg(file, f.errorString()));
        return false;
    }
    *data = f.readAll();
    return true;
}

bool writeAll(const QString &file, const QByteArray &data, QString *error)
{
    if (QFile::exists(file)) {
        setError(error, u"%1 already exists."_s.arg(file));
        return false;
    }
    QSaveFile f(file);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit()) {
        setError(error, u"Could not write %1: %2"_s.arg(file, f.errorString()));
        return false;
    }
    QFile(file).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

} // namespace

bool sealingAvailable(QString *why)
{
    const Crypto &c = Crypto::get();
    if (!c.ok())
        setError(why, c.error);
    return c.ok();
}

QByteArray newSalt()
{
    QByteArray salt(SaltBytes, Qt::Uninitialized);
    const Crypto &c = Crypto::get();
    if (!c.ok() || c.randBytes(u(salt), int(salt.size())) != 1)
        return {};
    return salt;
}

QByteArray sealingKey(const QString &password, const QByteArray &salt)
{
    const Crypto &c = Crypto::get();
    const QByteArray pw = password.toUtf8();
    QByteArray key(KeyBytes, Qt::Uninitialized);
    if (!c.ok() || password.isEmpty() || salt.size() != SaltBytes
        || c.pbkdf2(pw.constData(), int(pw.size()), u(salt), int(salt.size()), Rounds, c.sha256(), KeyBytes, u(key)) != 1)
        return {};
    return key;
}

bool sealFile(const QString &in, const QString &out, const QByteArray &key, const QByteArray &salt, QString *error)
{
    const Crypto &c = Crypto::get();
    if (!c.ok()) {
        setError(error, c.error);
        return false;
    }
    if (key.size() != KeyBytes || salt.size() != SaltBytes) {
        setError(error, u"No backup key is set."_s);
        return false;
    }
    QByteArray plain;
    if (!readAll(in, &plain, error))
        return false;
    QByteArray nonce(NonceBytes, Qt::Uninitialized);
    if (c.randBytes(u(nonce), NonceBytes) != 1) {
        setError(error, u"No random numbers for the nonce."_s);
        return false;
    }
    const QByteArray header = Magic.toByteArray() + salt + nonce;
    QByteArray cipher(plain.size() + 16, Qt::Uninitialized);
    QByteArray tag(TagBytes, Qt::Uninitialized);
    int len = 0;
    int total = 0;
    void *ctx = c.ctxNew();
    const bool ok = ctx && c.encryptInit(ctx, c.aes256gcm(), nullptr, nullptr, nullptr) == 1
                    && c.ctrl(ctx, CtrlGcmSetIvLen, NonceBytes, nullptr) == 1
                    && c.encryptInit(ctx, nullptr, nullptr, u(key), u(nonce)) == 1
                    && c.encryptUpdate(ctx, nullptr, &len, u(header), int(header.size())) == 1   // authenticated header
                    && c.encryptUpdate(ctx, u(cipher), &len, u(plain), int(plain.size())) == 1
                    && (total = len, c.encryptFinal(ctx, u(cipher) + total, &len) == 1)
                    && c.ctrl(ctx, CtrlGcmGetTag, TagBytes, tag.data()) == 1;
    total += len;
    if (ctx)
        c.ctxFree(ctx);
    plain.fill('\0');
    if (!ok) {
        setError(error, u"Encryption failed."_s);
        return false;
    }
    cipher.truncate(total);
    return writeAll(out, header + cipher + tag, error);
}

bool isSealed(const QString &file)
{
    QFile f(file);
    return f.open(QIODevice::ReadOnly) && f.read(Magic.size()) == Magic;
}

namespace {

bool openSealed(const QString &in, const QString &out, const std::function<QByteArray(const QByteArray &salt)> &keyFor,
                QString *error)
{
    const Crypto &c = Crypto::get();
    if (!c.ok()) {
        setError(error, c.error);
        return false;
    }
    QByteArray data;
    if (!readAll(in, &data, error))
        return false;
    const qsizetype head = Magic.size() + SaltBytes + NonceBytes;
    if (data.size() < head + TagBytes || !data.startsWith(Magic)) {
        setError(error, u"%1 is not an encrypted ViewTouch backup."_s.arg(in));
        return false;
    }
    const QByteArray header = data.left(head);
    const QByteArray salt = data.mid(Magic.size(), SaltBytes);
    const QByteArray nonce = data.mid(Magic.size() + SaltBytes, NonceBytes);
    QByteArray tag = data.right(TagBytes);
    const QByteArray cipher = data.mid(head, data.size() - head - TagBytes);
    const QByteArray key = keyFor(salt);
    if (key.size() != KeyBytes) {
        setError(error, u"No password was given."_s);
        return false;
    }
    QByteArray plain(cipher.size() + 16, Qt::Uninitialized);
    int len = 0;
    int total = 0;
    void *ctx = c.ctxNew();
    const bool ok = ctx && c.decryptInit(ctx, c.aes256gcm(), nullptr, nullptr, nullptr) == 1
                    && c.ctrl(ctx, CtrlGcmSetIvLen, NonceBytes, nullptr) == 1
                    && c.decryptInit(ctx, nullptr, nullptr, u(key), u(nonce)) == 1
                    && c.decryptUpdate(ctx, nullptr, &len, u(header), int(header.size())) == 1
                    && c.decryptUpdate(ctx, u(plain), &len, u(cipher), int(cipher.size())) == 1
                    && (total = len, c.ctrl(ctx, CtrlGcmSetTag, TagBytes, tag.data()) == 1)
                    && c.decryptFinal(ctx, u(plain) + total, &len) == 1;   // checks the tag
    total += len;
    if (ctx)
        c.ctxFree(ctx);
    if (!ok) {
        setError(error, u"Wrong password, or the file was changed or damaged."_s);
        return false;
    }
    plain.truncate(total);
    return writeAll(out, plain, error);
}

} // namespace

bool openSealedFile(const QString &in, const QString &out, const QString &password, QString *error)
{
    return openSealed(in, out, [&](const QByteArray &salt) { return sealingKey(password, salt); }, error);
}

bool openSealedFileWithKey(const QString &in, const QString &out, const QByteArray &key, QString *error)
{
    return openSealed(in, out, [&](const QByteArray &) { return key; }, error);
}

} // namespace vt::storage
