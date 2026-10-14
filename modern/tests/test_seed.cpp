#include <catch2/catch_test_macros.hpp>

#include "layout/layout.hh"
#include "qt_catch.hh"

using namespace Qt::StringLiterals;
using namespace vt::layout;

TEST_CASE("Shipped seed layout loads and validates cleanly", "[seed]")
{
    QStringList errors;
    const auto layout = Layout::loadDirectory(QStringLiteral(VTM_SEED_DIR), &errors);
    REQUIRE(layout);
    CHECK(errors == QStringList{});
    CHECK(layout->validate() == QStringList{});

    for (const char *role : {"login", "tables", "guestCount", "settle", "logout", "manager"})
        CHECK(layout->pageByRole(QString::fromLatin1(role)));

    // Every index / item / modifier page inherits the order template.
    for (const Page &p : layout->pages) {
        if (p.kind == u"index" || p.kind == u"items" || p.kind == u"modifier") {
            INFO(p.id.toStdString());
            CHECK(p.templateId == u"order-template"_s);
        }
    }
}
