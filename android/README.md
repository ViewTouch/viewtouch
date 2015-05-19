This directory contains patches for XSDL X server for Android,
that will add fonts used by Viewtouch, force display resolution to 1920x1080,
remove MIPS architecture, disable touchpad mode and gyroscope,
and add some branding.

You will need Debian or Ubuntu with Android SDK and Android NDK installed:

http://developer.android.com/sdk/index.html

http://developer.android.com/tools/sdk/ndk/index.html

You will need to install some packages to your Debian/Ubuntu first:

```
sudo apt-get install bison libpixman-1-dev \
libxfont-dev libxkbfile-dev libpciaccess-dev \
xutils-dev xcb-proto python-xcbgen xsltproc \
x11proto-bigreqs-dev x11proto-composite-dev \
x11proto-core-dev x11proto-damage-dev \
x11proto-dmx-dev x11proto-dri2-dev x11proto-fixes-dev \
x11proto-fonts-dev x11proto-gl-dev \
x11proto-input-dev x11proto-kb-dev \
x11proto-print-dev x11proto-randr-dev \
x11proto-record-dev x11proto-render-dev \
x11proto-resource-dev x11proto-scrnsaver-dev \
x11proto-video-dev x11proto-xcmisc-dev \
x11proto-xext-dev x11proto-xf86bigfont-dev \
x11proto-xf86dga-dev x11proto-xf86dri-dev \
x11proto-xf86vidmode-dev x11proto-xinerama-dev \
libxmuu-dev libxt-dev libsm-dev libice-dev \
libxrender-dev libxrandr-dev curl autoconf automake libtool \
pkg-config libjpeg-dev libpng-dev
```

You will need both xcb-proto and python-xcbgen packages
to have version 1.10-1, you may download newer packages
from http://packages.ubuntu.com/ or https://www.debian.org/distrib/packages

Then run commands:

```
git clone --depth=1 https://github.com/pelya/commandergenius.git xsdl
patch -p1 -d xsdl < viewtouch.patch
cp -a override/. xsdl
cd xsdl
git submodule update --init --depth=1 project/jni/application/xserver/xserver
rm project/jni/application/src
ln -s xserver project/jni/application/src
./build.sh
```

The compiled app will be at `project/bin/MainActivity-debug.apk`.

To put the app to the Play Store, it has to be signed:

http://developer.android.com/tools/publishing/app-signing.html

You can sign the app using helper script sign.sh from directory xsdl.
