#include <catch2/catch_session.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <catch2/catch_test_case_info.hpp>

#include "app/i18n.hh"

#include <QGuiApplication>
#include <QMutex>
#include <QQuickStyle>
#include <QtQml/qqmlextensionplugin.h>

#include <cstdio>

Q_IMPORT_QML_PLUGIN(ViewTouchPlugin)

// A QML warning in any test (a binding that failed, a property that isn't
// there): the screen shows a blank or does nothing, and no check notices.
// Each is noted with the test it came in, and the run fails at the end.
namespace {

QMutex g_qmlMutex;
QString g_testName;
QStringList g_qmlWarnings;
QtMessageHandler g_previous = nullptr;

void noteQmlWarnings(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const QString category = QString::fromLatin1(context.category ? context.category : "");
    if (type == QtWarningMsg && (category == u"qml" || category == u"js" || message.contains(u".qml:"))) {
        QMutexLocker lock(&g_qmlMutex);
        g_qmlWarnings << g_testName + QStringLiteral(": ") + message;
    }
    if (g_previous)
        g_previous(type, context, message);
}

class QmlWarningListener : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;
    void testCaseStarting(const Catch::TestCaseInfo &info) override
    {
        QMutexLocker lock(&g_qmlMutex);
        g_testName = QString::fromStdString(std::string(info.name));
    }
};
CATCH_REGISTER_LISTENER(QmlWarningListener)

} // namespace

// Qt plugins (SQLite driver) need an application object, and the UI tests
// need a GUI one. Default to the offscreen platform so tests run headless.
int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    // Warnings (QML errors among them) on the screen, not the system journal.
    qputenv("QT_FORCE_STDERR_LOGGING", "1");
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle(QStringLiteral("Fusion"));
    vt::i18n::install();   // as the app does: English plurals, Spanish for those who use it
    g_previous = qInstallMessageHandler(noteQmlWarnings);
    int result = Catch::Session().run(argc, argv);
    if (!g_qmlWarnings.isEmpty()) {
        std::fprintf(stderr, "\n%lld QML warning(s) during the tests:\n", qlonglong(g_qmlWarnings.size()));
        for (const QString &w : g_qmlWarnings)
            std::fprintf(stderr, "  %s\n", qPrintable(w));
        result = result ? result : 1;
    }
    return result;
}
