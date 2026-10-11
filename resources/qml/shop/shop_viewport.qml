import QtQuick
import QtQuick3D



// 同目录下的 Darkeye_shop.qml（无 qmldir）：使用限定目录导入。

import '.' as Shop



Rectangle {

    id: root
    property var coverTextureSources: []
    property vector3d camPos: Qt.vector3d(0, 0.865, 0.86)
    property real pitchDeg: 0
    property real yawDeg: 0
    signal coverClicked(int index)
    signal cameraChanged()
    onCamPosChanged: cameraChanged()
    onPitchDegChanged: cameraChanged()
    onYawDegChanged: cameraChanged()
    property int hoveredCoverIndex: -1
    property color outlineColor: "#00aaff"

    color: "#18181c"



    View3D {

        id: view3d

        anchors.fill: parent

        camera: cam



        environment: SceneEnvironment {

            clearColor: root.color

            backgroundMode: SceneEnvironment.SkyBox

            lightProbe: Texture {

                source: "qrc:/assets/hdr/"

                    + "lebombo_1k.hdr"

            }

            probeOrientation: Qt.vector3d(0, 155, 0)

            antialiasingMode: SceneEnvironment.MSAA

            antialiasingQuality: SceneEnvironment.High

            tonemapMode: SceneEnvironment.TonemapModeFilmic

        }



        PerspectiveCamera {
            id: cam
            // Center all three rows and fit their six columns in narrow windows.
            position: Qt.vector3d(root.camPos.x, root.camPos.y,
                Math.max(root.camPos.z, 0.52 / (Math.tan(25 * Math.PI / 180)
                    * Math.max(0.1, view3d.width / Math.max(1, view3d.height)))))
            eulerRotation.x: root.pitchDeg
            eulerRotation.y: root.yawDeg
            eulerRotation.z: 0
            fieldOfView: 50
            clipNear: 0.06
            clipFar: 400
        }



        DirectionalLight {

            eulerRotation.x: -40

            eulerRotation.y: -50

            color: Qt.rgba(1.0, 0.96, 0.92, 1.0)

            ambientColor: Qt.rgba(0.32, 0.33, 0.36, 1.0)

            brightness: 0.72

            castsShadow: false

        }



        DirectionalLight {

            eulerRotation.x: -28

            eulerRotation.y: 95

            color: Qt.rgba(0.62, 0.78, 1.0, 1.0)

            ambientColor: Qt.rgba(0.07, 0.09, 0.12, 1.0)

            brightness: 0.78

            castsShadow: false

        }



        DirectionalLight {

            eulerRotation.x: -6

            eulerRotation.y: 178

            color: Qt.rgba(1.0, 0.98, 0.94, 1.0)

            brightness: 0.5

            castsShadow: false

        }



        PointLight {

            position: Qt.vector3d(-0.35, 0.65, 0.55)

            brightness: 1.65

            color: "#eaf0ff"

            constantFade: 0.5

            linearFade: 7.5

            quadraticFade: 16

        }



        MouseArea {
            z: 1
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            property bool orbiting: false
            property real lastX: 0
            property real lastY: 0
            onPressed: function(mouse) {
                orbiting = mouse.button === Qt.RightButton
                lastX = mouse.x
                lastY = mouse.y
            }
            onReleased: orbiting = false
            onCanceled: orbiting = false
            hoverEnabled: true
            cursorShape: root.hoveredCoverIndex >= 0
                ? Qt.PointingHandCursor
                : Qt.ArrowCursor

            function coverIndexAt(x, y) {
                var result = view3d.pick(x, y)
                if (!result || !result.objectHit)
                    return -1
                var hit = result.objectHit
                return typeof hit.coverIndex !== "undefined" ? hit.coverIndex : -1
            }

            onPositionChanged: function(mouse) {
                if (orbiting) {
                    root.pitchDeg = Math.max(-89, Math.min(89,
                        root.pitchDeg - (mouse.y - lastY) * 0.12))
                    root.yawDeg -= (mouse.x - lastX) * 0.12
                    lastX = mouse.x
                    lastY = mouse.y
                    return
                }
                root.hoveredCoverIndex = coverIndexAt(mouse.x, mouse.y)
                hoverOutline.updateForIndex(root.hoveredCoverIndex)
            }

            onExited: {
                root.hoveredCoverIndex = -1
                hoverOutline.updateForIndex(-1)
            }

            onClicked: function(mouse) {
                if (mouse.button !== Qt.LeftButton) return
                var index = coverIndexAt(mouse.x, mouse.y)
                if (index >= 0)
                    root.coverClicked(index)
            }
        }

        Shop.Darkeye_shop {
            id: shopScene
            coverTextureSources: root.coverTextureSources
        }

        Canvas {
            id: hoverOutline
            objectName: "hoverOutline"
            z: 2
            anchors.fill: parent
            visible: false

            property var outlinePoints: []
            property color strokeColor: root.outlineColor

            onStrokeColorChanged: requestPaint()

            function cross(origin, a, b) {
                return (a.x - origin.x) * (b.y - origin.y)
                    - (a.y - origin.y) * (b.x - origin.x)
            }

            function convexHull(inputPoints) {
                if (inputPoints.length <= 1)
                    return inputPoints
                var points = inputPoints.slice(0)
                points.sort(function(a, b) {
                    return a.x === b.x ? a.y - b.y : a.x - b.x
                })

                var lower = []
                for (var i = 0; i < points.length; ++i) {
                    while (lower.length >= 2
                           && cross(lower[lower.length - 2],
                                    lower[lower.length - 1], points[i]) <= 0)
                        lower.pop()
                    lower.push(points[i])
                }

                var upper = []
                for (var j = points.length - 1; j >= 0; --j) {
                    while (upper.length >= 2
                           && cross(upper[upper.length - 2],
                                    upper[upper.length - 1], points[j]) <= 0)
                        upper.pop()
                    upper.push(points[j])
                }

                lower.pop()
                upper.pop()
                return lower.concat(upper)
            }

            function updateForIndex(index) {
                var cover = shopScene.coverObjectAt(index)
                if (!cover) {
                    outlinePoints = []
                    visible = false
                    return
                }

                var localCorners = [
                    Qt.vector3d(-0.007, 0.095, 0.0675),
                    Qt.vector3d(-0.007, 0.095, -0.0675),
                    Qt.vector3d(-0.007, -0.095, 0.0675),
                    Qt.vector3d(-0.007, -0.095, -0.0675),
                    Qt.vector3d(0.007, 0.095, 0.0675),
                    Qt.vector3d(0.007, 0.095, -0.0675),
                    Qt.vector3d(0.007, -0.095, 0.0675),
                    Qt.vector3d(0.007, -0.095, -0.0675)
                ]
                var projected = []
                for (var i = 0; i < localCorners.length; ++i) {
                    var point = view3d.mapFrom3DScene(
                        cover.mapPositionToScene(localCorners[i]))
                    if (point.z <= 0) {
                        outlinePoints = []
                        visible = false
                        return
                    }
                    projected.push(Qt.point(point.x, point.y))
                }

                outlinePoints = convexHull(projected)
                visible = true
                requestPaint()
            }

            onPaint: {
                var context = getContext("2d")
                context.clearRect(0, 0, width, height)
                if (outlinePoints.length < 3)
                    return
                context.beginPath()
                context.moveTo(outlinePoints[0].x, outlinePoints[0].y)
                for (var i = 1; i < outlinePoints.length; ++i)
                    context.lineTo(outlinePoints[i].x, outlinePoints[i].y)
                context.closePath()
                context.strokeStyle = strokeColor
                context.lineWidth = 2
                context.lineJoin = "round"
                context.stroke()
            }
        }

        Connections {
            target: root

            function onCameraChanged() {
                hoverOutline.updateForIndex(root.hoveredCoverIndex)
            }
        }

    }

}



