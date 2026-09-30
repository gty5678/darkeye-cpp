DarkEye
│
├── Application
│
├── Database
│
├── UI / QWidget
│
└── 3D
     │
     ├── viewport/
     │    ├── DvdViewport
     │    ├── CameraController
     │    └── SelectionManager
     │
     ├── scene/
     │    ├── Scene
     │    ├── SceneNode
     │    ├── Transform
     │    └── Light
     │
     ├── renderer/
     │    ├── Renderer
     │    ├── Mesh
     │    ├── Material
     │    ├── Texture
     │    ├── ShadowPass
     │    └── PbrPipeline
     │
     ├── asset/
     │    ├── GltfLoader
     │    └── ResourceManager
     │
     └── animation/
          └── TransformAnimator

                    ↓

                  QRhi

                    ↓

       D3D11 / D3D12 / Vulkan
          Metal / OpenGL