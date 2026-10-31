// PosService: the store's pictures - a logo, button pictures, backgrounds.
// Kept in the database (so backups, the standby and paired screens have
// them) and written out as files for this computer's screens to show.

#include "app/pos_json.hh"
#include "app/pos_service.hh"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrl>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {
constexpr qint64 kMaxImageBytes = 8 * 1024 * 1024;
const QStringList kImageTypes = {u"png"_s, u"jpg"_s, u"jpeg"_s, u"webp"_s, u"gif"_s, u"svg"_s, u"bmp"_s};
const QStringList kFontTypes = {u"ttf"_s, u"otf"_s};

bool isFont(const QString &name)
{
    return kFontTypes.contains(QFileInfo(name).suffix().toLower());
}

// By the first bytes: PNG, JPEG, GIF, WebP, BMP or SVG.
bool looksLikePicture(const QByteArray &d)
{
    return d.startsWith(QByteArray("\x00\x01\x00\x00", 4)) || d.startsWith("OTTO") || d.startsWith("true")   // fonts
           || d.startsWith("\x89PNG") || d.startsWith("\xff\xd8") || d.startsWith("GIF8") || d.startsWith("BM")
           || (d.startsWith("RIFF") && d.mid(8, 4) == "WEBP")
           || d.left(512).contains("<svg");
}

QString contentHash(const QByteArray &data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex().left(16));
}
} // namespace

QString PosShared::imageFile(const QString &ref)
{
    const QString name = ref.startsWith(u"store:") ? ref.mid(6) : ref;
    const auto it = images.find(ss(name));
    if (it == images.end())
        return {};
    // Named by content: a picture replaced under the same name is a new file.
    const QString dir = imageCacheDir.isEmpty() ? QDir::temp().filePath(u"vtm-store-images"_s) : imageCacheDir;
    QDir().mkpath(dir);
    const QString path = QDir(dir).filePath(contentHash(it->second) + u'.' + QFileInfo(name).suffix().toLower());
    if (!QFileInfo::exists(path)) {
        QSaveFile f(path);
        if (!f.open(QIODevice::WriteOnly) || f.write(it->second) != it->second.size() || !f.commit())
            return {};
    }
    return path;
}

QString PosService::imageUrl(const QString &ref) const
{
    if (ref.isEmpty())
        return {};
    if (ref == u"logo:") {   // the store's logo, wherever it is
        const QString logo = qs(s_->settings.displayLogo);
        return logo == u"logo:" ? QString() : imageUrl(logo);
    }
    if (ref.startsWith(u"store:")) {
        const QString file = s_->imageFile(ref);
        return file.isEmpty() ? QString() : QUrl::fromLocalFile(file).toString();
    }
    if (ref.startsWith(u'/'))
        return QUrl::fromLocalFile(ref).toString();
    return ref;   // qrc:, file:, http...
}

QVariantList PosService::storeImages() const
{
    QVariantList out;
    for (const auto &[name, data] : s_->images)
        out.append(QVariantMap{{u"name"_s, qs(name)}, {u"ref"_s, u"store:"_s + qs(name)},
                               {u"kind"_s, isFont(qs(name)) ? u"font"_s : u"picture"_s},
                               {u"hash"_s, contentHash(data)}, {u"bytes"_s, qint64(data.size())},
                               {u"url"_s, imageUrl(u"store:"_s + qs(name))}});
    return out;
}

bool PosService::addStoreImage(const QString &fileName, const QString &base64)
{
    if (!require(perm::Manager, tr("Adding pictures")))
        return false;
    const QString ref = storeImageRef(fileName);
    if (ref.isEmpty())
        return fail(tr("Pictures can be PNG, JPEG, WebP, GIF, BMP or SVG files; fonts TTF or OTF."));
    const QString name = ref.mid(6);
    const QString suffix = QFileInfo(name).suffix();
    const QByteArray data = QByteArray::fromBase64(base64.toLatin1());
    if (data.isEmpty())
        return fail(tr("That file is empty."));
    if (data.size() > kMaxImageBytes)
        return fail(tr("That picture is too big (8 MB at most)."));
    if (!looksLikePicture(data))
        return fail(isFont(name) ? tr("That file isn't a font this program can use.")
                                 : tr("That file isn't a picture this program can show."));
    const bool replaced = s_->images.contains(ss(name));
    s_->images[ss(name)] = data;
    if (s_->sink)
        s_->sink->saveImage(ss(name), data);
    ++s_->adminRevision;
    emit s_->adminChanged();
    emit notice(replaced ? tr("Picture %1 replaced").arg(name) : tr("Picture %1 added").arg(name));
    return true;
}

