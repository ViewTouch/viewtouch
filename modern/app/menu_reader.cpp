#include "app/menu_reader.hh"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QPointer>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThreadPool>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <vector>

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

QImage MenuReader::straightened(const QImage &page, double *degrees)
{
    if (page.isNull())
        return {};
    QImage gray = page.convertToFormat(QImage::Format_Grayscale8);
    // A phone's speckle (its sensor's, a JPEG's) smoothed a little, then the
    // ink made black and the paper white: Tesseract reads columns of prices
    // badly through either (2.50 2.95 3.25 read as "2200222695").
    {
        const int w = gray.width(), h = gray.height();
        const int k[5] = {1, 4, 6, 4, 1};   // a Gaussian, about one pixel wide
        QImage across(gray.size(), QImage::Format_Grayscale8);
        for (int y = 0; y < h; ++y) {
            const uchar *in = gray.constScanLine(y);
            uchar *out = across.scanLine(y);
            for (int x = 0; x < w; ++x) {
                int sum = 0;
                for (int i = -2; i <= 2; ++i)
                    sum += k[i + 2] * in[std::clamp(x + i, 0, w - 1)];
                out[x] = uchar(sum / 16);
            }
        }
        for (int y = 0; y < h; ++y) {
            uchar *out = gray.scanLine(y);
            for (int x = 0; x < w; ++x) {
                int sum = 0;
                for (int i = -2; i <= 2; ++i)
                    sum += k[i + 2] * across.constScanLine(std::clamp(y + i, 0, h - 1))[x];
                out[x] = uchar(sum / 16);
            }
        }
        // Stretched: the darkest 1% black, the lightest 1% white...
        long long hist[256] = {};
        for (int y = 0; y < h; ++y) {
            const uchar *row = gray.constScanLine(y);
            for (int x = 0; x < w; ++x)
                ++hist[row[x]];
        }
        const long long total = qint64(w) * h, edge = total / 100;
        int lo = 0, hi = 255;
        for (long long n = 0; lo < 255 && (n += hist[lo]) <= edge;)
            ++lo;
        for (long long n = 0; hi > 0 && (n += hist[hi]) <= edge;)
            --hi;
        // ...and pushed further (the paper's gray white, the ink black): what
        // made a photo's price columns read right on the store's computer.
        const int span = hi - lo;
        lo += span * 15 / 100;
        hi -= span * 15 / 100;
        if (hi > lo + 16) {
            uchar map[256];
            for (int v = 0; v < 256; ++v)
                map[v] = uchar(std::clamp((v - lo) * 255 / (hi - lo), 0, 255));
            for (int y = 0; y < h; ++y) {
                uchar *row = gray.scanLine(y);
                for (int x = 0; x < w; ++x)
                    row[x] = map[row[x]];
            }
        }
    }
    // The tilt, on a small copy: the dark pixels' rows line up best (their
    // counts per row vary most) when the text is level.
    const QImage small = gray.width() > 1000 ? gray.scaledToWidth(1000, Qt::SmoothTransformation) : gray;
    long long sum = 0;
    for (int y = 0; y < small.height(); ++y) {
        const uchar *row = small.constScanLine(y);
        for (int x = 0; x < small.width(); ++x)
            sum += row[x];
    }
    const int mean = !small.isNull() ? int(sum / (qint64(small.width()) * small.height())) : 128;
    const int dark = mean * 2 / 3;   // ink: well below the page's average
    std::vector<QPoint> ink;
    for (int y = 0; y < small.height(); ++y) {
        const uchar *row = small.constScanLine(y);
        for (int x = 0; x < small.width(); ++x)
            if (row[x] < dark)
                ink.emplace_back(x, y);
    }
    double best = 0, bestScore = -1;
    if (ink.size() > 50 && ink.size() < size_t(small.width()) * small.height() / 2) {
        const double cx = small.width() / 2.0, cy = small.height() / 2.0;
        const int rows = small.height() * 2;
        std::vector<int> count(size_t(rows), 0);
        for (int step = -24; step <= 24; ++step) {   // -6° to 6°, a quarter at a time
            const double a = step * 0.25;
            const double t = a * M_PI / 180, s = std::sin(t), c = std::cos(t);
            std::fill(count.begin(), count.end(), 0);
            for (const QPoint &p : ink) {
                const int y = int(std::lround((p.x() - cx) * s + (p.y() - cy) * c + small.height()));
                if (y >= 0 && y < rows)
                    ++count[size_t(y)];
            }
            double score = 0;
            for (int i = 1; i < rows; ++i)
                score += double(count[size_t(i)] - count[size_t(i - 1)]) * (count[size_t(i)] - count[size_t(i - 1)]);
            if (score > bestScore) {
                bestScore = score;
                best = a;
            }
        }
    }
    if (degrees)
        *degrees = best;
    if (std::abs(best) < 0.2)
        return gray;   // (cleaned, already level)
    // Turned back by that much, the corners filled with the page's own gray.
    QImage out(gray.size(), QImage::Format_Grayscale8);
    out.fill(QColor(mean, mean, mean));
    QPainter paint(&out);
    paint.setRenderHint(QPainter::SmoothPixmapTransform);
    paint.translate(gray.width() / 2.0, gray.height() / 2.0);
    paint.rotate(best);
    paint.translate(-gray.width() / 2.0, -gray.height() / 2.0);
    paint.drawImage(0, 0, gray);
    paint.end();
    return out;
}

void MenuReader::readPages(QStringList pages, int at, QString text)
{
    if (at >= pages.size())
        return finish(text, {});
    if (progress_)
        progress_(pages.size() > 1 ? tr("Reading page %1 of %2…").arg(at + 1).arg(pages.size())
                                   : tr("Reading the photo…"));
    // Straightened first, away from the screen (a photo is millions of pixels).
    const QString page = pages[at], level = dir_.filePath(u"level-%1.png"_s.arg(at));
    QPointer<MenuReader> self(this);
    QThreadPool::globalInstance()->start([self, page, level, pages, at, text] {
        QImage in(page);
        // (A phone's 12-megapixel photo: more than the words need.)
        if (in.width() > 3000)
            in = in.scaledToWidth(3000, Qt::SmoothTransformation);
        const QImage out = straightened(in);
        const bool saved = !out.isNull() && out.save(level, "PNG");
        // (Back on the reader's thread; it may be gone, given up.)
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, page, level, saved, pages, at, text] {
            if (self && !self->over_)
                self->readPage(saved ? level : page, pages, at, text);
        }, Qt::QueuedConnection);
    });
}

void MenuReader::readPage(const QString &file, QStringList pages, int at, QString text)
{
    // English and Spanish menus; a block of lines kept as lines (a price on
    // its item's row, columns of sizes beside it).
    run(u"tesseract"_s, {file, u"stdout"_s, u"-l"_s, u"eng+spa"_s, u"--psm"_s, u"4"_s, u"-c"_s, u"preserve_interword_spaces=1"_s},
        [this, pages, at, text](const QByteArray &out) mutable {
            QString page = QString::fromUtf8(out).trimmed();
            // OCR's usual slip on a cup size: "12oz" read as "120z".
            static const QRegularExpression ounces(u"\\b(\\d{1,2})0z\\b"_s);
            page.replace(ounces, u"\\1oz"_s);
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
