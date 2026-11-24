// PosService: VT Ai, the small AI on the store's computer (vtmodern-ai, see
// net/vtai_client.hh). It turns a menu typed or pasted, a photo of one or a
// PDF into items, sizes and choices, for Menu Builder's import preview: it
// only ever proposes, a manager looks and imports (one undo step). Nothing
// it reads or writes leaves the store.

#include "app/menu_import.hh"
#include "app/menu_reader.hh"
#include "app/pos_service.hh"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

// seed/vtai/menu-lines.json: what VT Ai is told, an example, and the
// grammar its answer keeps to (a line an item: about a third of JSON's words,
// three times as quick on the store's computer, and as right). The
// benchmark, tools/vtai_bench.py --format lines, sends the same.
const QJsonObject &menuSpec()
{
    static const QJsonObject spec = [] {
        QFile f(u":/seed/vtai/menu-lines.json"_s);
        return f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object() : QJsonObject();
    }();
    return spec;
}

// A menu in parts VT Ai reads well (about 1,500 letters), cut between
// paragraphs, else between lines: its headings stay with their items.
QStringList menuParts(const QString &text, qsizetype most = 1500)
{
    QStringList blocks;
    for (const QString &b : QString(text).replace(u"\r\n"_s, u"\n"_s).split(u"\n\n"_s, Qt::SkipEmptyParts)) {
        if (b.size() <= most) {
            blocks << b;
            continue;
        }
        QString piece;
        for (const QString &line : b.split(u'\n')) {
            if (!piece.isEmpty() && piece.size() + line.size() > most) {
                blocks << piece;
                piece.clear();
            }
            piece += (piece.isEmpty() ? QString() : u"\n"_s) + line.left(most);
        }
        if (!piece.isEmpty())
            blocks << piece;
    }
    QStringList parts;
    QString part;
    for (const QString &b : std::as_const(blocks)) {
        if (!part.isEmpty() && part.size() + b.size() + 2 > most) {
            parts << part;
            part.clear();
        }
        part += (part.isEmpty() ? QString() : u"\n\n"_s) + b.trimmed();
    }
    if (!part.trimmed().isEmpty())
        parts << part;
    return parts;
}

} // namespace

QVariantMap PosService::vtai() const
{
    QVariantMap out = vtai_;
    out.insert(u"available"_s, s_->vtaiRunning && bool(s_->vtaiCall) && can(QString::fromLatin1(perm::Manager)));
    out.insert(u"model"_s, s_->vtaiModel);
    return out;
}

bool PosService::vtaiReady()
{
    if (!require(perm::Manager, tr("VT Ai")))
        return false;
    if (!s_->vtaiCall || !s_->vtaiRunning)
        return fail(tr("VT Ai isn't running on the store's computer (Store Settings → VT Ai)."));
    if (vtai_.value(u"busy"_s).toBool())
        return fail(tr("VT Ai is still working on the last one."));
    if (menuSpec().isEmpty())
        return fail(tr("VT Ai's instructions are missing from this ViewTouch."));
    return true;
}

bool PosService::vtaiMenuFromText(const QString &text)
{
    if (!vtaiReady())
        return false;
    if (text.trimmed().isEmpty())
        return fail(tr("Type or paste the menu first."));
    return vtaiStartMenu(text);
}

