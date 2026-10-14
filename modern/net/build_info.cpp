#include "net/build_info.hh"

#include "vtm_build_number.hh"   // generated: VTM_BUILD_NUMBER, VTM_BUILD_COMMIT, VTM_BUILD_VERSION, VTM_BUILD_MODIFIED

#include <QCoreApplication>
#include <QDateTime>
#include <QSysInfo>
#include <QVariantMap>

namespace vt::net {

AppBuild appBuild()
{
#if defined(Q_OS_ANDROID)
    const QString os = QStringLiteral("android");
#elif defined(Q_OS_WIN)
    const QString os = QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    const QString os = QStringLiteral("macos");
#else
    const QString os = QStringLiteral("linux");
#endif
    return {QStringLiteral(VTM_BUILD_VERSION), VTM_BUILD_NUMBER, QStringLiteral(VTM_BUILD_COMMIT),
            os + u'-' + QSysInfo::buildCpuArchitecture(), VTM_BUILD_MODIFIED != 0};
}

QString buildText(const QString &version, int number, const QString &commit, bool modified)
{
    // The build number is in the version (0.9.1283); an older style isn't (0.7.0).
    QString text = version.endsWith(u'.' + QString::number(number))
                       ? version : QCoreApplication::translate("Build", "%1, build %2").arg(version).arg(number);
    QStringList more;
    if (!commit.isEmpty())
        more << commit;
    if (modified)
        more << QCoreApplication::translate("Build", "modified");
    if (!more.isEmpty())
        text += QStringLiteral(" (%1)").arg(more.join(QStringLiteral(", ")));
    return text;
}

QVariantList appHistory()
{
    static const char history[] =
#include "vtm_history.inc"   // generated: a line a build, fields apart by \x1f
        ;
    QVariantList out;
    for (const QString &line : QString::fromUtf8(history).split(u'\n', Qt::SkipEmptyParts)) {
        const QStringList f = line.split(QChar(0x1f));
        if (f.size() < 5)
            continue;
        out.append(QVariantMap{{QStringLiteral("number"), f[0].toInt()}, {QStringLiteral("version"), f[1]},
                               {QStringLiteral("commit"), f[2]},
                               {QStringLiteral("at"), QDateTime::fromString(f[3], Qt::ISODate).toMSecsSinceEpoch()},
                               {QStringLiteral("change"), f.mid(4).join(QChar(0x1f))}});
    }
    return out;
}

} // namespace vt::net