// A picture brought in with menu items (a spreadsheet's Photo column,
// another store's menu): kept under its name, or a name of its own when the
// store has a different picture by that name. "" if it isn't a picture.
QString PosService::putMenuPicture(const QString &fileName, const QByteArray &data)
{
    const QString ref = storeImageRef(fileName);
    if (ref.isEmpty() || isFont(ref) || data.isEmpty() || data.size() > kMaxImageBytes || !looksLikePicture(data))
        return {};
    const QFileInfo info(ref.mid(6));
    QString name = info.fileName();
    for (int n = 2;; ++n) {
        const auto it = s_->images.find(ss(name));
        if (it == s_->images.end())
            break;
        if (it->second == data)
            return u"store:"_s + name;   // the same picture: already here
        name = u"%1-%2.%3"_s.arg(info.completeBaseName()).arg(n).arg(info.suffix());
    }
    s_->images[ss(name)] = data;
    if (s_->sink)
        s_->sink->saveImage(ss(name), data);
    return u"store:"_s + name;   // the caller says the menu changed
}

bool PosService::removeStoreImage(const QString &name)
{
    if (!require(perm::Manager, tr("Removing pictures")))
        return false;
    const QString plain = name.startsWith(u"store:") ? name.mid(6) : name;
    if (!s_->images.erase(ss(plain)))
        return fail(tr("There is no picture %1.").arg(plain));
    if (s_->sink)
        s_->sink->deleteImage(ss(plain));
    ++s_->adminRevision;
    emit s_->adminChanged();
    emit notice(tr("Picture %1 removed").arg(plain));
    return true;
}

QVariantList PosSession::allergenList()
{
    QVariantList out;
    const QStringList names = allergenNames(core::allergenIds());
    for (std::size_t i = 0; i < core::allergenIds().size(); ++i)
        out.append(QVariantMap{{u"id"_s, QString::fromStdString(core::allergenIds()[i])}, {u"name"_s, names.value(int(i))}});
    return out;
}

QStringList PosSession::allergenNames(const std::vector<std::string> &ids)
{
    static const QHash<QString, const char *> names{
        {u"gluten"_s, QT_TR_NOOP("Gluten")}, {u"dairy"_s, QT_TR_NOOP("Dairy")}, {u"egg"_s, QT_TR_NOOP("Egg")},
        {u"peanut"_s, QT_TR_NOOP("Peanut")}, {u"treenut"_s, QT_TR_NOOP("Tree nuts")}, {u"soy"_s, QT_TR_NOOP("Soy")},
        {u"fish"_s, QT_TR_NOOP("Fish")}, {u"shellfish"_s, QT_TR_NOOP("Shellfish")}, {u"sesame"_s, QT_TR_NOOP("Sesame")}};
    QStringList out;
    for (const std::string &id : ids) {
        const QString k = QString::fromStdString(id);
        out << (names.contains(k) ? tr(names.value(k)) : k);
    }
    return out;
}

QString PosSession::storeImageRef(const QString &fileName)
{
    // A plain name: "Logo Final.PNG" -> "store:logo-final.png".
    const QFileInfo info(fileName);
    const QString suffix = info.suffix().toLower();
    if (!kImageTypes.contains(suffix) && !kFontTypes.contains(suffix))
        return {};
    static const QRegularExpression unsafe(u"[^a-z0-9]+"_s);
    QString base = info.completeBaseName().toLower();
    base.replace(unsafe, u"-"_s);
    while (base.startsWith(u'-')) base.remove(0, 1);
    while (base.endsWith(u'-')) base.chop(1);
    if (base.isEmpty())
        base = u"picture"_s;
    return u"store:"_s + base.left(40) + u'.' + (suffix == u"jpeg" ? u"jpg"_s : suffix);
}

