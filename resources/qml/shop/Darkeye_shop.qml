import QtQuick
import QtQuick3D

Node {
    id: shelf
    property var coverTextureSources: []

    function coverObjectAt(index) {
        return index >= 0 && index < coverRepeater.count ? coverRepeater.objectAt(index) : null
    }

    Model {
        objectName: "ShopBackPanel"
        source: "meshes/cube_004.mesh"
        // Python's back panel spans local Y -0.01..1.26. Fit it to the
        // three-row shelf's world Y 0.50..1.27 without changing its depth.
        scale.y: 0.77 / 1.27
        y: 0.50 + 0.01 * scale.y
        materials: PrincipledMaterial {
            baseColor: "#ffcca622"
            metalness: 0
            roughness: 0.5
            cullMode: Material.NoCulling
        }
    }

    // Keep the original shop's shelf and cover meshes, with three rows.
    Repeater3D {
        model: 3
        delegate: Model {
            required property int index
            // Retain Python's top row and its original 0.25 row spacing.
            y: 1.01 - index * 0.25
            source: "meshes/cube_005.mesh"
            materials: PrincipledMaterial {
                baseColor: "#ffcca622"
                metalness: 0
                roughness: 0.5
                cullMode: Material.NoCulling
            }
        }
    }

    Repeater3D {
        id: coverRepeater
        // Keep models/materials alive when only the selected covers change.
        model: shelf.coverTextureSources.length
        delegate: Node {
            id: cover
            objectName: "HomeCover" + index
            required property int index
            x: -0.394006 + (index % 6) * 0.16
            y: 1.115 - Math.floor(index / 6) * 0.25
            z: -0.043
            eulerRotation.y: -90
            Model {
                property int coverIndex: cover.index
                pickable: true
                source: "meshes/base.mesh"
                materials: [
                    PrincipledMaterial {
                        baseColorMap: Texture {
                            sourceItem: Image {
                                objectName: "HomeCoverImage" + cover.index
                                source: shelf.coverTextureSources[cover.index]
                                asynchronous: true
                                sourceSize: Qt.size(768, 768)
                                visible: false
                            }
                            tilingModeHorizontal: Texture.Repeat
                            tilingModeVertical: Texture.Repeat
                        }
                        opacityChannel: Material.A
                        metalness: 0
                        roughness: 0.08
                        cullMode: Material.NoCulling
                    },
                    PrincipledMaterial {
                        baseColor: "#94c1cc"
                        roughness: 0.3
                        cullMode: Material.NoCulling
                    }
                ]
            }
        }
    }
}
