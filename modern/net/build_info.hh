#pragma once

#include <QString>
#include <QVariantList>

namespace vt::net {

// This program's build: its version (0.9.1283: MAJOR.MINOR and the build
// number), build number (commits so far: a newer build is a higher number),
// commit, whether it was built with changes not committed yet, and what it
// runs on ("android-arm64", "linux-x86_64"), to tell screens that need an update.
struct AppBuild {
    QString version;
    int number = 0;
    QString commit;
    QString platform;
    bool modified = false;
};

AppBuild appBuild();
// "0.9.1283 (a13bde2)", "0.9.1283 (a13bde2, modified)"; an older style of
// version with its build: "0.7.0, build 812 (a13bde2)".
QString buildText(const QString &version, int number, const QString &commit = {}, bool modified = false);
// The recent builds that changed the program, newest first (built in at
// build time): [{number, version, commit, at (ms), change}].
QVariantList appHistory();

} // namespace vt::net
