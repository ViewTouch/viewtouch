#include "app/menu_reader.hh"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace vt::app {

namespace {
// One program's longest: a page of tesseract on a CM5 is about half a minute.
constexpr int MostSeconds = 180;
} // namespace

MenuReader::MenuReader(QByteArray data, QObject *parent)
    : QObject(parent)
    , data_(std::move(data))
{
    limit_.setSingleShot(true);
    limit_.setInterval(MostSeconds * 1000);
    connect(&limit_, &QTimer::timeout, this, [this] {
        if (process_)
            process_->kill();   // its finished() says it took too long
    });
}

QString MenuReader::kindOf(const QByteArray &d)
{
    if (d.startsWith("%PDF-"))
        return u"pdf"_s;
    if (d.startsWith("\x89PNG\r\n\x1a\n") || d.startsWith("\xff\xd8\xff") || d.startsWith("II*\0")
        || d.startsWith(QByteArray("MM\0*", 4)) || (d.startsWith("RIFF") && d.mid(8, 4) == "WEBP"))
        return u"image"_s;
    return {};
}

QString MenuReader::program(const QString &name)
{
    const QByteArray extra = qgetenv("VTM_VTAI_TOOLS");
    if (!extra.isEmpty()) {
        const QString found = QStandardPaths::findExecutable(name, {QString::fromLocal8Bit(extra)});
        if (!found.isEmpty())
            return found;
    }
    return QStandardPaths::findExecutable(name);
}

void MenuReader::start(Progress progress, Done done)
{
    progress_ = std::move(progress);
    done_ = std::move(done);
    const QString kind = kindOf(data_);
    if (data_.size() > MostBytes)
        return finish({}, tr("That file is over 20 MB: a smaller photo, or the PDF's menu pages alone."));
    if (kind.isEmpty())
        return finish({}, tr("That isn't a PDF or a photo VT Ai reads (JPEG, PNG, TIFF or WebP). An iPhone's photo: share it as JPEG (Most Compatible)."));
    if (!dir_.isValid())
        return finish({}, tr("VT Ai couldn't make a folder to read it in."));
    const QString file = dir_.filePath(kind == u"pdf"_s ? u"menu.pdf"_s : u"menu.img"_s);
    QFile f(file);
    if (!f.open(QIODevice::WriteOnly) || f.write(data_) != data_.size())
        return finish({}, tr("VT Ai couldn't make a folder to read it in."));
    f.close();
    data_.clear();

    if (kind == u"image"_s) {
        readPages({file}, 0, {});
        return;
    }
    // A PDF: its own words, when it has them (made on a computer)...
    if (progress_)
        progress_(tr("Reading the PDF…"));
    run(u"pdftotext"_s, {u"-layout"_s, u"-enc"_s, u"UTF-8"_s, u"-l"_s, QString::number(MostPages), file, u"-"_s},
        [this, file](const QByteArray &out) {
            const QString text = QString::fromUtf8(out);
            // (A scan has none, or a few stray letters.)
            if (text.simplified().size() >= 40)
                return finish(text, {});
            // ...else its pages as pictures, each read.
            if (progress_)
                progress_(tr("It's a scan: reading its pages…"));
            run(u"pdftoppm"_s, {u"-r"_s, u"300"_s, u"-gray"_s, u"-png"_s, u"-l"_s, QString::number(MostPages), file,
                                dir_.filePath(u"page"_s)},
                [this](const QByteArray &) {
                    QStringList pages;
                    for (const QString &p : QDir(dir_.path()).entryList({u"page*.png"_s}, QDir::Files, QDir::Name))
                        pages << dir_.filePath(p);
                    if (pages.isEmpty())
                        return finish({}, tr("That PDF has no pages VT Ai could read."));
                    readPages(pages, 0, {});
                });
        });
}

void MenuReader::readPages(QStringList pages, int at, QString text)
{
    if (at >= pages.size())
        return finish(text, {});
    if (progress_)
        progress_(pages.size() > 1 ? tr("Reading page %1 of %2…").arg(at + 1).arg(pages.size())
                                   : tr("Reading the photo…"));
    // English and Spanish menus; the page's own layout (columns too).
    run(u"tesseract"_s, {pages[at], u"stdout"_s, u"-l"_s, u"eng+spa"_s},
        [this, pages, at, text](const QByteArray &out) mutable {
            const QString page = QString::fromUtf8(out).trimmed();
            if (!page.isEmpty())
                text += (text.isEmpty() ? QString() : u"\n\n"_s) + page;
            readPages(pages, at + 1, text);
        });
}

void MenuReader::run(const QString &name, const QStringList &args, std::function<void(QByteArray out)> then)
{
    const QString path = program(name);
    if (path.isEmpty())
        return finish({}, tr("%1 isn't on this computer: it comes with VT Ai (sudo apt install vtmodern-ai).").arg(name));
    auto *p = new QProcess(this);
    process_ = p;
    p->setWorkingDirectory(dir_.path());
    p->setProcessChannelMode(QProcess::SeparateChannels);
    p->setStandardInputFile(QProcess::nullDevice());
    connect(p, &QProcess::finished, this, [this, p, name, then = std::move(then)](int code, QProcess::ExitStatus how) {
        p->deleteLater();
        limit_.stop();
        if (over_)
            return;
        if (how != QProcess::NormalExit)
            return finish({}, tr("%1 took too long or stopped: try a smaller file.").arg(name));
        if (code != 0) {
            const QString said = QString::fromUtf8(p->readAllStandardError()).trimmed().section(u'\n', -1).left(200);
            return finish({}, tr("%1 couldn't read it: %2").arg(name, said.isEmpty() ? QString::number(code) : said));
        }
        then(p->readAllStandardOutput());
    });
    connect(p, &QProcess::errorOccurred, this, [this, p, name](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart && !over_) {
            p->deleteLater();
            limit_.stop();
            finish({}, tr("%1 couldn't start.").arg(name));
        }
    });
    limit_.start();
    p->start(path, args);
}

void MenuReader::cancel()
{
    over_ = true;
    limit_.stop();
    if (process_) {
        process_->disconnect(this);
        process_->kill();
        process_->waitForFinished(1000);
    }
}

void MenuReader::finish(const QString &text, const QString &error)
{
    if (over_)
        return;
    over_ = true;
    if (done_)
        done_(text, error);
}

} // namespace vt::app
