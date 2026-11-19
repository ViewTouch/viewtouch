import QtQuick
import Qt.labs.lottieqt

// A Lottie animation (StorePicture's), in a file of its own: loaded only for
// a Lottie picture, so an app without Qt's Lottie module (an Android build
// that lacked it) only misses that picture instead of failing to start.
// It draws at its own size: scaled to fit (or fill) here.
Item {
    id: holder
    property url source
    property int fillMode: Image.PreserveAspectFit
    clip: fillMode === Image.PreserveAspectCrop
    LottieAnimation {
        objectName: "lottiePicture"
        anchors.centerIn: parent
        source: holder.source
        loops: LottieAnimation.Infinite
        autoPlay: !Ui.reduceMotion
        readonly property real fit: width > 0 && height > 0
            ? (holder.fillMode === Image.PreserveAspectCrop ? Math.max(holder.width / width, holder.height / height)
                                                            : Math.min(holder.width / width, holder.height / height))
            : 1
        scale: fit
    }
}
