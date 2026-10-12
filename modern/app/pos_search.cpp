// PosService: finding any check, today's or from earlier days - by number,
// amount, table, customer, server, item or gift card - and reprinting it.

#include "app/pos_json.hh"
#include "app/i18n.hh"
#include "app/pos_service.hh"

#include <QDateTime>
#include <QPointer>
#include <QRegularExpression>
#include <QThreadPool>

#include <algorithm>

using namespace Qt::StringLiterals;
using namespace vt::core;

namespace vt::app {

namespace {

constexpr int kPageScan = 100;   // checks read from the database a page at a time
constexpr std::int64_t kDayMs = 24LL * 3'600'000;

bool contains(const std::string &text, const QString &query)
{
    return QString::fromStdString(text).contains(query, Qt::CaseInsensitive);
}

// "#123" or "123": that check; "17.62" or "$17.62": that total; else text
// in the table, customer, phone, server, items or gift card numbers.
bool matches(const Check &c, const QString &q, const TaxRates &rates)
{
    static const QRegularExpression number(u"^#?(\\d{1,9})$"_s);
    static const QRegularExpression amount(u"^\\$?(\\d+)\\.(\\d\\d)$"_s);
    if (const auto m = number.match(q); m.hasMatch()) {
        if (c.id == m.captured(1).toLongLong())
            return true;
        if (q.startsWith(u'#'))
            return false;
        // Digits: also a phone number.
        return QString::fromStdString(CustomerRecord::digits(c.customer.phone)).contains(m.captured(1));
    }
    if (const auto m = amount.match(q); m.hasMatch()) {
        const std::int64_t cents = m.captured(1).toLongLong() * 100 + m.captured(2).toLongLong();
        const Totals t = c.totals(rates);
        return t.total.cents() == cents || (t.total + t.tips).cents() == cents;
    }
    if (contains(c.label, q) || contains(c.customer.name, q) || contains(c.customer.phone, q) || contains(c.serverName, q))
        return true;
    for (const OrderLine &l : c.lines)
        if (contains(l.name, q))
            return true;
    for (const Payment &p : c.payments)
        if (contains(p.reference, q) || contains(p.tenderName, q))
            return true;
    return false;
}

} // namespace

bool PosService::searchChecks(const QString &query, int days, int offset)
{
    if (!require(perm::Settle, tr("Finding checks")))
        return false;
    const QString q = query.trimmed();
    if (q.size() == 1)
        return fail(tr("Type at least two letters or digits."));
    // 0: every check ever; else the last `days` days (up to a century).
    days = days <= 0 ? 0 : std::clamp(days, 1, 36600);
    offset = std::max(0, offset);
    const int request = ++searchRequest_;
    searchHits_.clear();
    selectedHit_ = 0;
    checkSearch_ = {{u"query"_s, q}, {u"loading"_s, true}, {u"days"_s, days}, {u"offset"_s, offset}};
    emit sessionChanged();

    // Open checks (and today's, if there's no database) are here; the rest
    // is read from the database away from the screen, a page at a time.
    std::vector<Check> here;
    if (offset == 0) {
        if (!q.isEmpty())
            for (const auto &[id, c] : s_->open)
                here.push_back(c);
        if (!s_->findChecks)   // the database has today's too
            for (const Check &c : s_->closedToday)
                here.push_back(c);
    }
    // The database's search, or (without one) the range reports' history read whole.
    auto find = s_->findChecks;
    if (!find && s_->history) {
        const auto history = s_->history;
        const std::int64_t until = now() + kDayMs;
        find = [history, until](const PosShared::CheckFind &f) {
            std::vector<Check> all = history(f.from, until);
            std::ranges::stable_sort(all, std::greater{}, &Check::closedAt);
            std::vector<Check> page;
            for (std::size_t i = std::size_t(f.offset); i < all.size() && int(page.size()) < f.limit; ++i)
                page.push_back(std::move(all[i]));
            return page;
        };
    }
    const TaxRates rates = s_->settings.tax;
    PosShared::CheckFind range;
    range.from = days ? now() - std::int64_t(days) * kDayMs : 0;
    range.limit = kPageScan;
    range.offset = offset;
    // Words the saved record must contain (an amount or a phone number in
    // other punctuation can't be looked for that way: every record is read).
    static const QRegularExpression number(u"^#(\\d{1,9})$"_s);
    static const QRegularExpression digitsOrAmount(u"^\\$?[\\d.() -]+$"_s);
    if (const auto m = number.match(q); m.hasMatch())
        range.words << u"\"id\":%1,"_s.arg(m.captured(1));
    else if (!q.isEmpty() && !digitsOrAmount.match(q).hasMatch())
        range.words << q;
    QPointer<PosService> self(this);
    QThreadPool::globalInstance()->start([=, here = std::move(here)]() mutable {
        std::vector<Check> hits;
        for (Check &c : here)
            if (c.status != CheckStatus::Discarded && (q.isEmpty() || matches(c, q, rates)))
                hits.push_back(std::move(c));
        bool more = false;
        if (find) {
            std::vector<Check> page = find(range);
            more = int(page.size()) == range.limit;
            for (Check &c : page)
                if ((q.isEmpty() || matches(c, q, rates)) && std::ranges::none_of(hits, [&](const Check &x) { return x.id == c.id; }))
                    hits.push_back(std::move(c));
        }
        std::ranges::stable_sort(hits, std::greater{}, [](const Check &c) { return c.closedAt ? c.closedAt : c.openedAt; });
        QMetaObject::invokeMethod(self, [self, request, more, next = offset + range.limit, hits = std::move(hits)]() mutable {
            if (!self || request != self->searchRequest_)
                return;   // a newer search replaced it
            self->searchHits_ = std::move(hits);
            self->checkSearch_.insert(u"loading"_s, false);
            self->checkSearch_.insert(u"more"_s, more);
            self->checkSearch_.insert(u"nextOffset"_s, next);
            emit self->sessionChanged();
        }, Qt::QueuedConnection);
    });
    return true;
}

void PosService::selectFoundCheck(qint64 id)
{
    selectedHit_ = id;
    emit sessionChanged();
}

bool PosService::reprintCheck(qint64 id)
{
    if (!require(perm::Settle, tr("Printing receipts")))
        return false;
    const auto it = std::ranges::find_if(searchHits_, [&](const Check &c) { return c.id == id; });
    if (it == searchHits_.end())
        return fail(tr("Find the check first."));
    if (!s_->printer)
        return fail(tr("No printer is set up."));
    if (askReceiptPrinter()) {   // a handheld: where?
        offerReceipt(*it, true);
        return true;
    }
    s_->printer->printReceipt(s_->settings, *it, receiptPrinter());
    emit notice(tr("Printing a copy of check #%1").arg(id));
    return true;
}

QVariantMap PosService::checkSearch() const
{
    if (checkSearch_.isEmpty())
        return {};
    const auto when = [](std::int64_t ms) {
        return ms ? i18n::locale().toString(QDateTime::fromMSecsSinceEpoch(ms), u"ddd MMM d, h:mm AP"_s) : QString();
    };
    const auto statusText = [this](const Check &c) {
        return c.status == CheckStatus::Open ? tr("open") : c.status == CheckStatus::Merged ? tr("merged") : tr("closed");
    };
    QVariantList results;
    for (const Check &c : searchHits_) {
        const Totals t = c.totals(s_->settings.tax);
        results.append(QVariantMap{{u"id"_s, qint64(c.id)}, {u"label"_s, qs(c.label)},
                                   {u"when"_s, when(c.closedAt ? c.closedAt : c.openedAt)},
                                   {u"server"_s, qs(c.serverName)}, {u"customer"_s, qs(c.customer.name)},
                                   {u"total"_s, format(t.total)}, {u"status"_s, statusText(c)},
                                   {u"refunded"_s, !c.refunds.empty()}});
    }
    QVariantMap out = checkSearch_;
    out.insert(u"results"_s, results);
    const auto sel = std::ranges::find_if(searchHits_, [&](const Check &c) { return c.id == selectedHit_; });
    if (sel != searchHits_.end()) {
        const Check &c = *sel;
        const Totals t = c.totals(s_->settings.tax);
        QVariantList lines;
        for (const OrderLine &l : c.lines) {
            QStringList mods;
            for (const Modifier &m : l.modifiers)
                mods << qs(m.displayName());
            lines.append(QVariantMap{{u"name"_s, qs(l.displayName())}, {u"quantity"_s, l.quantity},
                                     {u"price"_s, l.isComment() ? QString() : format(l.voided ? Money() : l.total())},
                                     {u"voided"_s, l.voided}, {u"modifiers"_s, mods.join(u", "_s)}});
        }
        QVariantList payments;
        for (const Payment &p : c.payments) {
            QString name = qs(p.tenderName);
            if (!p.last4.empty())
                name += u"  ·  "_s + qs(p.cardBrand).toUpper() + u" •••• "_s + qs(p.last4);
            else if (!p.reference.empty() && p.kind != TenderKind::HouseAccount)
                name += u" ("_s + qs(p.reference) + u')';
            // What can still be given back (paid, tip included, less what was).
            const Money back = s_->refundedSoFar(c, p.id);   // today's from any terminal too
            const Money left = c.refundable(p, s_->settings.tax) - back;   // the bill: tips aren't refunded
            const bool refundable = c.status == CheckStatus::Closed && p.kind != TenderKind::Discount
                                    && p.kind != TenderKind::GiftCard && p.kind != TenderKind::HouseAccount && left.cents() > 0;
            payments.append(QVariantMap{{u"id"_s, qint64(p.id)}, {u"name"_s, name},
                                        {u"amount"_s, p.kind == TenderKind::Discount ? QString() : format(p.amount)},
                                        {u"tip"_s, p.tip.cents() ? format(p.tip) : QString()},
                                        {u"change"_s, c.changeFrom(p, s_->settings.tax).cents()
                                                          ? format(c.changeFrom(p, s_->settings.tax)) : QString()},
                                        {u"refunded"_s, back.cents() ? format(back) : QString()},
                                        {u"refundable"_s, refundable}, {u"leftCents"_s, qint64(left.cents())},
                                        {u"left"_s, format(left)},
                                        {u"how"_s, p.processor == "stripe" ? u"stripe"_s
                                                   : p.kind == TenderKind::Cash ? u"cash"_s : u"record"_s}});
        }
        // Everything that happened to it, oldest first: the audit trail.
        QVariantList events;
        for (const CheckEvent &e : c.events)
            events.append(QVariantMap{{u"when"_s, when(e.at)}, {u"who"_s, qs(e.who)}, {u"what"_s, e.reason.empty() ? qs(e.what) : u"%1 · %2"_s.arg(qs(e.what), qs(e.reason))},
                                      {u"kind"_s, qs(e.kind)}});
        QVariantList refunds;
        for (const Refund &r : c.refunds)
            refunds.append(QVariantMap{{u"when"_s, when(r.at)}, {u"amount"_s, format(r.amount)}, {u"by"_s, qs(r.by)},
                                       {u"reason"_s, qs(r.reason)}, {u"tender"_s, qs(r.tenderName)},
                                       {u"reference"_s, qs(r.reference)}});
        out.insert(u"selected"_s, QVariantMap{
            {u"id"_s, qint64(c.id)}, {u"label"_s, qs(c.label)}, {u"server"_s, qs(c.serverName)},
            {u"opened"_s, when(c.openedAt)}, {u"closed"_s, when(c.closedAt)}, {u"status"_s, statusText(c)},
            {u"customer"_s, qs(c.customer.name)}, {u"guests"_s, c.guests},
            {u"lines"_s, lines}, {u"payments"_s, payments}, {u"events"_s, events}, {u"refunds"_s, refunds},
            {u"subtotal"_s, format(t.subtotal)}, {u"tax"_s, format(t.tax)}, {u"total"_s, format(t.total)},
            {u"tips"_s, t.tips.cents() ? format(t.tips) : QString()}});
    }
    return out;
}

} // namespace vt::app
