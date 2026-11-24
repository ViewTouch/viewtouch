#pragma once

// The words of a menu's PDF or photo, read on this computer for VT Ai:
// a PDF's own text (pdftotext); a scanned PDF's pages as pictures
// (pdftoppm) and a photo, read by tesseract (English and Spanish). Those
// come with VT Ai's package (vtmodern-ai). Never blocks the screen: each
// program runs on its own, a page at a time, with a time limit; files only in
// a folder of its own, gone after. VTM_VTAI_TOOLS: a folder looked in first.

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>

#include <functional>

namespace vt::app {

class MenuReader : public QObject {
    Q_OBJECT

public:
    static constexpr qint64 MostBytes = 20 * 1024 * 1024;
    static constexpr int MostPages = 20;

    using Progress = std::function<void(const QString &said)>;
    using Done = std::function<void(const QString &text, const QString &error)>;

    // `data`: the file's bytes (a PDF, JPEG, PNG, TIFF or WebP).
    MenuReader(QByteArray data, QObject *parent = nullptr);

    // Reads it: Progress as it goes ("Reading page 2 of 3…"), then Done once.
    void start(Progress progress, Done done);
    // Given up: the program reading it stopped; Done isn't called.
    void cancel();

    // What the file is: "pdf", "image", or "" (neither).
    static QString kindOf(const QByteArray &data);
    // The program's path ("" when it isn't on this computer).
    static QString program(const QString &name);
    // A photo or scanned page turned level (a phone's photo is rarely
    // straight, and Tesseract reads tilted columns of prices badly), in
    // grays: the tilt found by trying angles up to 6 degrees either way.
    // The angle used comes back in `degrees`. Null: not a picture.
    static QImage straightened(const QImage &page, double *degrees = nullptr);

private:
    void run(const QString &name, const QStringList &args, std::function<void(QByteArray out)> then);
    void readPages(QStringList pages, int at, QString text);
    void readPage(const QString &file, QStringList pages, int at, QString text);
    void finish(const QString &text, const QString &error);

    QByteArray data_;
    QTemporaryDir dir_;
    Progress progress_;
    Done done_;
    QPointer<QProcess> process_;
    QTimer limit_;
    bool over_ = false;
};

} // namespace vt::app
