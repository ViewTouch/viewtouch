#include "app/navigator.hh"

using namespace Qt::StringLiterals;

namespace vt::app {

Navigator::Navigator(const layout::Layout &layout)
    : layout_(&layout)
{
}

std::optional<Navigator::Mode> Navigator::parseMode(const QString &mode)
{
    if (mode.isEmpty() || mode == u"push") return Mode::Push;
    if (mode == u"replace") return Mode::Replace;
    if (mode == u"back") return Mode::Back;
    if (mode == u"sequence") return Mode::Sequence;
    if (mode == u"home") return Mode::Home;
    if (mode == u"index") return Mode::Index;
    return std::nullopt;
}

void Navigator::reset(const QString &homePageId)
{
    home_ = homePageId;
    stack_.clear();
    lastIndex_.clear();
    enter(homePageId);
}

bool Navigator::jump(Mode mode, const QString &target)
{
    const QString before = current_;

    switch (mode) {
    case Mode::Push:
        if (!layout_->page(target))
            return false;
        push(current_);
        enter(target);
        break;
    case Mode::Replace:
        if (!layout_->page(target))
            return false;
        enter(target);
        break;
    case Mode::Back:
    case Mode::Sequence:
        // Skip pages deleted or renamed (in the editor) since they were pushed.
        while (!stack_.isEmpty() && !layout_->page(stack_.last()))
            stack_.removeLast();
        if (stack_.isEmpty())
            enter(home_);
        else
            enter(stack_.takeLast());
        break;
    case Mode::Home:
        stack_.clear();
        enter(home_);
        break;
    case Mode::Index: {
        QString idx = lastIndex_;
        if (idx.isEmpty()) {
            for (const layout::Page &p : layout_->pages) {
                if (p.kind == u"index" && !mealPeriod_.isEmpty() && p.mealPeriod == mealPeriod_) {
                    idx = p.id;
                    break;
                }
            }
        }
        if (idx.isEmpty()) {   // no page for this period: an all-day one, else the first
            for (const layout::Page &p : layout_->pages) {
                if (p.kind == u"index" && p.mealPeriod == u"all") {
                    idx = p.id;
                    break;
                }
            }
        }
        if (idx.isEmpty()) {
            if (const layout::Page *p = layout_->firstPageOfKind(u"index"_s))
                idx = p->id;
        }
        if (idx.isEmpty())
            return false;
        enter(idx);
        break;
    }
    }
    return current_ != before;
}

bool Navigator::startSequence(const QStringList &pages)
{
    QStringList valid;
    for (const QString &p : pages) {
        if (layout_->page(p))
            valid.append(p);
    }
    if (valid.isEmpty())
        return false;

    push(current_);
    for (qsizetype i = valid.size() - 1; i > 0; --i)
        push(valid[i]);
    enter(valid.first());
    return true;
}

void Navigator::enter(const QString &pageId)
{
    current_ = pageId;
    if (const layout::Page *p = layout_->page(pageId); p && p->kind == u"index")
        lastIndex_ = pageId;
}

void Navigator::push(const QString &pageId)
{
    if (pageId.isEmpty())
        return;
    if (stack_.size() >= MaxDepth)
        stack_.removeFirst();
    stack_.append(pageId);
}

} // namespace vt::app
