import QtQuick
import QtQuick.Shapes

// The home screen's icons: lines drawn on a 24 × 24 grid, in one color.
Item {
    id: icon
    property string name
    property color color: "white"
    property real lineWidth: 1.8

    implicitWidth: 48
    implicitHeight: 48

    readonly property var paths: ({
        start: "M8 5 L19 12 L8 19 Z",
        power: "M12 3 L12 11 M6.3 6.6 A8 8 0 1 0 17.7 6.6",
        restart: "M20 12 A8 8 0 1 1 17.7 6.3 M20 3 L20 8 L15 8",
        wifi: "M2.5 9 A14 14 0 0 1 21.5 9 M5.5 12.5 A9.5 9.5 0 0 1 18.5 12.5 M8.7 16 A4.8 4.8 0 0 1 15.3 16 M12 19.5 L12 19.6",
        network: "M9 3 H15 V8 H9 Z M3 16 H9 V21 H3 Z M15 16 H21 V21 H15 Z M12 8 V12 M6 16 V12 H18 V16",
        clock: "M3 12 A9 9 0 1 0 21 12 A9 9 0 1 0 3 12 M12 7 V12 L15.5 14",
        update: "M12 3 V15 M7 10 L12 15 L17 10 M4 17 V21 H20 V17",
        usb: "M12 3 V17 M10 5 L12 3 L14 5 M12 13 L7 10 V7 M12 15 L17 12 V9 M5.5 6 H8.5 V8 H5.5 Z M17 7.5 A1.2 1.2 0 1 0 17 9.9 A1.2 1.2 0 1 0 17 7.5 M10 19 A2 2 0 1 0 14 19 A2 2 0 1 0 10 19",
        erase: "M4 6 H20 M9 6 V4 H15 V6 M6 6 L7 21 H17 L18 6 M10 10 V17 M14 10 V17",
        startover: "M4 12 A8 8 0 1 0 6.3 6.3 M4 3 V8 H9",
        help: "M4 14 V12 A8 8 0 0 1 20 12 V14 M4 14 H7 V19 H4 Z M20 14 H17 V19 H20 Z",
        about: "M3 12 A9 9 0 1 0 21 12 A9 9 0 1 0 3 12 M12 11 V17 M12 7.5 V7.6",
        setup: "M10 6 H20 M10 12 H20 M10 18 H20 M4 6 L5.5 7.5 L8 5 M4 12 L5.5 13.5 L8 11 M4 18 L5.5 19.5 L8 17",
        menu: "M7 3 V21 M4 3 V8 A3 3 0 0 0 10 8 V3 M17 3 C14 5 14 11 17 12 V21",
        staff: "M5 7 A4 4 0 1 0 13 7 A4 4 0 1 0 5 7 M2 21 C2 15 16 15 16 21 M15.5 3.3 A4 4 0 0 1 15.5 10.7 M18 14.5 C21 15.5 22 18 22 21",
        printer: "M7 8 V3 H17 V8 M5 17 H3 V9 H21 V17 H19 M7 14 H17 V21 H7 Z",
        addscreen: "M3 4 H21 V16 H3 Z M8 21 H16 M12 16 V21 M12 7 V13 M9 10 H15",
        reports: "M4 20 H20 M7 16 V11 M12 16 V6 M17 16 V9",
        backup: "M5 3 H16 L21 8 V21 H5 Z M8 3 V8 H15 V3 M8 21 V14 H16 V21",
        store: "M3 9 L5 4 H19 L21 9 Z M4 9 V20 H20 V9 M10 20 V14 H14 V20",
        register: "M4 10 H20 V21 H4 Z M7 10 V6 H17 V10 M8 14 H10 M14 14 H16 M8 17 H10 M14 17 H16 M9 3 H15 V6 H9 Z",
        kitchen: "M3 10 H21 M5 10 V18 A2 2 0 0 0 7 20 H17 A2 2 0 0 0 19 18 V10 M9 7 C9 5 11 5 11 3 M13 7 C13 5 15 5 15 3",
        kiosk: "M7 2 H17 V22 H7 Z M10 18 H14 M9.5 6 H14.5",
        timeclock: "M3 12 A9 9 0 1 0 21 12 A9 9 0 1 0 3 12 M12 7 V12 L15.5 14",
        lock: "M6 11 H18 V21 H6 Z M8 11 V7 A4 4 0 0 1 16 7 V11",
        back: "M15 5 L8 12 L15 19",
        check: "M5 12 L10 17 L19 7",
        close: "M6 6 L18 18 M18 6 L6 18",
        warning: "M12 3 L22 20 H2 Z M12 9 V14 M12 17 V17.1"
    })

    Shape {
        anchors.centerIn: parent
        width: 24
        height: 24
        scale: Math.min(icon.width, icon.height) / 24
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: icon.color
            strokeWidth: icon.lineWidth
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: icon.paths[icon.name] ?? "" }
        }
    }
}
