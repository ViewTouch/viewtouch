import QtQuick

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
    // A Lottie animation (LottiePicture.qml: Qt's Lottie module is only
    // needed when one is shown).
    Loader {
        id: lottieLoader
        anchors.fill: parent
        active: pic.lottie
        visible: pic.lottie
        source: "LottiePicture.qml"
    }
    Binding { target: lottieLoader.item; property: "source"; value: pic.source; when: lottieLoader.item !== null }
    Binding { target: lottieLoader.item; property: "fillMode"; value: pic.fillMode; when: lottieLoader.item !== null }
}
