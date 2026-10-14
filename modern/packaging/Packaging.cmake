# Install layout and .deb / .rpm packages (cpack). Configure with
# -DCMAKE_INSTALL_PREFIX=/usr; packaging/build-packages.sh does it all.

include(GNUInstallDirs)
set(VTM_PKG ${CMAKE_CURRENT_LIST_DIR})

install(TARGETS vtmodern RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS ${VTM_PKG}/vtmodern-setup DESTINATION ${CMAKE_INSTALL_SBINDIR})
install(PROGRAMS ${VTM_PKG}/vtmodern-lockdown DESTINATION ${CMAKE_INSTALL_SBINDIR})
install(PROGRAMS ${VTM_PKG}/vtmodern-device DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/vtmodern)
# The key ViewTouch packages are signed with: "Update from USB" installs only signed ones.
# Where to look for a new one online (a fork sets its own: release-channel.sh).
install(FILES ${VTM_PKG}/update-signing-key.pub.pem ${VTM_PKG}/release-channel.conf
        DESTINATION ${CMAKE_INSTALL_DATADIR}/vtmodern)
# systemd looks in <prefix>/lib even where libraries go to lib64
install(FILES ${VTM_PKG}/vtmodern.service ${VTM_PKG}/vtmodern-kiosk.service ${VTM_PKG}/vtmodern-station.service
        DESTINATION lib/systemd/system)
install(FILES ${VTM_PKG}/viewtouch.sysusers DESTINATION lib/sysusers.d RENAME viewtouch.conf)
install(FILES ${VTM_PKG}/server.conf ${VTM_PKG}/kiosk.conf DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/viewtouch)
install(FILES ${VTM_PKG}/vtmodern.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${VTM_PKG}/vtmodern.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/vtmodern)
install(FILES ${VTM_LEGACY_ROOT}/LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/vtmodern)

set(CPACK_PACKAGE_NAME vtmodern)
# The version with this build's number (0.9.1283), as the program shows it.
execute_process(COMMAND git -c safe.directory=* rev-list --count HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE VTM_PKG_BUILD OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT VTM_PKG_BUILD MATCHES "^[0-9]+$")
    set(VTM_PKG_BUILD 0)
endif()
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${VTM_PKG_BUILD})
set(CPACK_PACKAGE_VENDOR "ViewTouch")
set(CPACK_PACKAGE_CONTACT "ViewTouch <https://github.com/No0ne558/viewtouchFork>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Restaurant point of sale with pages you design yourself")
set(CPACK_PACKAGE_DESCRIPTION
    "ViewTouch is a touch-screen point of sale for restaurants and bars. Every screen is a page of "
    "buttons you can move, restyle and extend in the built-in page editor. One machine keeps the "
    "data; other terminals, kitchen screens and a headless server join it over the network.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/No0ne558/viewtouchFork")
set(CPACK_RESOURCE_FILE_LICENSE ${VTM_LEGACY_ROOT}/LICENSE)
set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
set(CPACK_STRIP_FILES ON)

# --- Debian, Ubuntu, Raspberry Pi OS ---
# Debian and Ubuntu builds differ (Qt versions): name the release after the
# distribution, e.g. vtmodern_0.7.0-1~debian13_arm64.deb.
set(VTM_OS_TAG "")
if(EXISTS /etc/os-release)
    file(STRINGS /etc/os-release VTM_OS_ID REGEX "^ID=")
    file(STRINGS /etc/os-release VTM_OS_VERSION REGEX "^VERSION_ID=")
    string(REGEX REPLACE "^ID=\"?([^\"]*)\"?$" "\\1" VTM_OS_ID "${VTM_OS_ID}")
    string(REGEX REPLACE "^VERSION_ID=\"?([^\"]*)\"?$" "\\1" VTM_OS_VERSION "${VTM_OS_VERSION}")
    set(VTM_OS_TAG "~${VTM_OS_ID}${VTM_OS_VERSION}")
endif()
set(CPACK_DEBIAN_PACKAGE_RELEASE "1${VTM_OS_TAG}")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION misc)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# QML modules and Qt plugins are loaded at run time, so shlibdeps can't see them.
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-templates, \
qml6-module-qtquick-layouts, qml6-module-qtquick-shapes, qml6-module-qtquick-dialogs, \
qml6-module-qtquick-window, qml6-module-qtqml-workerscript, libqt6sql6-sqlite, qt6-qpa-plugins, \
fonts-dejavu-core, systemd, libssl3 | libssl3t64, openssl, curl")   # libcrypto: encrypted backups (loaded at run time)
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "cage, qt6-wayland, cups-client")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/debian/conffiles
     "/etc/viewtouch/server.conf\n/etc/viewtouch/kiosk.conf\n")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    ${VTM_PKG}/postinst ${VTM_PKG}/prerm ${VTM_PKG}/postrm ${CMAKE_CURRENT_BINARY_DIR}/debian/conffiles)
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)

# --- Fedora ---
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
set(CPACK_RPM_PACKAGE_RELOCATABLE OFF)
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Productivity")
set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtdeclarative, qt6-qtbase-gui, dejavu-sans-fonts, openssl-libs")
# systemd-sysusers creates the viewtouch account in the install script
set(CPACK_RPM_PACKAGE_REQUIRES_POST "systemd")
set(CPACK_RPM_PACKAGE_RECOMMENDS "cage, qt6-qtwayland, cups-client")
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE ${VTM_PKG}/postinst)
set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE ${VTM_PKG}/prerm)
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE ${VTM_PKG}/postrm)
set(CPACK_RPM_USER_FILELIST
    "%config(noreplace) /etc/viewtouch/server.conf" "%config(noreplace) /etc/viewtouch/kiosk.conf")
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    /usr/sbin /usr/lib/systemd /usr/lib/systemd/system /usr/lib/sysusers.d /usr/share/applications
    /usr/share/icons /usr/share/icons/hicolor /usr/share/icons/hicolor/scalable
    /usr/share/icons/hicolor/scalable/apps /usr/share/doc)

include(CPack)
