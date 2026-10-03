import QtQuick
import QtQuick3D
Node {
    id: rOOT
    /** 贴图路径，可动态更换；支持相对路径（相对 Dvd.qml 所在目录）或 file:// 绝对路径 */
    property string textureSource: (typeof mapsPath !== "undefined" ? mapsPath : "maps/") + "0.png"
    /** 由 dvd_scene 注入，用于 hover 判断 */
    property int delegateIndex: -1
    /** 展开状态：横着时再次点击触发展开，back 不动，spine 沿 back 轴转 -90°，front 沿 spine 转后再沿自身轴转 -90° */
    property bool expanded: false
    /** 展开且无本地视频时，盘面绕主轴（X）在盘面平面内顺时偏转（由 dvd_scene 注入） */
    property bool discNoVideoTilt: false
    // Expose the spine center anchor for scene-level action overlay projection.
    property var actionAnchorNode: spineActionAnchor
    // Expose the front center anchor for scene-level title/story overlay projection.
    property var frontInfoAnchorNode: frontInfoAnchor
    // 盘面几何中心（挂在 cdRollNode、不随 cdSpinNode 自转，供剧照条 2D 定位）
    property var discCenterAnchorNode: discCenterFanartAnchor
    /** 点击 CD 盘面时发射 */
    signal cdClicked()
    signal closeAnimationFinished()

    property bool discSpinRunning: true
    property int discSpinDuration: 3000
    property real discSpinAngle: 0
    property bool _closeAnimationPending: false
    property real closeAnimationSpeedMultiplier: 1.0
    // The original imported Timeline created two groups and 24 Keyframe
    // objects for every shelf delegate.  This is the same smoothstep curve in
    // closed form, so a collapsed shelf only needs the two animations below.
    property real _openFrame: 0
    readonly property real _openCurvePosition: Math.max(0, Math.min(1,
        (_openFrame - 41.6667) / 458.3333))
    readonly property real _openCurve: _openCurvePosition * _openCurvePosition
        * (3 - 2 * _openCurvePosition)
    readonly property real _openAngle: -90 * _openCurve

    function playOpenAnimation() {
        expanded = true
    }

    function playCloseAnimation() {
        expanded = false
    }

    function toggleClickAnimation() {
        if (expanded) {
            playCloseAnimation()
        } else {
            playOpenAnimation()
        }
    }

    // 当 expanded 被 dvd_scene 的 Binding 或 playOpen/Close 设置时，驱动开合动画
    onExpandedChanged: {
        if (expanded) {
            _closeAnimationPending = false
            back.playOpenAnimation()
        } else {
            _closeAnimationPending = true
            back.playCloseAnimation()
        }
    }

    NumberAnimation {
        id: openAnim
        target: rOOT
        property: "_openFrame"
        easing.type: Easing.Linear
    }

    NumberAnimation {
        id: closeAnim
        target: rOOT
        property: "_openFrame"
        easing.type: Easing.Linear
        onRunningChanged: {
            if (!running && rOOT._closeAnimationPending && rOOT._openFrame <= 0.001) {
                rOOT._closeAnimationPending = false
                rOOT.closeAnimationFinished()
            }
        }
    }

    /*
    NumberAnimation on discSpinAngle {
        from: 0
        to: 360
        duration: rOOT.discSpinDuration
        loops: Animation.Infinite
        running: rOOT.discSpinRunning
    }
    */

    // 伪 Toon 风格：降低金属度、提高粗糙度，让高光更“块状”、整体更偏 2D 纸质感
    PrincipledMaterial {
        id: pic_material
        baseColorMap: Texture {
            source: rOOT.textureSource
            tilingModeHorizontal: Texture.Repeat
            tilingModeVertical: Texture.Repeat
        }
        opacityChannel: Material.A
        metalness: 0
        // 原来 roughness≈0.08 偏写实高光，这里拉高到 0.9 做成近乎哑光
        roughness: 0.9
        cullMode: Material.NoCulling
    }

    Node {
        id: cdRollNode
        x: -0.0048184
        y: 0.095
        z: -0.0667828

        // 自转与「无视频」提示都在主轴 X 上：平面内偏转，不用 Y/Z 翘盘面。
        Node {
            id: cdSpinNode
            property real cdNoVideoPlaneTwistDeg: discNoVideoTilt ? 30 : 0
            eulerRotation: Qt.vector3d(discSpinAngle + cdNoVideoPlaneTwistDeg, 0, 0)
            Behavior on cdNoVideoPlaneTwistDeg {
                NumberAnimation { duration: 380; easing.type: Easing.OutCubic }
            }

            // Closed shelf cases do not expose the disc. Avoid constructing a
            // fourth model and two extra materials for every visible DVD; keep
            // it alive through the close animation once it has been requested.
            Loader3D {
                active: rOOT.expanded || rOOT._closeAnimationPending || rOOT._openFrame > 0.001
                sourceComponent: Component {
                    Model {
                        objectName: "cD"
                        pickable: true
                        scale.x: 1
                        scale.y: 1
                        scale.z: 1
                        source: (typeof meshesPath !== "undefined" ? meshesPath : "meshes/") + "cD.mesh"

                        // CD 盘面也调成更哑光、少金属的 toon 风格
                        PrincipledMaterial {
                            id: transparent_material
                            baseColor: "#ffffffcc"
                            metalness: 0
                            roughness: 0.9
                            cullMode: Material.NoCulling
                            alphaMode: PrincipledMaterial.Blend
                        }

                        PrincipledMaterial {
                            id: rainbow_material
                            baseColor: "#ffcccccc"
                            metalness: 0.15
                            roughness: 0.85
                            cullMode: Material.NoCulling
                        }
                        materials: [
                            pic_material,
                            transparent_material,
                            rainbow_material
                        ]
                    }
                }
            }
        }

        Node {
            id: discCenterFanartAnchor
            x: 0
            y: 0
            z: 0
        }
    }

    Model {
        id: back
        objectName: "dvdRoot"
        pickable: true

        function playOpenAnimation() {
            closeAnim.running = false
            var cf = rOOT._openFrame
            openAnim.from = cf
            openAnim.to = 500
            openAnim.duration = Math.max(100, 500 * (500 - cf) / 500)
            openAnim.running = true
        }
        function playCloseAnimation() {
            openAnim.running = false
            var cf = rOOT._openFrame
            closeAnim.from = cf            
            closeAnim.to = 0            
            var speed = Math.max(0.1, rOOT.closeAnimationSpeedMultiplier)
            closeAnim.duration = Math.max(100 / speed, (500 * cf / 500) / speed)
            closeAnim.running = true
        }

        x: -0.0067071
        y: 0.095
        z: -0.000478134
        source: (typeof meshesPath !== "undefined" ? meshesPath : "meshes/") + "back.mesh"

        // 盒体外壳同样走哑光 toon 质感
        PrincipledMaterial {
            id: trans_material
            baseColor: "#ffffffff"
            metalness: 0
            roughness: 0.9
            cullMode: Material.NoCulling
        }

        materials: [
            pic_material,
            trans_material
        ]
        Node{
            Node {
            id: spineActionAnchor
            // Use the spine mesh center as action buttons anchor.
            x: spine.x
            y: 0
            z: spine.z+0.007
        }

        Model {
            id: spine
            pickable: true
            eulerRotation: Qt.vector3d(0, rOOT._openAngle, 0)
            source: (typeof meshesPath !== "undefined" ? meshesPath : "meshes/") + "spine.mesh"
            materials: [
                pic_material,
                trans_material
            ]
            Node{
                id:frontnode
                Model {
                    id: front
                    pickable: true
                    x: 0.0134142
                    eulerRotation: Qt.vector3d(0, rOOT._openAngle, 0)
                    source: (typeof meshesPath !== "undefined" ? meshesPath : "meshes/") + "front.mesh"
                    materials: [
                        pic_material,
                        trans_material
                    ]
                }
                Node {
                    id: frontInfoAnchor
                    // Use the front mesh center as title/story anchor.
                    x: front.x+0.0675
                    y: 0 
                    z: front.z
                }
            }
        }
        }

    }
}
