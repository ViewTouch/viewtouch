# Install layout and .deb / .rpm packages (cpack). Configure with
# -DCMAKE_INSTALL_PREFIX=/usr; packaging/build-packages.sh does it all.

include(GNUInstallDirs)
set(VTM_PKG ${CMAKE_CURRENT_LIST_DIR})

# The Qt ViewTouch carries (tools/get-qt.sh: the newest Qt, the same on every
# system) rather than the distribution's: the program and its Qt in
# /usr/lib/vtmodern (bin/, lib/, plugins/, qml/, and qt.conf saying so),
# /usr/bin/vtmodern a link to it. On by itself when Qt isn't the system's.
if(Qt6_DIR MATCHES "^/usr/")
    set(VTM_BUNDLE_QT_DEFAULT OFF)
else()
    set(VTM_BUNDLE_QT_DEFAULT ON)
endif()
option(VTM_BUNDLE_QT "Install the Qt it was built with beside the program" ${VTM_BUNDLE_QT_DEFAULT})
if(VTM_BUNDLE_QT)
    set(VTM_QT_HOME lib/vtmodern)
    set_target_properties(vtmodern PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
    install(TARGETS vtmodern RUNTIME DESTINATION ${VTM_QT_HOME}/bin)
    install(CODE "
        file(MAKE_DIRECTORY \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\")
        file(CREATE_LINK ../${VTM_QT_HOME}/bin/vtmodern
             \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/vtmodern\" SYMBOLIC)")
    # What the program loads: Qt's libraries (and ICU), the plugins its modules
    # need (screens: Wayland, X11, offscreen; SQLite; image formats; TLS), and
    # the QML modules its pages import.
    # Plugins Qt's deploy doesn't pick by itself (only the default screen's):
    # the screens ViewTouch runs on (Wayland: the kiosk under cage; offscreen:
    # a server and the one-shot commands; EGLFS and the framebuffer: a screen
    # without a desktop) and SQLite's driver, the database itself. Installed
    # first, then given to the deploy below so their libraries come too.
    set(VTM_QT_PLUGINS platforms/libqwayland.so platforms/libqoffscreen.so platforms/libqeglfs.so
        platforms/libqminimal.so platforms/libqlinuxfb.so sqldrivers/libqsqlite.so)
    set(VTM_QT_PLUGIN_PATHS "")
    foreach(plugin ${VTM_QT_PLUGINS})
        get_filename_component(kind ${plugin} DIRECTORY)
        install(FILES ${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}/${plugin} DESTINATION ${VTM_QT_HOME}/plugins/${kind})
        string(APPEND VTM_QT_PLUGIN_PATHS " \"\${QT_DEPLOY_PREFIX}/${VTM_QT_HOME}/plugins/${plugin}\"")
    endforeach()
    # Qt SQL by name: on a computer that also has the system's Qt, the deploy
    # found that copy first and left it out as one of the system's libraries.
    file(REAL_PATH ${QT6_INSTALL_PREFIX}/${QT6_INSTALL_LIBS}/libQt6Sql.so.6 VTM_QT_SQL)
    install(FILES ${VTM_QT_SQL} DESTINATION ${VTM_QT_HOME}/lib RENAME libQt6Sql.so.6)
    # Plugin types left out: every database driver but SQLite's (installed
    # above; the others need MySQL's, Oracle's and others' client libraries),
    # QML debugging, and desktop themes (GTK's would bring GTK onto every Pi;
    # ViewTouch draws its own style).
    # (Qt's deploy works out the plugins' library path from QT_DEPLOY_LIB_DIR:
    # this layout is Qt's own, so the path they already have stays.)
    qt_generate_deploy_script(TARGET vtmodern OUTPUT_SCRIPT VTM_DEPLOY_SCRIPT CONTENT "
        set(QT_DEPLOY_BIN_DIR ${VTM_QT_HOME}/bin)
        set(QT_DEPLOY_LIB_DIR ${VTM_QT_HOME}/lib)
        set(QT_DEPLOY_PLUGINS_DIR ${VTM_QT_HOME}/plugins)
        set(QT_DEPLOY_QML_DIR ${VTM_QT_HOME}/qml)
        qt_deploy_qml_imports(TARGET vtmodern QML_DIR ${VTM_QT_HOME}/qml PLUGINS_FOUND vtm_qml_plugins)
        qt_deploy_runtime_dependencies(
            EXECUTABLE \"\${QT_DEPLOY_PREFIX}/${VTM_QT_HOME}/bin/$<TARGET_FILE_NAME:vtmodern>\"
            ADDITIONAL_MODULES \${vtm_qml_plugins}${VTM_QT_PLUGIN_PATHS}
            BIN_DIR ${VTM_QT_HOME}/bin
            LIB_DIR ${VTM_QT_HOME}/lib
            PLUGINS_DIR ${VTM_QT_HOME}/plugins
            QML_DIR ${VTM_QT_HOME}/qml
            EXCLUDE_PLUGIN_TYPES sqldrivers qmltooling platformthemes
            GENERATE_QT_CONF
            NO_TRANSLATIONS)
    ")
    install(SCRIPT ${VTM_DEPLOY_SCRIPT})
else()
    install(TARGETS vtmodern RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(PROGRAMS ${VTM_PKG}/vtmodern-setup DESTINATION ${CMAKE_INSTALL_SBINDIR})
install(PROGRAMS ${VTM_PKG}/vtmodern-lockdown DESTINATION ${CMAKE_INSTALL_SBINDIR})
install(PROGRAMS ${VTM_PKG}/vtmodern-device ${VTM_PKG}/vtmodern-screen DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/vtmodern)
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
install(FILES ${VTM_REPO_ROOT}/LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/vtmodern)

set(CPACK_PACKAGE_NAME vtmodern)
# The version with this build's number (0.9.1283), as the program shows it.
execute_process(COMMAND git -c safe.directory=* rev-list --count HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE VTM_PKG_BUILD OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT VTM_PKG_BUILD MATCHES "^[0-9]+$")
    set(VTM_PKG_BUILD 0)
endif()
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${VTM_PKG_BUILD})
set(CPACK_PACKAGE_VENDOR "ViewTouch")
set(CPACK_PACKAGE_CONTACT "ViewTouch <https://github.com/ViewTouch/viewtouch>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Restaurant point of sale with pages you design yourself")
set(CPACK_PACKAGE_DESCRIPTION
    "ViewTouch is a touch-screen point of sale for restaurants and bars. Every screen is a page of "
    "buttons you can move, restyle and extend in the built-in page editor. One machine keeps the "
    "data; other terminals, kitchen screens and a headless server join it over the network.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/ViewTouch/viewtouch")
set(CPACK_RESOURCE_FILE_LICENSE ${VTM_REPO_ROOT}/LICENSE)
set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
set(CPACK_STRIP_FILES ON)

# --- Debian, Ubuntu, Raspberry Pi OS ---
# (Recommended: the color emoji font, for "🌶 Spicy Wings" on the screens:
# Qt 6.9 and later pick it for emoji by itself. Receipts leave emoji out.)
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
# libcrypto: encrypted backups and TLS (loaded at run time).
set(VTM_DEB_COMMON "fonts-dejavu-core, systemd, libssl3 | libssl3t64, openssl, curl")
if(VTM_BUNDLE_QT)
    # Its own Qt: shlibdeps finds what that Qt needs from the system (fonts,
    # keyboard maps, OpenGL...), and leaves its libraries out (they're ours).
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS ${CMAKE_INSTALL_PREFIX}/${VTM_QT_HOME}/lib)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "${VTM_DEB_COMMON}")
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "cage, cups-client, fonts-inter, fonts-noto-color-emoji")
else()
    # The system's Qt: its QML modules and plugins are loaded at run time, so
    # shlibdeps can't see them.
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-templates, \
qml6-module-qtquick-layouts, qml6-module-qtquick-shapes, qml6-module-qtquick-dialogs, \
qml6-module-qtquick-window, qml6-module-qtqml-workerscript, libqt6sql6-sqlite, qt6-qpa-plugins, \
${VTM_DEB_COMMON}")
    # fonts-inter: the Modern style's font (Store Settings -> Screens -> Style).
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "cage, qt6-wayland, cups-client, fonts-inter, fonts-noto-color-emoji")
endif()
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
# openssl, curl: signed updates; util-linux: runuser (vtmodern-setup), not in a minimal Fedora.
if(VTM_BUNDLE_QT)
    # Its own Qt: what that Qt needs from the system is found by rpm itself;
    # its libraries are neither asked for nor offered to other packages.
    set(CPACK_RPM_PACKAGE_REQUIRES "dejavu-sans-fonts, openssl-libs, openssl, curl, util-linux")
    set(CPACK_RPM_SPEC_MORE_DEFINE
        "%global __provides_exclude_from ^${CMAKE_INSTALL_PREFIX}/${VTM_QT_HOME}/.*$
%global __requires_exclude ^(libQt6|libicu).*$")
else()
    set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtdeclarative, qt6-qtbase-gui, dejavu-sans-fonts, openssl-libs, openssl, curl, util-linux")
endif()
# systemd-sysusers creates the viewtouch account in the install script
set(CPACK_RPM_PACKAGE_REQUIRES_POST "systemd")
if(VTM_BUNDLE_QT)
    set(CPACK_RPM_PACKAGE_RECOMMENDS "cage, cups-client, rsms-inter-fonts, google-noto-color-emoji-fonts")
else()
    set(CPACK_RPM_PACKAGE_RECOMMENDS "cage, qt6-qtwayland, cups-client, rsms-inter-fonts, google-noto-color-emoji-fonts")
endif()
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE ${VTM_PKG}/postinst)
set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE ${VTM_PKG}/prerm)
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE ${VTM_PKG}/postrm)
set(CPACK_RPM_USER_FILELIST
    "%config(noreplace) /etc/viewtouch/server.conf" "%config(noreplace) /etc/viewtouch/kiosk.conf")
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    /usr/sbin /usr/lib/systemd /usr/lib/systemd/system /usr/lib/sysusers.d /usr/share/applications
    /usr/share/icons /usr/share/icons/hicolor /usr/share/icons/hicolor/scalable
    /usr/share/icons/hicolor/scalable/apps /usr/share/doc /usr/lib /usr/bin)

include(CPack)
