import QtQuick
import QtQuick3D

Item {
    width: 48
    height: 48

    View3D {
        anchors.fill: parent

        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.SkyBox
            lightProbe: Texture {
                source: hdrPath + "lebombo_1k.hdr"
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
        }

        PerspectiveCamera {
            y: 0.1
            z: 0.25
            clipNear: 0.001
            clipFar: 100000
            fieldOfView: 60
        }

        DirectionalLight {
            eulerRotation.x: -35
            eulerRotation.y: 25
            castsShadow: true
            shadowMapQuality: Light.ShadowMapQualityHigh
        }

        // Instantiate the production DVD component once. Opening it also
        // creates the lazily loaded disc and all material variants, warming
        // the exact mesh/shader/HDR path used by the shelf.
        Loader3D {
            source: prewarmDvdUrl
            onItemChanged: {
                if (item) {
                    item.textureSource = mapsPath + "0.png"
                    item.expanded = true
                }
            }
        }
    }
}
