import QtQuick
import QtCore
import QtQuick3D

View3D {
    id: view3d
    anchors.fill: parent
    camera: orbitCamera
    // 相机在书架上的横向位置（范围由外部约束到 [0, dvdShelfLength]）。
    property real cameraX: 0
    // 右键拖拽控制的俯仰与偏航角。
    property real orbitRotationX: 0
    property real orbitRotationY: 0
    // 可见窗口内的悬停/选中/展开/按下索引。
    property int hoveredDelegateIndex: -1
    property int selectedDelegateIndex: -1
    property int expandedDelegateIndex: -1
    property int pressedDelegateIndex: -1
    // 左键按下时命中的 3D 对象，用于区分 CD 与盒体点击。
    property var pressedObjectHit: null
    // 展开态操作按钮（爱心/编辑/删除）的 3D 锚点映射，key=delegate index。
    property var actionAnchorByIndex: ({})
    // 展开态 title/story 的 front 面锚点映射，key=delegate index。
    property var frontInfoAnchorByIndex: ({})
    // 展开态光盘中心锚点（不随盘面自转），key=delegate index。
    property var discCenterAnchorByIndex: ({})
    property int _pendingCollapseSelectedIndex: -1
    property real _pendingCollapseCloseSpeedMultiplier: 1.0
    property real wheelCloseAnimationSpeedMultiplier: 3.0
    property int _frozenSelectedDelegateIndex: -1
    property int _frozenSelectedVirtualIndex: -1

    Timer {
        id: wheelFreezeHoldTimer
        interval: 140
        repeat: false
    }

    function nlerpQuaternion(from, to, factor) {
        var t = Math.max(0, Math.min(1, factor))
        var end = to
        if (from.dotProduct(end) < 0)
            end = end.times(-1)
        return from.times(1 - t).plus(end.times(t)).normalized()
    }

    // 完全展开后的 delegate 索引（开盒动画完成后才 >= 0；折叠时立即 -1）。Dvd.qml 当前动画时长为 500ms。
    property int fullyExpandedDelegateIndex: -1
    readonly property int _expandAnimMs: 500
    Timer {
        id: fullyExpandedTimer
        interval: view3d._expandAnimMs
        onTriggered: view3d.fullyExpandedDelegateIndex = view3d.expandedDelegateIndex
    }
    onExpandedDelegateIndexChanged: {
        if (view3d.expandedDelegateIndex >= 0)
            fullyExpandedTimer.start()
        else {
            fullyExpandedTimer.stop()
            view3d.fullyExpandedDelegateIndex = -1
        }
    }

    // 选中/展开变化时通知 Bridge，用于在左侧显示或隐藏力导向图。
    Connections {
        target: view3d
        function onSelectedDelegateIndexChanged() {
            if (typeof dvdBridge !== "undefined" && dvdBridge && typeof dvdBridge.selection_changed === "function")
                dvdBridge.selection_changed(view3d.selectedDelegateIndex, view3d.expandedDelegateIndex)
        }
        function onExpandedDelegateIndexChanged() {
            if (typeof dvdBridge !== "undefined" && dvdBridge && typeof dvdBridge.selection_changed === "function")
                dvdBridge.selection_changed(view3d.selectedDelegateIndex, view3d.expandedDelegateIndex)
        }
    }

    // 将可见窗口内 delegate 索引映射回全量 work 索引（与 Repeater3D delegate 的 targetVirtualIndex 一致）。
    function expandedVirtualIndexFor(delegateIndex) {
        if (delegateIndex < 0)
            return -1
        var vStart = (typeof dvdVisibleStart !== "undefined" ? dvdVisibleStart : 0)
        var dCount = (typeof dvdCount !== "undefined" ? dvdCount : 0)
        var frozenD = _frozenSelectedDelegateIndex
        var frozenV = _frozenSelectedVirtualIndex
        if (frozenD >= 0 && delegateIndex === frozenD && frozenV >= 0)
            return frozenV
        var visibleEnd = vStart + Math.max(0, dCount - 1)
        var shouldReserve = frozenD >= 0
            && delegateIndex !== frozenD
            && frozenV >= vStart
            && frozenV <= visibleEnd
        if (!shouldReserve)
            return vStart + delegateIndex
        var reservedLocalOffset = frozenV - vStart
        var nonSelectedRank = delegateIndex < frozenD ? delegateIndex : delegateIndex - 1
        var targetSourceIndex = nonSelectedRank + (nonSelectedRank >= reservedLocalOffset ? 1 : 0)
        return vStart + targetSourceIndex
    }

    readonly property int selectedWorkVirtualIndex: selectedDelegateIndex < 0
        ? -1
        : expandedVirtualIndexFor(selectedDelegateIndex)

    function cancelPendingCollapseAfterClose() {
        _pendingCollapseSelectedIndex = -1
        _pendingCollapseCloseSpeedMultiplier = 1.0
    }

    function requestCollapseAfterClose(closeSpeedMultiplier) {
        if (_pendingCollapseSelectedIndex >= 0)
            return
        var speedMultiplier = (typeof closeSpeedMultiplier === "number" && isFinite(closeSpeedMultiplier))
            ? Math.max(0.1, closeSpeedMultiplier)
            : 1.0
        if (expandedDelegateIndex >= 0 && selectedDelegateIndex === expandedDelegateIndex) {
            _pendingCollapseSelectedIndex = selectedDelegateIndex
            _pendingCollapseCloseSpeedMultiplier = speedMultiplier
            expandedDelegateIndex = -1
            return
        }
        _pendingCollapseSelectedIndex = -1
        _pendingCollapseCloseSpeedMultiplier = 1.0
        selectedDelegateIndex = -1
    }

    function finishPendingCollapseAfterClose(delegateIndex) {
        if (_pendingCollapseSelectedIndex !== delegateIndex)
            return
        if (expandedDelegateIndex >= 0 || selectedDelegateIndex !== delegateIndex)
            return
        _pendingCollapseSelectedIndex = -1
        _pendingCollapseCloseSpeedMultiplier = 1.0
        selectedDelegateIndex = -1
    }

    // 场景环境：天空盒、探针、抗锯齿与 AO。
    environment: SceneEnvironment {
        clearColor: "#1a1a2e"
        backgroundMode: SceneEnvironment.SkyBox
        lightProbe: Texture {
            source: (typeof hdrPath !== "undefined" ? hdrPath : "/") + "lebombo_1k.hdr"//fireplace_2k,lebombo_2k
        }
        probeOrientation: Qt.vector3d(0, 155, 0)
        antialiasingMode: SceneEnvironment.MSAA
        antialiasingQuality: SceneEnvironment.High
        tonemapMode: SceneEnvironment.TonemapModeFilmic
        aoEnabled: true
        aoStrength: 0.18
        aoDistance: 1.5
        aoSoftness: 15
        aoSampleRate: 2
        debugSettings: DebugSettings {
            wireframeEnabled: showWireframe
        }
    }


    // 轨道相机节点：位置跟随 cameraX，朝向由 orbitRotationX/Y 控制。
    Node {
        id: orbitOrigin
        position: Qt.vector3d(0, 0, 0)

        PerspectiveCamera {
            id: orbitCamera
            position: Qt.vector3d((typeof dvdBridge !== "undefined" && dvdBridge) ? dvdBridge.cameraX : view3d.cameraX, 0.1, cameraDistance)
            eulerRotation.x: view3d.orbitRotationX
            eulerRotation.y: view3d.orbitRotationY
            clipNear: 0.001
            clipFar: 100000
            fieldOfView: 60


            // 相机前方固定点：选中时用于把 DVD 拉到镜头前。
            Node {
                id: cameraFront
                //现在这个0.0075现在还是有点问题，不过先这样吧
                position: Qt.vector3d(0.0075, -0.095, -(typeof selectedDvdDistance !== "undefined" ? selectedDvdDistance : 1.5))

                Node {
                    id: phase2TargetAnchor
                    rotation: Quaternion.fromEulerAngles(0, -90, 0)
                }
            }
        }
    }

    // 主光。
    DirectionalLight {
        id: keyLight
        eulerRotation.x: -40
        eulerRotation.y: -50
        color: Qt.rgba(1.0, 0.96, 0.92, 1.0)
        ambientColor: Qt.rgba(0.2, 0.2, 0.22, 1.0)
        brightness: 4
        castsShadow: true
        shadowMapQuality: Light.ShadowMapQualityHigh
        shadowMapFar: 50
    }

    // 补光。
    DirectionalLight {
        id: fillLight
        eulerRotation.x: -25
        eulerRotation.y: 100
        color: Qt.rgba(0.6, 0.75, 1.0, 1.0)
        ambientColor: Qt.rgba(0.05, 0.06, 0.1, 1.0)
        brightness: 0.8
    }

    // 轮廓光。
    DirectionalLight {
        id: rimLight
        eulerRotation.x: -5
        eulerRotation.y: 180
        color: Qt.rgba(1.0, 0.98, 0.95, 1.0)
        brightness: 0.6
    }

    // 3D 场景根节点。
    Node {
        id: sceneRoot
        // DVD 虚拟化可见窗口：只渲染 dvdCount 个可见项。
        Repeater3D {
            id: dvdRepeater
            model: dvdCount
            delegate: Node {
                id: delegateRoot
                readonly property int _visibleStart: (typeof dvdVisibleStart !== "undefined" ? dvdVisibleStart : 0)
                readonly property int _visibleEnd: _visibleStart + Math.max(0, dvdCount - 1)
                readonly property int _reservedLocalOffset: view3d._frozenSelectedVirtualIndex - _visibleStart
                readonly property int _nonSelectedRank: index < view3d._frozenSelectedDelegateIndex ? index : index - 1
                readonly property bool _shouldReserveFrozenSlot: view3d._frozenSelectedDelegateIndex >= 0
                    && index !== view3d._frozenSelectedDelegateIndex
                    && view3d._frozenSelectedVirtualIndex >= _visibleStart
                    && view3d._frozenSelectedVirtualIndex <= _visibleEnd
                readonly property int _targetSourceIndex: _shouldReserveFrozenSlot
                    ? _nonSelectedRank + (_nonSelectedRank >= _reservedLocalOffset ? 1 : 0)
                    : index
                // targetVirtualIndex 是当前可见窗口映射出的全量 work 列表索引；index 是可见窗口索引。
                property int targetVirtualIndex: _visibleStart + _targetSourceIndex
                // 每个 DVD 的目标封面纹理；缺失时回退默认贴图。
                property string targetTex: (dvdTextureSources
                    && _targetSourceIndex >= 0
                    && _targetSourceIndex < dvdTextureSources.length)
                    ? dvdTextureSources[_targetSourceIndex]
                    : ((typeof mapsPath !== "undefined" ? mapsPath : "maps/") + "0.png")
                property bool selected: view3d.selectedDelegateIndex === index
                // Selected item ignores hover to avoid z-jitter during animation.
                property bool hovered: !selected && view3d.hoveredDelegateIndex === index
                property real selectionProgress: selected ? 1 : 0
                Behavior on selectionProgress { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
                property bool _contentFrozen: false
                property bool _freezeCaptured: false
                property bool _freezeReleasePending: false
                property int _frozenVirtualIndex: targetVirtualIndex
                property string _frozenTex: targetTex
                // 新一次选中时立即冻结当前 target 并推到 Loader：避免 signal 顺序下
                // onSelectionProgressChanged 先于 onSelectedChanged、且 _freezeCaptured 仍为
                // true 时跳过 _frozenTex 更新（首次跳转最容易撞上，第二次状态已稳）。
                onSelectedChanged: {
                    if (selected) {
                        /*
                         * 不在这里改 view3d._frozenSelectedDelegateIndex：全局冻结槽位
                         * 仍由 selectionProgress>0 时设置，避免拉片动画开头几帧打乱邻格贴图映射。
                         */
                        _freezeReleasePending = false
                        freezeReleaseTimer.stop()
                        _frozenVirtualIndex = targetVirtualIndex
                        _frozenTex = targetTex
                        _freezeCaptured = true
                        _contentFrozen = true
                        if (dvdLoader.item && typeof dvdLoader.item.textureSource !== "undefined")
                            dvdLoader.item.textureSource = _frozenTex
                    }
                }
                // Freeze content identity while selected/closing/returning so scroll doesn't swap the art mid-animation.
                property int virtualIndex: _contentFrozen ? _frozenVirtualIndex : targetVirtualIndex
                property string tex: _contentFrozen ? _frozenTex : targetTex

                readonly property real _shelfX: virtualIndex * dvdSpacing
                readonly property real _shelfZ: hovered ? 0.03 : 0
                readonly property real _pullOutZ: (typeof dvdPullOutDistance !== "undefined" ? dvdPullOutDistance : 0.14)
                readonly property real _phase1Factor: Math.min(1, selectionProgress * 2)
                readonly property real _phase2Factor: Math.max(0, selectionProgress * 2 - 1)
                readonly property vector3d _phase2TargetPos: sceneRoot.mapPositionFromScene(phase2TargetAnchor.scenePosition)
                readonly property quaternion _phase2TargetRotation: sceneRoot.sceneRotation.inverted().times(phase2TargetAnchor.sceneRotation).normalized()
                // 0：封面与镜头平面平行；可改为小角度（如 3°）保留轻微立体感。
                readonly property real _tiltRad: 0
                readonly property quaternion _tiltQuat: Qt.quaternion(Math.cos(_tiltRad/2), 0,0, Math.sin(_tiltRad/2))
                
                Timer {
                    id: freezeReleaseTimer
                    interval: 16
                    repeat: false
                    onTriggered: {
                        if (selectionProgress > 0.001) {
                            _freezeReleasePending = false
                            return
                        }
                        if (wheelFreezeHoldTimer.running) {
                            restart()
                            return
                        }
                        if (view3d._frozenSelectedDelegateIndex === index) {
                            view3d._frozenSelectedDelegateIndex = -1
                            view3d._frozenSelectedVirtualIndex = -1
                        }
                        _freezeReleasePending = false
                        _freezeCaptured = false
                        _contentFrozen = false
                        _frozenVirtualIndex = targetVirtualIndex
                        _frozenTex = targetTex
                    }
                }

                onSelectionProgressChanged: {
                    if (selectionProgress > 0.001) {
                        if (!_freezeCaptured) {
                            _frozenVirtualIndex = targetVirtualIndex
                            _frozenTex = targetTex
                            _freezeCaptured = true
                        }
                        _contentFrozen = true
                        _freezeReleasePending = false
                        freezeReleaseTimer.stop()
                        view3d._frozenSelectedDelegateIndex = index
                        view3d._frozenSelectedVirtualIndex = _frozenVirtualIndex
                    } else {
                        // 选中刚开始时 progress 可能仍为 0，但 _contentFrozen 已被置为 true：
                        // 不能在未选中前启动解冻定时器，否则会误清本次选中的冻结贴图。
                        if (!selected && _contentFrozen && !_freezeReleasePending) {
                            _freezeReleasePending = true
                            freezeReleaseTimer.restart()
                        } else if (!_contentFrozen) {
                            _freezeCaptured = false
                            _frozenVirtualIndex = targetVirtualIndex
                            _frozenTex = targetTex
                        }
                    }
                }

                // Phase-1: z only pull-out. Phase-2: move to camera front.
                x: _shelfX + (_phase2TargetPos.x - _shelfX) * _phase2Factor
                y: _phase2TargetPos.y * _phase2Factor
                z: _shelfZ + _pullOutZ * _phase1Factor
                    + (_phase2TargetPos.z - _shelfZ - _pullOutZ) * _phase2Factor

                // 旋转中心，避免旋转时出现“飘移”。
                pivot: Qt.vector3d(0, 0, 0)

                // 选中/悬停位移动画。
                Behavior on z { enabled: selectionProgress <= 0.001; NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

                // 二阶段直接插值到镜头前目标坐标系，保证 front 面与镜头平面一致。
                Node {
                    // Rotate around spine axis.
                    pivot: Qt.vector3d(-0.006707, 0, -0.000293)
                    rotation: selectionProgress > 0
                        ? view3d.nlerpQuaternion(Qt.quaternion(1, 0, 0, 0), _phase2TargetRotation.times(_tiltQuat), _phase2Factor)
                        : Qt.quaternion(1, 0, 0, 0)
                    Loader3D {
                        id: dvdLoader
                        source: dvdQmlUrl
                        scale: Qt.vector3d(modelScale, modelScale, modelScale)
                        onStatusChanged: {
                            if (status === Loader3D.Error) console.warn("Dvd.qml load error")
                        }
                        // item 变化时同步贴图、索引、锚点与 CD 点击回调。
                        onItemChanged: {
                            view3d.actionAnchorByIndex[index] = null
                            view3d.frontInfoAnchorByIndex[index] = null
                            view3d.discCenterAnchorByIndex[index] = null
                            if (item) {
                                if (typeof item.textureSource !== "undefined") item.textureSource = tex
                                if (typeof item.delegateIndex !== "undefined") item.delegateIndex = index
                                if (typeof item.actionAnchorNode !== "undefined")
                                    view3d.actionAnchorByIndex[index] = item.actionAnchorNode
                                if (typeof item.frontInfoAnchorNode !== "undefined")
                                    view3d.frontInfoAnchorByIndex[index] = item.frontInfoAnchorNode
                                if (typeof item.discCenterAnchorNode !== "undefined")
                                    view3d.discCenterAnchorByIndex[index] = item.discCenterAnchorNode
                                if (typeof item.cdClicked !== "undefined") {
                                    item.cdClicked.connect(function() {
                                        var vIdx = virtualIndex
                                        if (typeof dvdBridge !== "undefined" && dvdBridge)
                                            dvdBridge.on_cd_clicked(vIdx)
                                    })
                                }
                                if (typeof item.closeAnimationFinished !== "undefined") {
                                    item.closeAnimationFinished.connect(function() {
                                        view3d.finishPendingCollapseAfterClose(index)
                                    })
                                }
                            }
                        }
                        Binding {
                            target: dvdLoader.item
                            property: "closeAnimationSpeedMultiplier"
                            value: view3d._pendingCollapseSelectedIndex === index
                                ? view3d._pendingCollapseCloseSpeedMultiplier
                                : 1.0
                            when: dvdLoader.item && typeof dvdLoader.item.closeAnimationSpeedMultiplier !== "undefined"
                        }
                        Binding {
                            target: dvdLoader.item
                            property: "expanded"
                            value: view3d.expandedDelegateIndex === index
                            // 将展开态同步给 Dvd.qml（开盒动画）。
                            when: dvdLoader.item && typeof dvdLoader.item.expanded !== "undefined"
                        }
                        Binding {
                            target: dvdLoader.item
                            property: "discNoVideoTilt"
                            value: view3d.expandedDelegateIndex === index
                                && typeof dvdBridge !== "undefined" && dvdBridge
                                && !dvdBridge.expandedWorkHasLocalVideo
                            when: dvdLoader.item && typeof dvdLoader.item.discNoVideoTilt !== "undefined"
                        }
                    }
                }
                // 可见窗口复用时，贴图变化需同步到现有 item。
                onTexChanged: {
                    if (dvdLoader.item && typeof dvdLoader.item.textureSource !== "undefined")
                        dvdLoader.item.textureSource = tex
                }
                onTargetTexChanged: {
                    if (_contentFrozen && _frozenVirtualIndex === targetVirtualIndex) {
                        _frozenTex = targetTex
                        if (dvdLoader.item && typeof dvdLoader.item.textureSource !== "undefined")
                            dvdLoader.item.textureSource = _frozenTex
                    }
                }
                // 销毁时清理锚点映射，避免索引复用时残留引用。
                Component.onDestruction: {
                    if (view3d.actionAnchorByIndex[index])
                        view3d.actionAnchorByIndex[index] = null
                    if (view3d.frontInfoAnchorByIndex[index])
                        view3d.frontInfoAnchorByIndex[index] = null
                    if (view3d.discCenterAnchorByIndex[index])
                        view3d.discCenterAnchorByIndex[index] = null
                }
            }
        }

    }



    // 从拾取命中的对象向上回溯到所属 delegate，返回其 index。
    function findDelegateIndex(obj) {
        var n = obj
        while (n) {
            if (typeof n.delegateIndex !== "undefined" && n.delegateIndex >= 0)
                return n.delegateIndex
            n = n.parent
        }
        return -1
    }

    function findAncestorWithCdClicked(obj) {
        var n = obj
        while (n) {
            if (typeof n.cdClicked !== "undefined")
                return n
            n = n.parent
        }
        return null
    }

    // 统一鼠标交互层：滚轮平移书架，右键拖拽旋转，左键点选/展开/DVD-CD 点击。
    MouseArea {
        z: 1
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        propagateComposedEvents: true
        property real _lastMouseX: 0
        property real _lastMouseY: 0
        property bool _rightDragging: false
        property bool _hoverPickCd: false
        cursorShape: _rightDragging ? Qt.ArrowCursor
            : (_hoverPickCd ? Qt.PointingHandCursor : Qt.ArrowCursor)
        // 右键旋转灵敏度。
        property real _rotSensitivity: 0.15



        // 滚轮：沿书架 X 方向移动相机，并收起选中/展开态。
        onWheel: function(wheel) {
            wheelFreezeHoldTimer.restart()
            view3d.requestCollapseAfterClose(view3d.wheelCloseAnimationSpeedMultiplier)
            var shelfLen = (typeof dvdShelfLength !== "undefined" && dvdShelfLength > 0) ? dvdShelfLength : 1.5
            var step = (typeof dvdSpacing !== "undefined" ? dvdSpacing : 0.0145) * 3
            var scrollUnits = 0
            if (wheel.pixelDelta.y !== 0)
                scrollUnits = wheel.pixelDelta.y / 40
            else if (wheel.angleDelta.y !== 0)
                scrollUnits = wheel.angleDelta.y / 120
            var delta = -scrollUnits * step
            if (typeof dvdBridge !== "undefined" && dvdBridge) {
                if (typeof dvdBridge.scroll_camera_by !== "undefined")
                    dvdBridge.scroll_camera_by(delta, shelfLen)
                else {
                    var baseCameraX = (typeof dvdBridge.cameraTargetX !== "undefined")
                        ? dvdBridge.cameraTargetX
                        : dvdBridge.cameraX
                    var newVal = Math.max(0, Math.min(shelfLen, baseCameraX + delta))
                    dvdBridge.set_camera_x(newVal)
                }
            } else {
                view3d.cameraX = Math.max(0, Math.min(shelfLen, view3d.cameraX + delta))
            }
            wheel.accepted = true
        }
        // 鼠标移动：右键拖拽时旋转相机，否则更新悬停项。
        onPositionChanged: function(mouse) {
            if (_rightDragging) {
                var dx = mouse.x - _lastMouseX
                var dy = mouse.y - _lastMouseY
                _lastMouseX = mouse.x
                _lastMouseY = mouse.y
                view3d.orbitRotationY = Math.max(-15, Math.min(15, view3d.orbitRotationY - dx * _rotSensitivity))
                view3d.orbitRotationX = Math.max(-10, Math.min(10, view3d.orbitRotationX - dy * _rotSensitivity))
                mouse.accepted = true
                return
            }
            var result = view3d.pick(mouse.x, mouse.y)
            if (result && result.objectHit) {
                var hoveredIdx = view3d.findDelegateIndex(result.objectHit)
                view3d.hoveredDelegateIndex = (hoveredIdx === view3d.selectedDelegateIndex) ? -1 : hoveredIdx
                _hoverPickCd = result.objectHit.objectName === "cD"
            } else {
                view3d.hoveredDelegateIndex = -1
                _hoverPickCd = false
            }
            mouse.accepted = false
        }
        // 按下：记录拖拽起点与按下命中的对象。
        onPressed: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                _rightDragging = true
                _lastMouseX = mouse.x
                _lastMouseY = mouse.y
                mouse.accepted = true
                return
            }
            if (mouse.button !== Qt.LeftButton) {
                mouse.accepted = false
                return
            }
            var result = view3d.pick(mouse.x, mouse.y)
            if (result && result.objectHit) {
                view3d.pressedDelegateIndex = view3d.findDelegateIndex(result.objectHit)
                view3d.pressedObjectHit = result.objectHit
            } else {
                view3d.pressedDelegateIndex = -1
                view3d.pressedObjectHit = null
            }
            mouse.accepted = true
        }
        // 释放：处理 CD 点击、选中切换与展开切换。
        onReleased: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                _rightDragging = false
                mouse.accepted = true
                return
            }
            if (mouse.button !== Qt.LeftButton) {
                mouse.accepted = false
                return
            }
            if (view3d.pressedDelegateIndex >= 0) {
                view3d.cancelPendingCollapseAfterClose()
                var idx = view3d.pressedDelegateIndex
                var hitCd = view3d.pressedObjectHit && view3d.pressedObjectHit.objectName === "cD"
                if (hitCd) {
                    var root = view3d.findAncestorWithCdClicked(view3d.pressedObjectHit)
                    if (root && typeof root.cdClicked !== "undefined")
                        root.cdClicked()
                } else {
                    if (view3d.selectedDelegateIndex === idx) {

                        view3d.expandedDelegateIndex = (view3d.expandedDelegateIndex === idx) ? -1 : idx
                    } else {
                        view3d.selectedDelegateIndex = idx
                        view3d.expandedDelegateIndex = -1
                    }
                }
            } else {
                view3d.requestCollapseAfterClose(1.0)
            }
            view3d.pressedDelegateIndex = -1
            view3d.pressedObjectHit = null
            mouse.accepted = true
        }
        onExited: {
            view3d.hoveredDelegateIndex = -1
            _rightDragging = false
            _hoverPickCd = false
        }
    }

    // 选中未展开：与 workInfoOverlay 相同，用封面 frontInfo 锚点 2D 投影更新力导图位置（宽高由 Python 按视图高度比例）。
    Timer {
        id: forceViewOverlayTimer
        interval: 16
        repeat: true
        running: view3d.selectedDelegateIndex >= 0 && view3d.expandedDelegateIndex < 0
        onTriggered: view3d.syncForceViewOverlayLayout()
    }

    function syncForceViewOverlayLayout() {
        if (!(selectedDelegateIndex >= 0 && expandedDelegateIndex < 0))
            return
        var anchor = frontInfoAnchorByIndex[selectedDelegateIndex]
        if (!anchor)
            return
        var _cameraTick = orbitCamera.position.x + orbitCamera.position.y + orbitCamera.position.z
            + orbitCamera.eulerRotation.x + orbitCamera.eulerRotation.y + orbitCamera.eulerRotation.z
            + view3d.width + view3d.height
        void _cameraTick
        var p = mapFrom3DScene(anchor.scenePosition)
        if (!isFinite(p.x) || !isFinite(p.y))
            return
        if (typeof dvdBridge === "undefined" || !dvdBridge
            || typeof dvdBridge.set_force_view_overlay !== "function")
            return
        dvdBridge.set_force_view_overlay(p.x, p.y)
    }

    // front 面信息层：title/story 的 2D 投影，仅完全展开后显示。
    Item {
        id: workInfoOverlay
        z: 2
        readonly property real _overlayWidth: (typeof workInfoOverlayWidth !== "undefined")
            ? workInfoOverlayWidth
            : Math.min(600, Math.max(240, view3d.height * 0.55))
        width: _overlayWidth
        height: contentColumn.implicitHeight + 24
        property var expandedAnchor: (view3d.fullyExpandedDelegateIndex >= 0)
            ? view3d.frontInfoAnchorByIndex[view3d.fullyExpandedDelegateIndex]
            : null
        property point projectedPoint: {
            if (!expandedAnchor)
                return Qt.point(-99999, -99999)
            var _cameraTick = orbitCamera.position.x + orbitCamera.position.y + orbitCamera.position.z
                + orbitCamera.eulerRotation.x + orbitCamera.eulerRotation.y + orbitCamera.eulerRotation.z
                + view3d.width + view3d.height
            var p = view3d.mapFrom3DScene(expandedAnchor.scenePosition)
            return Qt.point(p.x + _cameraTick * 0, p.y)
        }
        x: projectedPoint.x - width * 0.5
        y: projectedPoint.y - height * 0.5
        visible: view3d.fullyExpandedDelegateIndex >= 0
            && expandedAnchor
            && isFinite(projectedPoint.x)
            && isFinite(projectedPoint.y)

        // 剧照条：宽度与信息块相同；水平居光盘中心 2D 投影，垂直向下偏移约为场景高度 0.3 倍。
        readonly property real _fanartBelowDiscFactor: 0.41
        function _syncFanartStripLayout() {
            if (typeof dvdBridge === "undefined" || !dvdBridge
                || typeof dvdBridge.set_fanart_strip_layout !== "function")
                return
            if (!visible || !isFinite(x) || !isFinite(y) || !isFinite(width) || !isFinite(height))
                return
            var fi = view3d.fullyExpandedDelegateIndex
            var disc = fi >= 0 ? view3d.discCenterAnchorByIndex[fi] : null
            if (!disc)
                return
            var p = view3d.mapFrom3DScene(disc.scenePosition)
            if (!isFinite(p.x) || !isFinite(p.y))
                return
            var w = width
            dvdBridge.set_fanart_strip_layout(
                p.x - w * 0.5,
                p.y + view3d.height * _fanartBelowDiscFactor-124,
                w)
        }
        onVisibleChanged: _syncFanartStripLayout()
        onXChanged: _syncFanartStripLayout()
        onYChanged: _syncFanartStripLayout()
        onWidthChanged: _syncFanartStripLayout()
        onHeightChanged: _syncFanartStripLayout()

        property int expandedVirtualIndex: view3d.fullyExpandedDelegateIndex >= 0
            ? view3d.expandedVirtualIndexFor(view3d.fullyExpandedDelegateIndex)
            : -1

        // 展开时立即请求数据（动画期间加载）；完全展开后再次请求以兜底；折叠时传 -1。
        Connections {
            target: view3d
            function onExpandedDelegateIndexChanged() {
                if (typeof dvdBridge !== "undefined" && dvdBridge)
                    dvdBridge.refresh_expanded_work_meta(view3d.expandedDelegateIndex >= 0
                        ? view3d.expandedVirtualIndexFor(view3d.expandedDelegateIndex) : -1)
            }
            function onFullyExpandedDelegateIndexChanged() {
                if (typeof dvdBridge !== "undefined" && dvdBridge && view3d.fullyExpandedDelegateIndex >= 0)
                    dvdBridge.refresh_expanded_work_meta(view3d.expandedVirtualIndexFor(view3d.fullyExpandedDelegateIndex))
            }
        }

        Rectangle {
            anchors.fill: parent
            radius: 10
            color: "#CC101622"
            border.color: "#99ffffff"
            border.width: 1
        }

        // 文本内容：标题、简介、番号/日期与人员制作信息；作品标签在最后一节。
        Column {
            id: contentColumn
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 12
            spacing: 8
            Text {
                width: parent.width
                color: "#ffffff"
                font.pixelSize: 18
                font.bold: true
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                text: (typeof dvdBridge !== "undefined" && dvdBridge && dvdBridge.expandedWorkTitle)
                    ? dvdBridge.expandedWorkTitle : ""
            }
            Text {
                width: parent.width
                color: "#e8ecf5"
                font.pixelSize: 14
                wrapMode: Text.WordWrap
                maximumLineCount: 6
                elide: Text.ElideRight
                text: (typeof dvdBridge !== "undefined" && dvdBridge && dvdBridge.expandedWorkStory)
                    ? dvdBridge.expandedWorkStory : ""
            }
            Row {
                width: parent.width
                visible: typeof dvdBridge !== "undefined" && dvdBridge && dvdBridge.expandedWorkCode
                spacing: 4
                Text {
                    color: "#a0a8b8"
                    font.pixelSize: 14
                    text: "番号: "
                }
                MouseArea {
                    width: codeText.implicitWidth
                    height: codeText.implicitHeight
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: {
                        if (typeof dvdBridge !== "undefined" && dvdBridge && dvdBridge.expandedWorkCode)
                            dvdBridge.copy_to_clipboard(dvdBridge.expandedWorkCode)
                    }
                    Text {
                        id: codeText
                        color: hovered ? "#e8ecf5" : "#e8ecf5"
                        font.pixelSize: 14
                        font.underline: hovered
                        text: (typeof dvdBridge !== "undefined" && dvdBridge && dvdBridge.expandedWorkCode)
                            ? dvdBridge.expandedWorkCode : ""
                        property bool hovered: parent.containsMouse
                    }
                }
            }
            Text {
                width: parent.width
                color: "#a0a8b8"
                font.pixelSize: 14
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
                visible: text !== ""
                text: (typeof dvdBridge !== "undefined" && dvdBridge && dvdBridge.expandedWorkReleaseDate)
                    ? ("发布日期: " + dvdBridge.expandedWorkReleaseDate) : ""
            }

            // 女优
            Column {
                width: parent.width - 24
                spacing: 4
                visible: typeof dvdBridge !== "undefined" && dvdBridge && dvdBridge.expandedWorkActresses
                    && dvdBridge.expandedWorkActresses.length > 0
                Text {
                    text: "女优"
                    color: "#a0a8b8"
                    font.pixelSize: 14
                }
                Flow {
                    width: parent.width
                    spacing: 4
                    Repeater {
                        model: (typeof dvdBridge !== "undefined" && dvdBridge) ? dvdBridge.expandedWorkActresses : []
                        delegate: Rectangle {
                            width: actressLabel.implicitWidth + 10
                            height: actressLabel.implicitHeight + 4
                            radius: 4
                            color: "#ffffff"
                            Text {
                                id: actressLabel
                                anchors.centerIn: parent
                                text: modelData ? modelData.actress_name : ""
                                font.pixelSize: 14
                                color: "#333333"
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (typeof dvdBridge !== "undefined" && dvdBridge && modelData)
                                        dvdBridge.on_actress_clicked(modelData.actress_id)
                                }
                            }
                        }
                    }
                }
            }

            // 男优
            Column {
                width: parent.width - 24
                spacing: 4
                visible: typeof dvdBridge !== "undefined" && dvdBridge && dvdBridge.expandedWorkActors
                    && dvdBridge.expandedWorkActors.length > 0
                Text {
                    text: "男优"
                    color: "#a0a8b8"
                    font.pixelSize: 14
                }
                Flow {
                    width: parent.width
                    spacing: 4
                    Repeater {
                        model: (typeof dvdBridge !== "undefined" && dvdBridge) ? dvdBridge.expandedWorkActors : []
                        delegate: Rectangle {
                            width: actorLabel.implicitWidth + 10
                            height: actorLabel.implicitHeight + 4
                            radius: 4
                            color: "#ffffff"
                            Text {
                                id: actorLabel
                                anchors.centerIn: parent
                                text: modelData ? modelData.actor_name : ""
                                font.pixelSize: 14
                                color: "#333333"
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (typeof dvdBridge !== "undefined" && dvdBridge && modelData)
                                        dvdBridge.on_actor_clicked(modelData.actor_id)
                                }
                            }
                        }
                    }
                }
            }

            // 导演
            Row {
                width: parent.width
                spacing: 4
                visible: typeof dvdBridge !== "undefined" && dvdBridge && dvdBridge.expandedWorkDirector
                    && dvdBridge.expandedWorkDirector.length > 0
                Text {
                    text: "导演: "
                    color: "#a0a8b8"
                    font.pixelSize: 14
                }
                Item {
                    width: directorText.implicitWidth
                    height: directorText.implicitHeight
                    Text {
                        id: directorText
                        text: (typeof dvdBridge !== "undefined" && dvdBridge) ? dvdBridge.expandedWorkDirector : ""
                        color: "#e8ecf5"
                        font.pixelSize: 14
                        font.underline: directorMouseArea.containsMouse
                    }
                    MouseArea {
                        id: directorMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof dvdBridge !== "undefined" && dvdBridge)
                                dvdBridge.on_director_clicked()
                        }
                    }
                }
            }

            // 片商
            Row {
                width: parent.width
                spacing: 4
                visible: typeof dvdBridge !== "undefined" && dvdBridge && dvdBridge.expandedWorkStudio
                    && dvdBridge.expandedWorkStudio.length > 0
                Text {
                    text: "片商: "
                    color: "#a0a8b8"
                    font.pixelSize: 14
                }
                Item {
                    width: studioText.implicitWidth
                    height: studioText.implicitHeight
                    Text {
                        id: studioText
                        text: (typeof dvdBridge !== "undefined" && dvdBridge) ? dvdBridge.expandedWorkStudio : ""
                        color: "#e8ecf5"
                        font.pixelSize: 14
                        font.underline: studioMouseArea.containsMouse && studioMouseArea.enabled
                    }
                    MouseArea {
                        id: studioMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: typeof dvdBridge !== "undefined" && dvdBridge && dvdBridge.expandedWorkMakerId > 0
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: {
                            if (typeof dvdBridge !== "undefined" && dvdBridge)
                                dvdBridge.on_studio_clicked()
                        }
                    }
                }
            }

            // 厂牌（label）
            Row {
                width: parent.width
                spacing: 4
                visible: typeof dvdBridge !== "undefined" && dvdBridge && dvdBridge.expandedWorkLabel
                    && dvdBridge.expandedWorkLabel.length > 0
                Text {
                    id: labelPrefix
                    text: "厂牌: "
                    color: "#a0a8b8"
                    font.pixelSize: 14
                }
                Item {
                    width: parent.width - labelPrefix.width - parent.spacing
                    height: labelText.implicitHeight
                    Text {
                        id: labelText
                        width: parent.width
                        color: "#e8ecf5"
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                        elide: Text.ElideRight
                        maximumLineCount: 2
                        font.underline: labelMouseArea.containsMouse && labelMouseArea.enabled
                        text: (typeof dvdBridge !== "undefined" && dvdBridge) ? dvdBridge.expandedWorkLabel : ""
                    }
                    MouseArea {
                        id: labelMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: typeof dvdBridge !== "undefined" && dvdBridge && dvdBridge.expandedWorkLabelId > 0
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: {
                            if (typeof dvdBridge !== "undefined" && dvdBridge)
                                dvdBridge.on_label_clicked()
                        }
                    }
                }
            }

            // 系列（series）
            Row {
                width: parent.width
                spacing: 4
                visible: typeof dvdBridge !== "undefined" && dvdBridge && dvdBridge.expandedWorkSeries
                    && dvdBridge.expandedWorkSeries.length > 0
                Text {
                    id: seriesPrefix
                    text: "系列: "
                    color: "#a0a8b8"
                    font.pixelSize: 14
                }
                Item {
                    width: parent.width - seriesPrefix.width - parent.spacing
                    height: seriesText.implicitHeight
                    Text {
                        id: seriesText
                        width: parent.width
                        color: "#e8ecf5"
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                        elide: Text.ElideRight
                        maximumLineCount: 2
                        font.underline: seriesMouseArea.containsMouse && seriesMouseArea.enabled
                        text: (typeof dvdBridge !== "undefined" && dvdBridge) ? dvdBridge.expandedWorkSeries : ""
                    }
                    MouseArea {
                        id: seriesMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: typeof dvdBridge !== "undefined" && dvdBridge && dvdBridge.expandedWorkSeriesId > 0
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: {
                            if (typeof dvdBridge !== "undefined" && dvdBridge)
                                dvdBridge.on_series_clicked()
                        }
                    }
                }
            }

            // 作品标签（置底）
            Column {
                width: parent.width - 24
                spacing: 4
                visible: typeof dvdBridge !== "undefined" && dvdBridge && dvdBridge.expandedWorkTags
                    && dvdBridge.expandedWorkTags.length > 0
                Text {
                    text: "作品标签"
                    color: "#a0a8b8"
                    font.pixelSize: 14
                }
                Flow {
                    width: parent.width
                    spacing: 4
                    Repeater {
                        model: (typeof dvdBridge !== "undefined" && dvdBridge) ? dvdBridge.expandedWorkTags : []
                        delegate: Rectangle {
                            width: tagLabel.implicitWidth + 10
                            height: tagLabel.implicitHeight + 4
                            radius: 4
                            color: (modelData && modelData.color) ? modelData.color : "#cccccc"
                            Text {
                                id: tagLabel
                                anchors.centerIn: parent
                                text: modelData ? modelData.tag_name : ""
                                font.pixelSize: 14
                                color: (modelData && modelData.text_color) ? modelData.text_color : "#333333"
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                onClicked: {
                                    if (typeof dvdBridge !== "undefined" && dvdBridge && modelData)
                                        dvdBridge.on_tag_clicked(modelData.tag_id)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // spine 中心操作层：爱心、编辑、删除按钮的 2D 投影，仅完全展开后显示。
    Item {
        id: actionOverlay
        z: 2
        width: 48
        height: actionColumn.height
        property var expandedAnchor: (view3d.fullyExpandedDelegateIndex >= 0)
            ? view3d.actionAnchorByIndex[view3d.fullyExpandedDelegateIndex]
            : null
        property point projectedPoint: {
            if (!expandedAnchor)
                return Qt.point(-99999, -99999)

            // Keep bindings reactive to camera movement.
            var _cameraTick = orbitCamera.position.x + orbitCamera.position.y + orbitCamera.position.z
                + orbitCamera.eulerRotation.x + orbitCamera.eulerRotation.y + orbitCamera.eulerRotation.z
                + view3d.width + view3d.height
            var p = view3d.mapFrom3DScene(expandedAnchor.scenePosition)
            return Qt.point(p.x + _cameraTick * 0, p.y)
        }
        x: projectedPoint.x - width * 0.5
        y: projectedPoint.y - height * 0.5
        visible: view3d.fullyExpandedDelegateIndex >= 0
            && expandedAnchor
            && isFinite(projectedPoint.x)
            && isFinite(projectedPoint.y)

        property int expandedVirtualIndex: view3d.fullyExpandedDelegateIndex >= 0
            ? view3d.expandedVirtualIndexFor(view3d.fullyExpandedDelegateIndex)
            : -1

        // 展开时立即请求收藏状态；完全展开后再次请求以兜底，确保爱心显示正确。
        Connections {
            target: view3d
            function onExpandedDelegateIndexChanged() {
                if (view3d.expandedDelegateIndex >= 0 && typeof dvdBridge !== "undefined" && dvdBridge)
                    dvdBridge.refresh_expanded_favorite_state(view3d.expandedVirtualIndexFor(view3d.expandedDelegateIndex))
            }
            function onFullyExpandedDelegateIndexChanged() {
                if (view3d.fullyExpandedDelegateIndex >= 0 && typeof dvdBridge !== "undefined" && dvdBridge)
                    dvdBridge.refresh_expanded_favorite_state(view3d.expandedVirtualIndexFor(view3d.fullyExpandedDelegateIndex))
            }
        }

        Column {
            id: actionColumn
            spacing: 8
            anchors.horizontalCenter: parent.horizontalCenter


            // 收藏按钮（爱心）。
            Item {
                width: 40
                height: 40
                Image {
                    id: heartImg
                    anchors.centerIn: parent
                    width: 28
                    height: 28
                    source: (typeof dvdBridge !== "undefined" && dvdBridge && dvdBridge.expandedWorkFavorited)
                        ? Qt.resolvedUrl("icons/love-on.svg")
                        : Qt.resolvedUrl("icons/love-off.svg")
                    fillMode: Image.PreserveAspectFit
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof dvdBridge !== "undefined" && dvdBridge)
                            dvdBridge.on_heart_clicked(actionOverlay.expandedVirtualIndex)
                    }
                }
            }


            // 编辑按钮。
            Item {
                width: 40
                height: 40
                Image {
                    anchors.centerIn: parent
                    width: 24
                    height: 24
                    source: Qt.resolvedUrl("icons/square-pen.svg")
                    fillMode: Image.PreserveAspectFit
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof dvdBridge !== "undefined" && dvdBridge)
                            dvdBridge.on_edit_clicked(actionOverlay.expandedVirtualIndex)
                    }
                }
            }


            // 删除按钮。
            Item {
                width: 40
                height: 40
                Image {
                    anchors.centerIn: parent
                    width: 24
                    height: 24
                    source: Qt.resolvedUrl("icons/trash-2.svg")
                    fillMode: Image.PreserveAspectFit
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof dvdBridge !== "undefined" && dvdBridge)
                            dvdBridge.on_delete_clicked(actionOverlay.expandedVirtualIndex)
                    }
                }
            }
        }
    }

}
