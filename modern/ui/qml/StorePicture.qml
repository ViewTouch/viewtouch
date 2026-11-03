import QtQuick
import Qt.labs.lottieqt

// A store's picture on a guest's screen (a slide, the kiosk's welcome): an
// animated GIF or WebP plays (AnimatedImage, which reads it from its file),
// a Lottie animation (.json) plays (LottieAnimation), anything else is an
// Image. Less motion asked for: the first frame, still.
Item {
    id: pic
    property url source
    property int fillMode: Image.PreserveAspectFit
    property bool asynchronous: false
    readonly property bool animated: /\.(gif|webp)$/i.test(String(source))
    readonly property bool lottie: /\.json$/i.test(String(source))
    Image {
        anchors.fill: parent
        visible: !pic.animated && !pic.lottie
        source: pic.animated || pic.lottie ? "" : pic.source
        fillMode: pic.fillMode
        asynchronous: pic.asynchronous
    }
    AnimatedImage {
        objectName: "animatedPicture"
        anchors.fill: parent
        visible: pic.animated
        source: pic.animated ? pic.source : ""
        fillMode: pic.fillMode
        playing: visible && !Ui.reduceMotion
    }
    // A Lottie animation draws at its own size: scaled to fit (or fill) here.
    Item {
        anchors.fill: parent
        clip: pic.fillMode === Image.PreserveAspectCrop
        visible: pic.lottie
        LottieAnimation {
            id: lottie
            objectName: "lottiePicture"
            anchors.centerIn: parent
            source: pic.lottie ? pic.source : ""
            loops: LottieAnimation.Infinite
            autoPlay: !Ui.reduceMotion
            readonly property real fit: width > 0 && height > 0
                ? (pic.fillMode === Image.PreserveAspectCrop ? Math.max(parent.width / width, parent.height / height)
                                                              : Math.min(parent.width / width, parent.height / height))
                : 1
            scale: fit
        }
    }
}
