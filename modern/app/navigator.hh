#pragma once

#include "layout/layout.hh"

#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace vt::app {

// Page navigation for one terminal: current page, the return stack, and the
// last index page visited. Semantics follow the legacy Terminal::Jump:
//
//   push      remember the current page, then go (legacy JUMP_NORMAL)
//   replace   go without remembering (legacy JUMP_STEALTH)
//   back      return to the remembered page (legacy JUMP_RETURN)
//   sequence  continue a modifier sequence; same as back (legacy JUMP_SCRIPT)
//   home      clear the stack and go home (legacy JUMP_HOME)
//   index     go to the last index page visited (legacy JUMP_INDEX)
class Navigator {
public:
    enum class Mode { Push, Replace, Back, Sequence, Home, Index };
    static constexpr int MaxDepth = 32;

    explicit Navigator(const layout::Layout &layout);

    // Point at another layout (the editor's draft while editing). The stack
    // is kept; callers re-validate the current page.
    void setLayout(const layout::Layout &layout) { layout_ = &layout; }

    static std::optional<Mode> parseMode(const QString &mode);

    QString current() const { return current_; }
    QString home() const { return home_; }
    QString lastIndex() const { return lastIndex_; }

    // Meal period used by `index` when no index page has been visited yet;
    // the index page with this mealPeriod is preferred over the first one.
    void setMealPeriod(const QString &period) { mealPeriod_ = period; }
    QString mealPeriod() const { return mealPeriod_; }
    int depth() const { return int(stack_.size()); }
    bool canGoBack() const { return !stack_.isEmpty(); }

    // Change the home page without moving (e.g. the login page was renamed).
    void setHome(const QString &homePageId) { home_ = homePageId; }

    // Set the home page and go there with an empty stack.
    void reset(const QString &homePageId);

    // Returns true when the current page changed.
    bool jump(Mode mode, const QString &target = {});

    // Visit `pages` in order, then come back to the current page (legacy
    // RunScript). Each modifier page ends with a `sequence` jump.
    bool startSequence(const QStringList &pages);

private:
    void enter(const QString &pageId);
    void push(const QString &pageId);

    const layout::Layout *layout_;
    QString current_;
    QString home_;
    QString lastIndex_;
    QString mealPeriod_;
    QList<QString> stack_;
};

} // namespace vt::app