QVariantList PosSession::pictureFiles() const
{
    static const QStringList kinds{u"*.png"_s, u"*.jpg"_s, u"*.jpeg"_s, u"*.webp"_s, u"*.gif"_s, u"*.bmp"_s, u"*.svg"_s,
                                   u"*.PNG"_s, u"*.JPG"_s, u"*.JPEG"_s};
    // Where people keep pictures: this account's folders, and each person's
    // on a ViewTouch computer (it runs as its own account); USB sticks.
    QList<std::pair<QString, QString>> places;   // folder, how it's called
    for (const auto where : {QStandardPaths::PicturesLocation, QStandardPaths::DocumentsLocation,
                             QStandardPaths::DownloadLocation, QStandardPaths::DesktopLocation})
        places.append({QStandardPaths::writableLocation(where), QStandardPaths::displayName(where)});
    for (const QFileInfo &home : QDir(u"/home"_s).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
        for (const char *sub : {"Pictures", "Documents", "Downloads", "Desktop"})
            places.append({home.filePath() + u'/' + QString::fromLatin1(sub),
                           QString::fromLatin1(sub) + u" ("_s + home.fileName() + u')'});
    for (const QString &root : {u"/run/vtmodern-usb"_s, u"/media"_s, u"/run/media"_s})
        for (const QFileInfo &d : QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            places.append({d.filePath(), tr("USB stick")});
            for (const QFileInfo &dd : QDir(d.filePath()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
                places.append({dd.filePath(), tr("USB stick")});
        }
    QList<std::pair<QFileInfo, QString>> found;   // the file, where it is
    QSet<QString> seen;
    for (const auto &[folder, label] : places) {
        if (folder.isEmpty() || !QFileInfo(folder).isReadable())
            continue;
        // The folder and one level down (a "Logos" folder in Pictures).
        QStringList dirs{folder};
        for (const QFileInfo &d : QDir(folder).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
            dirs << d.filePath();
        for (const QString &dir : dirs)
            for (const QFileInfo &f : QDir(dir).entryInfoList(kinds, QDir::Files | QDir::Readable)) {
                if (f.size() > kMaxImageBytes || seen.contains(f.canonicalFilePath()))
                    continue;
                seen.insert(f.canonicalFilePath());
                found.append({f, label});
            }
    }
    std::sort(found.begin(), found.end(),
              [](const auto &a, const auto &b) { return a.first.lastModified() > b.first.lastModified(); });
    QVariantList out;
    for (const auto &[f, label] : found.mid(0, 300))
        out << QVariantMap{{u"path"_s, f.filePath()}, {u"name"_s, f.fileName()}, {u"folder"_s, label}};
    return out;
}

bool PosSession::openUsbSticks()
{
    const QString helper = u"/usr/libexec/vtmodern/vtmodern-device"_s;
    if (!QFileInfo(helper).isExecutable())
        return false;
    auto *p = new QProcess(this);
    connect(p, &QProcess::finished, this, [this, p](int code, QProcess::ExitStatus status) {
        p->deleteLater();
        const bool ok = status == QProcess::NormalExit && code == 0;
        if (!ok) {
            const QString why = QString::fromUtf8(p->readAllStandardError()).trimmed();
            emit notice(why.isEmpty() ? tr("No USB stick found.") : why.split(u'\n').last());
        }
        emit usbSticksOpened(ok);
    });
    connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        p->deleteLater();
        emit notice(tr("No USB stick found."));
        emit usbSticksOpened(false);
    });
    p->start(u"sudo"_s, {u"-n"_s, helper, u"usb-open"_s});
    return true;
}

namespace {

// What kind of picture or font these bytes are, by how they start ("png",
// "jpg"...); empty if none we take.
QString kindOf(const QByteArray &data)
{
    const auto starts = [&](const char *magic, qsizetype at = 0) {
        return data.mid(at, qsizetype(qstrlen(magic))) == QByteArray(magic);
    };
    if (starts("\x89PNG"))
        return u"png"_s;
    if (starts("\xFF\xD8\xFF"))
        return u"jpg"_s;
    if (starts("GIF8"))
        return u"gif"_s;
    if (starts("BM"))
        return u"bmp"_s;
    if (starts("RIFF") && starts("WEBP", 8))
        return u"webp"_s;
    if (starts("OTTO"))
        return u"otf"_s;
    if (data.startsWith(QByteArray("\x00\x01\x00\x00", 4)) || starts("true"))
        return u"ttf"_s;
    if (data.left(1024).contains("<svg"))
        return u"svg"_s;
    return {};
}

} // namespace

QString PosSession::addImageFile(const QString &fileOrUrl)
{
    // Read here (this computer's file), kept by the store. On Android the
    // photo picker hands over a content:// link: no file name ending, and
    // often no name at all ("image:1000012345").
    const bool content = fileOrUrl.startsWith(u"content:");
    const QString path = fileOrUrl.startsWith(u"file:") ? QUrl(fileOrUrl).toLocalFile() : fileOrUrl;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        emit notice(tr("Can't open %1.").arg(QFileInfo(path).fileName()));
        return {};
    }
    const QByteArray data = f.read(kMaxImageBytes + 1);
    if (data.size() > kMaxImageBytes) {
        emit notice(tr("That picture is too big (8 MB at most)."));
        return {};
    }
    QString name = QFileInfo(path).fileName();
    if (content) {
        // "primary%3APictures%2FLogo.png" -> "Logo.png"; "image%3A1000012345" -> "1000012345"
        name = QUrl::fromPercentEncoding(name.toUtf8());
        name = name.mid(std::max(name.lastIndexOf(u'/'), name.lastIndexOf(u':')) + 1);
    }
    // A name without the right ending: named by what the bytes are.
    if (storeImageRef(name).isEmpty()) {
        const QString kind = kindOf(data);
        if (!kind.isEmpty())
            name = (QFileInfo(name).completeBaseName().isEmpty() || (content && name.contains(QRegularExpression(u"^[0-9]+$"_s)))
                        ? u"picture"_s
                        : QFileInfo(name).completeBaseName())
                   + u'.' + kind;
    }
    const QString ref = storeImageRef(name);
    if (ref.isEmpty()) {
        emit notice(tr("Pictures can be PNG, JPEG, WebP, GIF, BMP or SVG files; fonts TTF or OTF."));
        return {};
    }
    invoke(u"addStoreImage"_s, {name, QString::fromLatin1(data.toBase64())});
    return ref;
}

} // namespace vt::app
