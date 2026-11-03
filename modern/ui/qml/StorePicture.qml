import QtQuick

// A store's picture on a guest's screen (a slide, the kiosk's welcome): an
// animated GIF or WebP plays (AnimatedImage, which reads it from its file),
// anything else is an Image. Less motion asked for: its first frame, still.
Item {
    id: pic
    property url source
    property int fillMode: Image.PreserveAspectFit
    property bool asynchronous: false
    readonly property bool animated: /\.(gif|webp)$/i.test(String(source))
    Image {
        anchors.fill: parent
        visible: !pic.animated
        source: pic.animated ? "" : pic.source
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
}
