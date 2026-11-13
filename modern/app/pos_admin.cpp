// PosService: the manager screens are in pos_admin_fields.cpp (what each
// record has), pos_admin_records.cpp (the records), pos_admin_save.cpp
// (saving them) and pos_changes.cpp (the Changes report). Here: pairing
// devices, a factory reset, a backup on demand, and this screen's settings.

#include "app/pos_admin_shared.hh"

#include <QPointer>
#include <QThreadPool>
#include <QDateTime>
#include <QRandomGenerator>
#include <QTimer>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

using namespace admin;

// --- pairing devices -------------------------------------------------------------

namespace {
// Crockford base32: no I, L, O or U to mix up. 10 characters = 50 bits,
// stretched by the key derivation on both sides (see net/pairing).
QString newPairingCode()
{
    static const char alphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
    QString code;
    for (int i = 0; i < 10; ++i) {
        if (i == 5)
            code += u'-';
        code += QChar::fromLatin1(alphabet[QRandomGenerator::system()->bounded(32)]);
    }
    return code;
}
} // namespace

QString PosShared::startPairing(bool standby)
{
    pairing = Pairing{newPairingCode(), now() + 10 * 60 * 1000, standby};
    ++adminRevision;
    emit adminChanged();
    // Take the code off the manager screens when it runs out.
    QTimer::singleShot(10 * 60 * 1000 + 500, this, [this, code = pairing->code] {
        if (pairing && pairing->code == code && !activePairing()) {
            pairing.reset();
            ++adminRevision;
            emit adminChanged();
        }
    });
    return pairing->code;
}

bool PosService::factoryReset(const QString &confirm, bool empty)
{
    if (!require(perm::Maintain, tr("Factory reset")))
        return false;
    if (confirm.trimmed() != u"RESET")
        return fail(tr("Type RESET to confirm."));
    if (!s_->requestFactoryReset)
        return fail(tr("A factory reset is done on the computer that keeps the data."));
    if (!s_->requestFactoryReset(empty))
        return fail(tr("The backup failed, so nothing was reset."));
    emit notice(tr("Backed up. Resetting and restarting..."));
    return true;
}

bool PosService::legacyStart(const QString &folder, const QVariantMap &parts, bool apply)
{
    if (!require(perm::Owner, tr("A legacy store's menu and sales")))
        return false;
    if (!s_->legacyRead || !s_->legacyFinish)
        return fail(tr("A legacy store's menu and sales are brought over on the computer that keeps the data."));
    if (legacy_.value(u"busy"_s).toBool())
        return fail(tr("Still reading the folder."));
    const int request = ++legacyRequest_;
    legacy_.remove(u"error"_s);
    legacy_.insert(u"folder"_s, folder);
    legacy_.insert(u"busy"_s, apply ? tr("Bringing it over...") : tr("Reading the folder..."));
    emit sessionChanged();
    // Read and worked out away from the screen (a year of days takes
    // seconds); brought in here.
    QPointer<PosService> self(this);
    QThreadPool::globalInstance()->start([self, request, folder, parts, apply, read = s_->legacyRead,
                                          settings = s_->settings] {
        const std::any work = read(folder, parts, settings);
        QMetaObject::invokeMethod(self, [self, request, work, apply] {
            if (!self || request != self->legacyRequest_)
                return;
            const QVariantMap result = self->s_->legacyFinish(work, apply);
            self->legacy_.remove(u"busy"_s);
            for (auto it = result.begin(); it != result.end(); ++it)
                self->legacy_.insert(it.key(), it.value());
            emit self->sessionChanged();
            if (const QString error = result.value(u"error"_s).toString(); !error.isEmpty())
                emit self->notice(error);
            else if (apply)
                emit self->notice(tr("Brought over from legacy ViewTouch."));
        });
    });
    return true;
}

bool PosService::legacyPreviewOf(const QString &folder)
{
    return legacyStart(folder, {}, false);
}

bool PosService::legacyImportRun(const QString &folder, const QVariantMap &parts)
{
    return legacyStart(folder, parts, true);
}

void PosService::legacyClearState()
{
    ++legacyRequest_;   // (an answer still on its way: not wanted)
    legacy_.clear();
    emit sessionChanged();
}

bool PosService::backupNow()
{
    if (!require(perm::Manager, tr("Backups")))
        return false;
    if (!s_->requestBackup || !s_->requestBackup())
        return fail(tr("Backups aren't set up on this store, or one is already running."));
    emit notice(tr("Backing up..."));
    return true;
}

bool PosService::startPairing(bool standby)
{
    if (!require(standby ? perm::Maintain : perm::Manager, standby ? tr("Pairing a standby computer") : tr("Pairing a device")))
        return false;
    s_->startPairing(standby);
    emit notice(standby ? tr("On the standby computer: vtmodern-setup standby %1. The code works once, for 10 minutes.").arg(s_->pairing->code)
                        : tr("Type %1 on the new device. The code works once, for 10 minutes.").arg(s_->pairing->code));
    return true;
}

bool PosService::stopPairing()
{
    if (!require(perm::Manager, tr("Pairing a device")))
        return false;
    s_->pairing.reset();
    ++s_->adminRevision;
    emit s_->adminChanged();
    return true;
}

QString PosService::terminalLook() const
{
    // The person's own, while they're logged in.
    if (const Employee *e = user(); e && !e->look.empty())
        return qs(e->look);
    for (const TerminalConfig &t : s_->settings.terminals)
        if (qs(t.name) == terminal_)
            return qs(t.look);
    return {};
}

QString PosService::terminalStartCategory() const
{
    for (const TerminalConfig &t : s_->settings.terminals)
        if (qs(t.name) == terminal_)
            return qs(t.startCategory);
    return {};
}

QString PosService::terminalKeyboard() const
{
    for (const TerminalConfig &t : s_->settings.terminals)
        if (qs(t.name) == terminal_)
            return qs(t.keyboard);
    return {};
}

QString PosService::terminalGuestScreen() const
{
    for (const TerminalConfig &t : s_->settings.terminals)
        if (qs(t.name) == terminal_)
            return qs(t.guestScreen);
    return {};
}

QString PosService::screenMode() const
{
    for (const TerminalConfig &t : s_->settings.terminals) {
        if (qs(t.name) == terminal_)
            return qs(t.screen);
    }
    return {};
}

QVariantMap PosService::pairingInfo() const
{
    const PosShared::Pairing *p = s_->activePairing();
    if (!p || !can(QString::fromLatin1(perm::Manager)))
        return {{u"active"_s, false}};
    const QString until = i18n::timeText(QDateTime::fromMSecsSinceEpoch(p->expires).time());
    return {{u"active"_s, true}, {u"code"_s, p->code}, {u"until"_s, until}, {u"standby"_s, p->standby}};
}

void PosService::settingsChanged()
{
    if (s_->sink)
        s_->sink->saveSettings(s_->settings);
    // Tax changes re-total open checks.
    emit checkChanged();
    emit s_->checksChanged();
}

} // namespace vt::app