bool PosService::vtaiStartMenu(const QString &text)
{
    const QString menu = text.trimmed();
    if (menu.size() > 30'000)
        return fail(tr("That's more than VT Ai reads at once: 30,000 letters (about 10 pages). Paste it in parts."));
    const QStringList parts = menuParts(menu);
    const int request = ++vtaiRequest_;
    vtai_ = {{u"task"_s, u"menu"_s}, {u"busy"_s, true}, {u"step"_s, 0}, {u"steps"_s, int(parts.size())},
             {u"progress"_s, tr("Reading the menu…")}};
    emit sessionChanged();
    vtaiMenuPart(parts, 0, {}, request, {});
    return true;
}

bool PosService::vtaiMenuFromFile(const QString &base64)
{
    if (!vtaiReady())
        return false;
    // (Base64 is a third longer than the file.)
    if (base64.size() > MenuReader::MostBytes / 3 * 4 + 8)
        return fail(tr("That file is over 20 MB: a smaller photo, or the PDF's menu pages alone."));
    const QByteArray data = QByteArray::fromBase64(base64.toLatin1());
    if (data.isEmpty())
        return fail(tr("That file is empty."));
    const int request = ++vtaiRequest_;
    vtai_ = {{u"task"_s, u"menu"_s}, {u"busy"_s, true}, {u"progress"_s, tr("Reading the file…")}};
    emit sessionChanged();
    auto *reader = new MenuReader(data, this);
    vtaiReader_ = reader;
    QPointer<PosService> self(this);
    reader->start(
        [self, request](const QString &said) {
            if (!self || request != self->vtaiRequest_)
                return;
            self->vtai_.insert(u"progress"_s, said);
            emit self->sessionChanged();
        },
        [self, request, reader](const QString &text, const QString &error) {
            reader->deleteLater();
            if (!self || request != self->vtaiRequest_)
                return;
            QString why = error;
            if (why.isEmpty() && text.simplified().size() < 3)
                why = tr("No words could be read in it: a sharper photo, flat, straight on and in good light, reads better.");
            if (!why.isEmpty()) {
                self->vtai_ = {{u"task"_s, u"menu"_s}, {u"busy"_s, false}, {u"error"_s, why}};
                emit self->sessionChanged();
                return;
            }
            // Its words, read as a pasted menu would be (the busy flag let go first).
            self->vtai_.insert(u"busy"_s, false);
            if (!self->vtaiStartMenu(text)) {
                self->vtai_ = {{u"task"_s, u"menu"_s}, {u"busy"_s, false},
                               {u"error"_s, tr("That's more than VT Ai reads at once: 30,000 letters (about 10 pages). Paste it in parts.")}};
                emit self->sessionChanged();
            }
        });
    return true;
}

void PosService::vtaiMenuPart(const QStringList &parts, int at, QList<QJsonObject> answers, int request, QString lastCategory)
{
    if (request != vtaiRequest_)
        return;   // given up, or another begun
    if (at >= parts.size()) {
        MenuImport read = readVtAiMenu(answers);
        for (int i = 0; i < answers.size(); ++i)
            if (answers[i].isEmpty())
                read.problems.prepend(tr("Part %1 of the menu couldn't be read: check its items in the preview, or try that part alone.").arg(i + 1));
        vtai_ = {{u"task"_s, u"menu"_s}, {u"busy"_s, false}, {u"result"_s, importRowsOf(read)}};
        emit sessionChanged();
        return;
    }
    if (parts.size() > 1) {
        vtai_.insert(u"step"_s, at + 1);
        vtai_.insert(u"progress"_s, tr("Reading the menu… (part %1 of %2)").arg(at + 1).arg(parts.size()));
        emit sessionChanged();
    }
    const QJsonObject &spec = menuSpec();
    // A part that starts mid-section: under the heading the one before ended with.
    QString said = parts[at];
    if (!lastCategory.isEmpty())
        said = u"(This part goes on from the heading \"%1\".)"_s.arg(lastCategory) + u"\n\n"_s + said;
    const QJsonObject example = spec.value(u"example").toObject();
    const QJsonObject body{
        {u"messages"_s, QJsonArray{QJsonObject{{u"role"_s, u"system"_s}, {u"content"_s, spec.value(u"system")}},
                                   QJsonObject{{u"role"_s, u"user"_s}, {u"content"_s, example.value(u"menu")}},
                                   QJsonObject{{u"role"_s, u"assistant"_s}, {u"content"_s, example.value(u"lines")}},
                                   QJsonObject{{u"role"_s, u"user"_s}, {u"content"_s, said}}}},
        {u"temperature"_s, 0},
        {u"max_tokens"_s, 2000},
        {u"grammar"_s, spec.value(u"grammar")},   // only its lines, ended by END
        {u"stop"_s, QJsonArray{u"END"_s}}};
    QPointer<PosService> self(this);
    s_->vtaiCall(body, [self, parts, at, answers, request](const QJsonObject &reply, const QString &error) mutable {
        if (!self || request != self->vtaiRequest_)
            return;
        if (!error.isEmpty()) {
            self->vtai_ = {{u"task"_s, u"menu"_s}, {u"busy"_s, false}, {u"error"_s, error}};
            emit self->sessionChanged();
            return;
        }
        const QString content = reply.value(u"choices").toArray().first().toObject().value(u"message").toObject()
                                    .value(u"content").toString();
        const QJsonObject answer = parseVtAiLines(content);
        QString last;
        if (answer.value(u"items").toArray().isEmpty() && answer.value(u"groups").toArray().isEmpty()) {
            // (Cut off, or not the shape asked for: that part is said, the others still come.)
            QJsonObject none;
            answers.append(none);
        } else {
            answers.append(answer);
            const QJsonArray items = answer.value(u"items").toArray();
            if (!items.isEmpty())
                last = items.last().toObject().value(u"category").toString();
        }
        self->vtaiMenuPart(parts, at + 1, std::move(answers), request, last);
    });
}

bool PosService::vtaiCancel()
{
    if (!vtai_.value(u"busy"_s).toBool())
        return true;
    ++vtaiRequest_;
    if (vtaiReader_) {
        vtaiReader_->cancel();
        vtaiReader_->deleteLater();
    }
    if (s_->vtaiCancel)
        s_->vtaiCancel();
    vtai_.clear();
    emit sessionChanged();
    return true;
}

bool PosService::vtaiClear()
{
    if (vtai_.value(u"busy"_s).toBool())
        return vtaiCancel();
    vtai_.clear();
    emit sessionChanged();
    return true;
}

} // namespace vt::app
