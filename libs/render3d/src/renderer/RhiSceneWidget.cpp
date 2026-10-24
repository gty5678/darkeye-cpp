#include "render3d/RhiSceneWidget.h"
#include "Renderer.h"

#include <QLoggingCategory>
#include <QElapsedTimer>
#include <algorithm>
#include <cmath>

Q_DECLARE_LOGGING_CATEGORY(render3dLog)

namespace darkeye::render3d {

RhiSceneWidget::RhiSceneWidget(QWidget *parent)
    : QRhiWidget(parent), m_scene(std::make_shared<Scene>()), m_renderer(std::make_unique<Renderer>())
{
#ifdef Q_OS_WIN
    setApi(Api::Direct3D11);
#endif
    setSampleCount(1);
    connect(this, &QRhiWidget::frameSubmitted, this, [this] {
        ++m_statistics.submittedFrames;
    });
    connect(this, &QRhiWidget::renderFailed, this, [this] {
        reportError(QStringLiteral("QRhiWidget failed to initialize or submit a frame"));
    });
}

RhiSceneWidget::~RhiSceneWidget()
{
    // Qt cannot call our virtual releaseResources from its base destructor.
    releaseResources();
}

bool RhiSceneWidget::setMesh(MeshData mesh)
{
    const auto error = mesh.validationError();
    if (!error.isEmpty()) {
        reportError(error);
        return false;
    }
    m_scene = std::make_shared<Scene>();
    if (!mesh.indices.empty()) {
        const auto node = m_scene->createNode();
        auto material = std::make_shared<Material>();
        material->doubleSided = true;
        m_scene->setMesh(node, std::make_shared<const MeshData>(std::move(mesh)), material);
    }
    m_lastError.clear();
    update();
    return true;
}

void RhiSceneWidget::setScene(std::shared_ptr<Scene> scene)
{
    m_scene = scene ? std::move(scene) : std::make_shared<Scene>();
    m_lastError.clear();
    update();
}

void RhiSceneWidget::setCamera(const Camera &camera)
{
    if (!std::isfinite(camera.distance) || camera.distance <= 0
        || !std::isfinite(camera.target.lengthSquared()) || !std::isfinite(camera.yaw)
        || !std::isfinite(camera.pitch) || !std::isfinite(camera.verticalFov)
        || camera.verticalFov <= 0 || camera.verticalFov >= 180
        || !std::isfinite(camera.nearPlane) || camera.nearPlane <= 0
        || !std::isfinite(camera.farPlane) || camera.farPlane <= camera.nearPlane)
        return;
    m_camera = camera;
    update();
}

void RhiSceneWidget::setRotation(const QQuaternion &rotation)
{
    if (rotation.isNull() || !std::isfinite(rotation.lengthSquared()))
        return;
    const auto normalized = rotation.normalized();
    if (m_rotation == normalized)
        return;
    m_rotation = normalized;
    update();
}

void RhiSceneWidget::setCameraDistance(float distance)
{
    if (!std::isfinite(distance))
        return;
    distance = std::clamp(distance, 3.5f, 30.0f);
    if (m_camera.distance == distance)
        return;
    m_camera.distance = distance;
    update();
}

void RhiSceneWidget::setClearColor(const QColor &color)
{
    if (!color.isValid() || color == m_clearColor)
        return;
    m_clearColor = color;
    update();
}

void RhiSceneWidget::setTextureBudget(quint64 bytes)
{
    m_renderer->setTextureBudget(bytes);
    update();
}

const RenderStatistics &RhiSceneWidget::statistics() const { return m_statistics; }
void RhiSceneWidget::setEnvironment(std::shared_ptr<const EnvironmentMap> environment)
{
    m_renderer->setEnvironment(std::move(environment)); update();
}
void RhiSceneWidget::setLighting(const LightingSettings &settings)
{
    auto valid=settings;
    const auto clamp=[](float value,float low,float high,float fallback) {
        return std::isfinite(value)?std::clamp(value,low,high):fallback;
    };
    valid.exposure=clamp(valid.exposure,0.05f,8.0f,1);
    valid.environmentIntensity=clamp(valid.environmentIntensity,0,8,1);
    valid.environmentRotation=clamp(valid.environmentRotation,-360,360,0);
    valid.keyLightIntensity=clamp(valid.keyLightIntensity,0,8,2.4f);
    if(!std::isfinite(valid.keyLightDirection.lengthSquared()) || valid.keyLightDirection.lengthSquared()<1e-8f)
        valid.keyLightDirection={0.5f,1,0.6f};
    valid.keyLightDirection.normalize();
    valid.aoRadius=clamp(valid.aoRadius,0.0001f,10,0.025f);
    valid.aoStrength=clamp(valid.aoStrength,0,3,1);
    valid.transmissionPassLimit=std::clamp(valid.transmissionPassLimit,1,16);
    m_renderer->setLighting(valid); update();
}
LightingSettings RhiSceneWidget::lighting() const { return m_renderer->lighting(); }
QString RhiSceneWidget::lastError() const { return m_lastError; }

void RhiSceneWidget::reportError(const QString &message)
{
    if (m_lastError == message)
        return;
    m_lastError = message;
    qCCritical(render3dLog).noquote() << message;
    emit renderingError(message);
}

void RhiSceneWidget::initialize(QRhiCommandBuffer *cb)
{
    // initialize is also called for target-only changes. prepare checks the
    // actual QRhi, sample count and pass compatibility without rebuilding on
    // every resize. No render-target pointer is kept across callbacks.
    qCInfo(render3dLog) << "initialize" << this << "QRhi" << rhi()
                       << "pixels" << renderTarget()->pixelSize()
                       << "DPR" << devicePixelRatioF()
                       << "samples" << renderTarget()->sampleCount();
    QString error;
    if (!m_renderer->prepare(rhi(), renderTarget(), m_scene->renderItems(),
                              cb, m_statistics, error))
        reportError(error);
}

void RhiSceneWidget::render(QRhiCommandBuffer *cb)
{
    if (!rhi() || !renderTarget() || renderTarget()->pixelSize().isEmpty())
        return;
    const auto size = renderTarget()->pixelSize();
    QElapsedTimer frameTimer;
    frameTimer.start();
    if (m_statistics.pixelSize != size) {
        qCInfo(render3dLog) << "Surface size" << size << "DPR" << devicePixelRatioF();
        m_statistics.pixelSize = size;
    }
    m_statistics.samples = renderTarget()->sampleCount();
    QString error;
    if (!m_renderer->prepare(rhi(), renderTarget(), m_scene->renderItems(),
                              cb, m_statistics, error)) {
        cb->beginPass(renderTarget(), QColor(Qt::darkRed), {1.0f, 0});
        cb->endPass();
        reportError(error);
        return;
    }
    m_renderer->draw(renderTarget(), cb, m_camera, m_rotation, m_clearColor, m_statistics);
    m_statistics.cpuFrameMs = double(frameTimer.nsecsElapsed()) / 1e6;
    // Requests come from CPU state changes or the demo's animation timer.
}

void RhiSceneWidget::releaseResources()
{
    m_statistics.transmissionPasses=0;
    m_statistics.transmissionFallbackItems=0;
    qCInfo(render3dLog) << "Release GPU resources" << this;
    m_renderer->release();
    m_statistics.bufferBytes = 0;
    m_statistics.textureBytes = 0;
    m_statistics.sourceTextureBytes = 0;
    m_statistics.geometryCount = 0;
    m_statistics.textureCount = 0;
}

} // namespace darkeye::render3d
