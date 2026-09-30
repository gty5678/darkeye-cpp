#include "ForceViewRhiWidget.h"

#include "Forces.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QMatrix4x4>
#include <QMetaObject>
#include <QPointer>
#include <QResizeEvent>
#include <QThread>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtWidgets/QRhiWidget>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <random>

namespace {

// 返回高精度秒级时间戳，用于绘制、仿真和 FPS 统计。
double nowSec()
{
    using clock = std::chrono::high_resolution_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

// 从 Qt 资源中读取并反序列化 QRhi 使用的 qsb shader。
QShader loadShader(const QString& name)
{
    QFile f(QStringLiteral(":/forceview/shaders/shaders/%1.qsb").arg(name));
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning("ForceViewRhiWidget: failed to open shader resource %s",
                 qPrintable(f.fileName()));
        return {};
    }
    return QShader::fromSerialized(f.readAll());
}

} // namespace

class ForceRhiCanvas : public QRhiWidget
{
public:
    // 创建实际承载 QRhi 渲染的 QRhiWidget，并记录外层 ForceViewRhiWidget。
    explicit ForceRhiCanvas(ForceViewRhiWidget* owner)
        : QRhiWidget(owner)
        , m_owner(owner)
    {
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setSampleCount(1);
    }

    // 暴露 QRhiWidget 内部 QRhi 对象给外层渲染器。
    QRhi* rhiObject() const { return rhi(); }
    // 暴露当前 render target 给外层渲染器创建管线和开始 render pass。
    QRhiRenderTarget* currentRenderTarget() const { return renderTarget(); }
    // 外层析构时断开 owner，避免 QRhiWidget 回调访问已析构对象。
    void detachOwner() { m_owner = nullptr; }

protected:
    // QRhiWidget 初始化回调，转交给外层创建基础 QRhi 资源。
    void initialize(QRhiCommandBuffer* cb) override
    {
        if (m_owner) {
            m_owner->initializeRhi(cb);
        }
    }

    // QRhiWidget 绘制回调，转交给外层执行完整渲染流程。
    void render(QRhiCommandBuffer* cb) override
    {
        if (m_owner) {
            m_owner->renderRhi(cb);
        }
    }

    // QRhiWidget 资源释放回调，转交给外层清理 QRhi 资源。
    void releaseResources() override
    {
        if (m_owner) {
            m_owner->releaseRhiResources();
        }
    }

    // 转发滚轮事件，让外层统一处理缩放逻辑。
    void wheelEvent(QWheelEvent* event) override
    {
        if (m_owner) {
            m_owner->wheelEvent(event);
        } else {
            QRhiWidget::wheelEvent(event);
        }
    }

    // 转发鼠标按下事件，让外层统一处理选择、拖拽和平移。
    void mousePressEvent(QMouseEvent* event) override
    {
        if (m_owner) {
            m_owner->mousePressEvent(event);
        } else {
            QRhiWidget::mousePressEvent(event);
        }
    }

    // 转发鼠标移动事件，让外层统一处理 hover、拖拽和平移。
    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (m_owner) {
            m_owner->mouseMoveEvent(event);
        } else {
            QRhiWidget::mouseMoveEvent(event);
        }
    }

    // 转发鼠标释放事件，让外层统一结束拖拽或平移。
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (m_owner) {
            m_owner->mouseReleaseEvent(event);
        } else {
            QRhiWidget::mouseReleaseEvent(event);
        }
    }

    // 转发离开事件，让外层清理 hover 状态。
    void leaveEvent(QEvent* event) override
    {
        if (m_owner) {
            m_owner->leaveEvent(event);
        } else {
            QRhiWidget::leaveEvent(event);
        }
    }

private:
    ForceViewRhiWidget* m_owner = nullptr;
};

ForceViewRhiWidget::ForceViewRhiWidget(QWidget* parent)
    : QWidget(parent)
{
    m_fontPath = detectDefaultFontPath();
    m_fontAtlas = std::make_unique<MsdfFontAtlas>();
    m_fontAtlas->initialize(makeFontConfig());

    setupLayout();
    setMinimumSize(200, 150);
    setMouseTracking(true);
    updateFactor();
    connect(m_canvas, &QRhiWidget::frameSubmitted,
            this, &ForceViewRhiWidget::onFrameSubmitted);
}

ForceViewRhiWidget::~ForceViewRhiWidget()
{
    stopSimThread();
    releaseRhiResources();
    if (m_canvas) {
        m_canvas->detachOwner();
    }
    m_msdfAtlasThreadRunning.store(false, std::memory_order_release);
    if (m_msdfAtlasThread.joinable()) {
        m_msdfAtlasThread.join();
    }
}

void ForceViewRhiWidget::setupLayout()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_canvas = new ForceRhiCanvas(this);
    layout->addWidget(m_canvas);
}

QString ForceViewRhiWidget::detectDefaultFontPath()
{
    static const char* const candidates[] = {
#ifdef Q_OS_WIN
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/msyhl.ttc",
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/simsun.ttc",
        "C:/Windows/Fonts/arial.ttf",
#elif defined(Q_OS_MAC)
        "/System/Library/Fonts/PingFang.ttc",
        "/System/Library/Fonts/STHeiti Light.ttc",
        "/System/Library/Fonts/Helvetica.ttc",
        "/Library/Fonts/Arial.ttf",
#else
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
#endif
    };

    for (const char* path : candidates) {
        if (QFileInfo::exists(QString::fromUtf8(path))) {
            return QString::fromUtf8(path);
        }
    }
    return {};
}

MsdfFontAtlas::Config ForceViewRhiWidget::makeFontConfig() const
{
    MsdfFontAtlas::Config cfg;
    cfg.fontPath = m_fontPath;
    cfg.atlasWidth = 2048;
    cfg.atlasHeight = 2048;
    cfg.pxRange = 6.0f;
    return cfg;
}

void ForceViewRhiWidget::update()
{
    // Match the Python binding: QRhiWidget owns coalescing its update events.
    // Filtering on isVisible() can drop simulation frames while a stacked
    // page or native QRhi child is transitioning into the visible state.
    if (m_canvas) {
        m_canvas->update();
    }
    QWidget::update();
}

void ForceViewRhiWidget::initializeRhi(QRhiCommandBuffer* cb)
{
    Q_UNUSED(cb);
    m_rhi = m_canvas ? m_canvas->rhiObject() : nullptr;
    m_rhiReady = false;
    m_pipelineDirty = true;
    ensureRhiResources();
}

void ForceViewRhiWidget::releaseRhiResources()
{
    releaseTextRhiResources();
    m_pipeline.reset();
    m_srb.reset();
    m_uniformBuffer.reset();
    m_vertexBuffer.reset();
    m_vertexBufferSize = 0;
    m_rhiReady = false;
}

bool ForceViewRhiWidget::ensureRhiResources()
{
    if (!m_rhi) {
        return false;
    }

    if (!m_uniformBuffer) {
        m_uniformBuffer.reset(
            m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64));
        if (!m_uniformBuffer->create()) {
            qWarning("ForceViewRhiWidget: failed to create uniform buffer");
            m_uniformBuffer.reset();
            return false;
        }
    }

    if (!m_srb) {
        m_srb.reset(m_rhi->newShaderResourceBindings());
        m_srb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(
                0,
                QRhiShaderResourceBinding::VertexStage,
                m_uniformBuffer.get())
        });
        if (!m_srb->create()) {
            qWarning("ForceViewRhiWidget: failed to create shader resources");
            m_srb.reset();
            return false;
        }
        m_pipelineDirty = true;
    }

    m_rhiReady = true;
    return true;
}

bool ForceViewRhiWidget::ensureVertexBuffer(quint32 byteSize)
{
    if (!m_rhi || byteSize == 0) {
        return false;
    }
    if (m_vertexBuffer && m_vertexBufferSize >= byteSize) {
        return true;
    }

    const quint32 rounded = std::max<quint32>(4096, byteSize + byteSize / 2);
    m_vertexBuffer.reset(
        m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, rounded));
    if (!m_vertexBuffer->create()) {
        qWarning("ForceViewRhiWidget: failed to create vertex buffer");
        m_vertexBuffer.reset();
        m_vertexBufferSize = 0;
        return false;
    }
    m_vertexBufferSize = rounded;
    return true;
}

void ForceViewRhiWidget::rebuildPipeline()
{
    if (!m_rhi || !m_srb || !m_canvas || !m_canvas->currentRenderTarget()) {
        return;
    }

    QShader vs = loadShader(QStringLiteral("force_rhi_color.vert"));
    QShader fs = loadShader(QStringLiteral("force_rhi_color.frag"));
    if (!vs.isValid() || !fs.isValid()) {
        qWarning("ForceViewRhiWidget: invalid QRhi shaders");
        return;
    }

    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        QRhiVertexInputBinding(9 * sizeof(float))
    });
    inputLayout.setAttributes({
        QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float2, 0),
        QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)),
        QRhiVertexInputAttribute(0, 2, QRhiVertexInputAttribute::Float, 4 * sizeof(float)),
        QRhiVertexInputAttribute(0, 3, QRhiVertexInputAttribute::Float4, 5 * sizeof(float))
    });

    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;

    m_pipeline.reset(m_rhi->newGraphicsPipeline());
    m_pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    m_pipeline->setCullMode(QRhiGraphicsPipeline::None);
    m_pipeline->setTargetBlends({blend});
    m_pipeline->setShaderStages({
        QRhiShaderStage(QRhiShaderStage::Vertex, vs),
        QRhiShaderStage(QRhiShaderStage::Fragment, fs)
    });
    m_pipeline->setVertexInputLayout(inputLayout);
    m_pipeline->setShaderResourceBindings(m_srb.get());
    m_pipeline->setRenderPassDescriptor(
        m_canvas->currentRenderTarget()->renderPassDescriptor());
    if (!m_pipeline->create()) {
        qWarning("ForceViewRhiWidget: failed to create graphics pipeline");
        m_pipeline.reset();
        return;
    }
    m_pipelineDirty = false;
}

void ForceViewRhiWidget::releaseTextRhiResources()
{
    m_textPipeline.reset();
    m_textSrb.reset();
    m_textSampler.reset();
    m_textAtlasTexture.reset();
    m_textUniformBuffer.reset();
    m_textVertexBuffer.reset();
    m_textVertexBufferSize = 0;
    m_textAtlasUploadedGeneration = -1;
    m_textPipelineDirty = true;
}

bool ForceViewRhiWidget::ensureTextVertexBuffer(quint32 byteSize)
{
    if (!m_rhi || byteSize == 0) {
        return false;
    }
    if (m_textVertexBuffer && m_textVertexBufferSize >= byteSize) {
        return true;
    }

    const quint32 rounded = std::max<quint32>(4096, byteSize + byteSize / 2);
    m_textVertexBuffer.reset(
        m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, rounded));
    if (!m_textVertexBuffer->create()) {
        qWarning("ForceViewRhiWidget: failed to create text vertex buffer");
        m_textVertexBuffer.reset();
        m_textVertexBufferSize = 0;
        return false;
    }
    m_textVertexBufferSize = rounded;
    return true;
}

bool ForceViewRhiWidget::ensureTextResources(QRhiResourceUpdateBatch* batch)
{
    if (!m_rhi || !m_fontAtlas || !m_fontAtlas->isReady() || !batch) {
        return false;
    }

    if (!m_textUniformBuffer) {
        m_textUniformBuffer.reset(
            m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 256));
        if (!m_textUniformBuffer->create()) {
            qWarning("ForceViewRhiWidget: failed to create text uniform buffer");
            m_textUniformBuffer.reset();
            return false;
        }
    }

    const int atlasW = m_fontAtlas->atlasWidth();
    const int atlasH = m_fontAtlas->atlasHeight();
    if (atlasW <= 0 || atlasH <= 0 || m_fontAtlas->atlasPixels().empty()) {
        return false;
    }

    const QSize atlasSize(atlasW, atlasH);
    if (!m_textAtlasTexture || m_textAtlasTexture->pixelSize() != atlasSize) {
        m_textPipeline.reset();
        m_textSrb.reset();
        m_textAtlasTexture.reset(m_rhi->newTexture(QRhiTexture::RGBA8, atlasSize));
        if (!m_textAtlasTexture->create()) {
            qWarning("ForceViewRhiWidget: failed to create text atlas texture");
            m_textAtlasTexture.reset();
            return false;
        }
        m_textAtlasUploadedGeneration = -1;
        m_textPipelineDirty = true;
    }

    if (!m_textSampler) {
        m_textSampler.reset(m_rhi->newSampler(
            QRhiSampler::Linear,
            QRhiSampler::Linear,
            QRhiSampler::None,
            QRhiSampler::ClampToEdge,
            QRhiSampler::ClampToEdge));
        if (!m_textSampler->create()) {
            qWarning("ForceViewRhiWidget: failed to create text sampler");
            m_textSampler.reset();
            return false;
        }
    }

    if (m_textAtlasUploadedGeneration != m_fontAtlas->generation()) {
        const std::vector<unsigned char>& rgb = m_fontAtlas->atlasPixels();
        QByteArray rgba;
        rgba.resize(atlasW * atlasH * 4);
        char* dst = rgba.data();
        for (int i = 0; i < atlasW * atlasH; ++i) {
            dst[4 * i + 0] = static_cast<char>(rgb[3 * i + 0]);
            dst[4 * i + 1] = static_cast<char>(rgb[3 * i + 1]);
            dst[4 * i + 2] = static_cast<char>(rgb[3 * i + 2]);
            dst[4 * i + 3] = static_cast<char>(255);
        }
        QRhiTextureSubresourceUploadDescription sub(rgba);
        sub.setDataStride(static_cast<quint32>(atlasW * 4));
        sub.setSourceSize(atlasSize);
        batch->uploadTexture(
            m_textAtlasTexture.get(),
            QRhiTextureUploadDescription(QRhiTextureUploadEntry(0, 0, sub)));
        m_textAtlasUploadedGeneration = m_fontAtlas->generation();
    }

    if (!m_textSrb) {
        m_textSrb.reset(m_rhi->newShaderResourceBindings());
        m_textSrb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(
                0,
                QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                m_textUniformBuffer.get()),
            QRhiShaderResourceBinding::sampledTexture(
                1,
                QRhiShaderResourceBinding::FragmentStage,
                m_textAtlasTexture.get(),
                m_textSampler.get())
        });
        if (!m_textSrb->create()) {
            qWarning("ForceViewRhiWidget: failed to create text shader resources");
            m_textSrb.reset();
            return false;
        }
        m_textPipelineDirty = true;
    }

    return true;
}

void ForceViewRhiWidget::rebuildTextPipeline()
{
    if (!m_rhi || !m_textSrb || !m_canvas || !m_canvas->currentRenderTarget()) {
        return;
    }

    QShader vs = loadShader(QStringLiteral("force_rhi_text.vert"));
    QShader fs = loadShader(QStringLiteral("force_rhi_text.frag"));
    if (!vs.isValid() || !fs.isValid()) {
        qWarning("ForceViewRhiWidget: invalid QRhi text shaders");
        return;
    }

    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        QRhiVertexInputBinding(9 * sizeof(float))
    });
    inputLayout.setAttributes({
        QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float2, 0),
        QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)),
        QRhiVertexInputAttribute(0, 2, QRhiVertexInputAttribute::Float4, 4 * sizeof(float)),
        QRhiVertexInputAttribute(0, 3, QRhiVertexInputAttribute::Float, 8 * sizeof(float))
    });

    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;

    m_textPipeline.reset(m_rhi->newGraphicsPipeline());
    m_textPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    m_textPipeline->setCullMode(QRhiGraphicsPipeline::None);
    m_textPipeline->setTargetBlends({blend});
    m_textPipeline->setShaderStages({
        QRhiShaderStage(QRhiShaderStage::Vertex, vs),
        QRhiShaderStage(QRhiShaderStage::Fragment, fs)
    });
    m_textPipeline->setVertexInputLayout(inputLayout);
    m_textPipeline->setShaderResourceBindings(m_textSrb.get());
    m_textPipeline->setRenderPassDescriptor(
        m_canvas->currentRenderTarget()->renderPassDescriptor());
    if (!m_textPipeline->create()) {
        qWarning("ForceViewRhiWidget: failed to create text graphics pipeline");
        m_textPipeline.reset();
        return;
    }
    m_textPipelineDirty = false;
}

void ForceViewRhiWidget::renderRhi(QRhiCommandBuffer* cb)
{
    if (!m_rhi || !m_canvas || !m_canvas->currentRenderTarget()) {
        return;
    }
    if (!ensureRhiResources()) {
        cb->beginPass(m_canvas->currentRenderTarget(), m_backgroundColor,
                      QRhiDepthStencilClearValue());
        cb->endPass();
        return;
    }

    const double paintStart = nowSec();
    applyMsdfAtlasResultIfReady();
    advanceHover();
    if (m_physicsState) {
        m_physicsState->copyRenderPos(m_renderPosSnapshot);
    } else {
        m_renderPosSnapshot.clear();
    }
    prepareFrameVertices();
    prepareTextVertices();

    QRhiRenderTarget* rt = m_canvas->currentRenderTarget();
    if (!m_pipeline
        || m_pipelineDirty
        || m_pipeline->renderPassDescriptor() != rt->renderPassDescriptor()) {
        rebuildPipeline();
    }

    QMatrix4x4 proj;
    const float halfW = (m_viewportW / 2.0f) / std::max(m_zoom, 1e-6f);
    const float halfH = (m_viewportH / 2.0f) / std::max(m_zoom, 1e-6f);
    const float left = m_panX - halfW;
    const float right = m_panX + halfW;
    const float bottom = m_panY + halfH;
    const float top = m_panY - halfH;
    proj.ortho(left, right, bottom, top, -1.0f, 1.0f);
    QMatrix4x4 mvp = m_rhi->clipSpaceCorrMatrix();
    mvp *= proj;

    QRhiResourceUpdateBatch* updates = m_rhi->nextResourceUpdateBatch();
    updates->updateDynamicBuffer(m_uniformBuffer.get(), 0, 64, mvp.constData());

    const quint32 vertexBytes = static_cast<quint32>(m_frameVertices.size() * sizeof(Vertex));
    const bool hasVertices = vertexBytes > 0 && ensureVertexBuffer(vertexBytes);
    if (hasVertices) {
        updates->updateDynamicBuffer(
            m_vertexBuffer.get(),
            0,
            vertexBytes,
            m_frameVertices.data());
    }

    const quint32 textBytes = static_cast<quint32>(m_textVertices.size() * sizeof(TextVertex));
    const bool hasTextVertices = textBytes > 0 && ensureTextVertexBuffer(textBytes);
    const bool hasTextResources = hasTextVertices && ensureTextResources(updates);
    if (hasTextResources) {
        float textUniforms[20] = {};
        std::memcpy(textUniforms, mvp.constData(), 16 * sizeof(float));
        const float dpr = m_canvas ? static_cast<float>(m_canvas->devicePixelRatioF())
                                   : static_cast<float>(devicePixelRatioF());
        const float screenGlyphSize = m_msdfFontSize * m_zoom * dpr;
        const float atlasGlyphSize = m_fontAtlas ? m_fontAtlas->targetInnerPixels() : 32.0f;
        textUniforms[16] = std::max(
            1.0f,
            m_fontAtlas ? m_fontAtlas->pxRange() * screenGlyphSize / atlasGlyphSize : 1.0f);
        updates->updateDynamicBuffer(m_textUniformBuffer.get(), 0, sizeof(textUniforms), textUniforms);
        updates->updateDynamicBuffer(
            m_textVertexBuffer.get(),
            0,
            textBytes,
            m_textVertices.data());
    }

    if (hasTextResources
        && (!m_textPipeline
            || m_textPipelineDirty
            || m_textPipeline->renderPassDescriptor() != rt->renderPassDescriptor())) {
        rebuildTextPipeline();
    }

    cb->beginPass(rt, m_backgroundColor, QRhiDepthStencilClearValue(), updates);
    const QSize pixelSize = rt->pixelSize();
    const QRhiViewport viewport(
        0.0f,
        0.0f,
        static_cast<float>(pixelSize.width()),
        static_cast<float>(pixelSize.height()));

    // hover/fade 帧先画暗化图形和暗化文字，再把高亮边/节点画到暗化文字上方，
    // 然后画普通文字，最后画放大的 hover 文字。这样高亮图形不会被无关标签压住。
    if (hasVertices && m_pipeline && m_srb) {
        cb->setGraphicsPipeline(m_pipeline.get());
        cb->setViewport(viewport);
        cb->setShaderResources(m_srb.get());
        const QRhiCommandBuffer::VertexInput vertexInput(m_vertexBuffer.get(), 0);
        cb->setVertexInput(0, 1, &vertexInput);
        if (m_geometryDimCount > 0) {
            cb->draw(m_geometryDimCount, 1, m_geometryDimFirst);
        }
    }
    if (hasTextResources && m_textPipeline && m_textSrb) {
        cb->setGraphicsPipeline(m_textPipeline.get());
        cb->setViewport(viewport);
        cb->setShaderResources(m_textSrb.get());
        const QRhiCommandBuffer::VertexInput vertexInput(m_textVertexBuffer.get(), 0);
        cb->setVertexInput(0, 1, &vertexInput);
        if (m_textDimCount > 0) {
            cb->draw(m_textDimCount, 1, m_textDimFirst);
        }
    }
    if (hasVertices && m_pipeline && m_srb
        && (m_geometryHighlightCount > 0 || m_geometryRestCount > 0)) {
        cb->setGraphicsPipeline(m_pipeline.get());
        cb->setViewport(viewport);
        cb->setShaderResources(m_srb.get());
        const QRhiCommandBuffer::VertexInput vertexInput(m_vertexBuffer.get(), 0);
        cb->setVertexInput(0, 1, &vertexInput);
        if (m_geometryHighlightCount > 0) {
            cb->draw(m_geometryHighlightCount, 1, m_geometryHighlightFirst);
        }
        if (m_geometryRestCount > 0) {
            cb->draw(m_geometryRestCount, 1, m_geometryRestFirst);
        }
    }
    if (hasTextResources && m_textPipeline && m_textSrb
        && (m_textRestCount > 0 || m_textHoverCount > 0)) {
        cb->setGraphicsPipeline(m_textPipeline.get());
        cb->setViewport(viewport);
        cb->setShaderResources(m_textSrb.get());
        const QRhiCommandBuffer::VertexInput vertexInput(m_textVertexBuffer.get(), 0);
        cb->setVertexInput(0, 1, &vertexInput);
        if (m_textRestCount > 0) {
            cb->draw(m_textRestCount, 1, m_textRestFirst);
        }
        if (m_textHoverCount > 0) {
            cb->draw(m_textHoverCount, 1, m_textHoverFirst);
        }
    }
    cb->endPass();

    emit paintTimeUpdated(static_cast<float>((nowSec() - paintStart) * 1000.0));
}

void ForceViewRhiWidget::onFrameSubmitted()
{
    if (!m_hasSubmittedFrame) {
        m_hasSubmittedFrame = true;
        emit firstFrameSubmitted();
    }
    ++m_frameCount;
    const double now = nowSec();
    if (m_lastFpsTime <= 0.0) {
        m_lastFpsTime = now;
        return;
    }
    if (now - m_lastFpsTime >= 1.0) {
        m_currentFps = static_cast<float>(m_frameCount / (now - m_lastFpsTime));
        m_frameCount = 0;
        m_lastFpsTime = now;
        emit fpsUpdated(m_currentFps);
    }
}

void ForceViewRhiWidget::setGraph(int nNodes,
                                  const QVector<int>& edges,
                                  const QVector<float>& pos,
                                  const QStringList& id,
                                  const QStringList& labels,
                                  const QVector<float>& radii,
                                  const QVector<QColor>& nodeColors)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, nNodes, edges, pos, id, labels, radii, nodeColors]() {
            setGraph(nNodes, edges, pos, id, labels, radii, nodeColors);
        }, Qt::QueuedConnection);
        return;
    }

    stopSimThread();
    if (nNodes <= 0) {
        m_physicsState.reset();
        m_simulation.reset();
        m_simActive.store(false, std::memory_order_release);
        m_ids.clear();
        m_labels.clear();
        m_textVertices.clear();
        m_labelLayoutCache.clear();
        m_labelLayoutByIndex.clear();
        ++m_msdfAtlasBuildId;
        m_showRadiiBase.clear();
        m_showRadii.clear();
        m_nodeColors.clear();
        m_hoverIndex = -1;
        m_lastHoverIndex = -1;
        m_hoverGlobal = 0.0f;
        m_allowWarmup.store(false, std::memory_order_release);
        m_neighborMask.clear();
        m_lastNeighborMask.clear();
        m_lastDimEdges.clear();
        m_lastHighlightEdges.clear();
        requestCanvasUpdate();
        return;
    }

    m_physicsState = std::make_unique<PhysicsState>();
    std::vector<int> edgeVec(edges.begin(), edges.end());
    m_physicsState->init(nNodes, edgeVec);

    bool useInputPos = pos.size() >= 2 * nNodes;
    if (useInputPos) {
        for (int i = 0; i < nNodes; ++i) {
            const float x = pos[2 * i];
            const float y = pos[2 * i + 1];
            if (!std::isfinite(x) || !std::isfinite(y)) {
                useInputPos = false;
                break;
            }
        }
    }
    if (useInputPos) {
        for (int i = 0; i < nNodes; ++i) {
            m_physicsState->pos[2 * i] = pos[2 * i];
            m_physicsState->pos[2 * i + 1] = pos[2 * i + 1];
        }
    } else {
        const float L = std::sqrt(static_cast<float>(nNodes)) * kInitialLayoutScaleFactor
            + kInitialLayoutBaseOffset;
        std::mt19937 rng(static_cast<unsigned>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        std::uniform_real_distribution<float> dist(-L, L);
        for (int i = 0; i < nNodes; ++i) {
            m_physicsState->pos[2 * i] = dist(rng);
            m_physicsState->pos[2 * i + 1] = dist(rng);
        }
    }
    m_physicsState->syncRenderPosFromPos();
    m_physicsState->syncDragPosFromPos();

    m_ids = id;
    m_ids.resize(nNodes);
    for (int i = 0; i < nNodes; ++i) {
        if (m_ids[i].isEmpty()) {
            m_ids[i] = QStringLiteral("node_%1").arg(i);
        }
    }

    m_labels = labels;
    m_labels.resize(nNodes);
    for (int i = 0; i < nNodes; ++i) {
        if (m_labels[i].isEmpty()) {
            m_labels[i] = m_ids[i];
        }
    }
    m_labelLayoutCache.clear();
    m_labelLayoutByIndex.clear();
    m_textVertices.clear();
    ++m_msdfAtlasBuildId;
    {
        std::lock_guard<std::mutex> lock(m_msdfAtlasMutex);
        m_msdfAtlasResultReady = false;
    }
    startMsdfAtlasBuildAsync();

    m_showRadiiBase = radii;
    m_showRadiiBase.resize(nNodes);
    for (int i = 0; i < nNodes; ++i) {
        const float r = m_showRadiiBase[i];
        if (!std::isfinite(r) || r <= 0.0f) {
            m_showRadiiBase[i] = kDefaultNodeRadius;
        }
    }
    updateFactor();

    m_nodeColors = nodeColors;
    m_nodeColors.resize(nNodes);
    for (int i = 0; i < nNodes; ++i) {
        if (!m_nodeColors[i].isValid()) {
            m_nodeColors[i] = m_baseColor;
        }
    }

    m_hoverIndex = -1;
    m_lastHoverIndex = -1;
    m_hoverGlobal = 0.0f;
    m_selectedIndex = -1;
    m_dragging = false;
    rebuildNeighborsFromEdges();
    m_neighborMask.assign(static_cast<size_t>(nNodes), 0);
    m_lastNeighborMask.assign(static_cast<size_t>(nNodes), 0);
    m_lastDimEdges.clear();
    m_lastHighlightEdges.clear();
    m_allowWarmup.store(true, std::memory_order_release);
    switchToCpuSimulation();

    QTimer::singleShot(kFitViewDelayMs, this, [this]() {
        fitViewToContent();
    });
}

void ForceViewRhiWidget::rebuildSimulation()
{
    if (!m_physicsState) {
        m_simulation.reset();
        return;
    }

    m_simulation = std::make_unique<Simulation>(m_physicsState.get());
    m_simulation->addForce(
        "manybody",
        std::make_unique<ManyBodyForce>(m_manyBodyStrength, kManyBodyDistanceLimitSq));
    m_simulation->addForce(
        "link",
        std::make_unique<LinkForce>(m_linkStrength, m_linkDistance));
    m_simulation->addForce(
        "center",
        std::make_unique<CenterForce>(0.0f, 0.0f, m_centerStrength));
    m_simulation->addForce(
        "collision",
        std::make_unique<CollisionForce>(m_collisionRadius, m_collisionStrength));
}

void ForceViewRhiWidget::startSimThread()
{
    if (m_simThreadRunning.load(std::memory_order_acquire)) {
        return;
    }
    m_simThreadRunning.store(true, std::memory_order_release);
    m_simThread = std::thread([this]() { simLoop(); });
}

void ForceViewRhiWidget::stopSimThread()
{
    if (!m_simThreadRunning.load(std::memory_order_acquire)) {
        return;
    }
    m_simThreadRunning.store(false, std::memory_order_release);
    m_simCv.notify_all();
    if (m_simThread.joinable()) {
        m_simThread.join();
    }
}

void ForceViewRhiWidget::simLoop()
{
    bool lastActive = false;
    bool lastWarmup = false;
    auto nextTick = std::chrono::steady_clock::now();
    auto nextRenderUpdate = nextTick;
    const auto interval = std::chrono::milliseconds(kSimTickIntervalMs);
    while (m_simThreadRunning.load(std::memory_order_acquire)) {
        bool didTick = false;
        bool shouldSleep = false;
        bool requestRender = false;
        bool active = false;
        float elapsed = 0.0f;
        float alphaVal = 0.0f;
        std::chrono::milliseconds sleepFor(1);
        {
            std::lock_guard<std::mutex> lock(m_simMutex);
            if (m_simulation && m_physicsState && m_simulation->isActive()) {
                active = true;
                const int n = m_physicsState->nNodes;
                const int warmupTicks = n > 0
                    ? static_cast<int>(n * std::log(static_cast<float>(std::max(2, n))) * 0.2f + 10.0f)
                    : 5;
                const bool allowWarmup = m_allowWarmup.load(std::memory_order_acquire);
                const bool warmup = allowWarmup && m_simulation->tickCount() < warmupTicks;
                if (allowWarmup && !warmup) {
                    m_allowWarmup.store(false, std::memory_order_release);
                }
                if (!lastActive || (lastWarmup && !warmup)) {
                    nextTick = std::chrono::steady_clock::now();
                    nextRenderUpdate = nextTick;
                }
                const auto now = std::chrono::steady_clock::now();
                if (warmup || now >= nextTick) {
                    const double t0 = nowSec();
                    m_simulation->tick();
                    m_physicsState->publishRenderPos();
                    elapsed = static_cast<float>((nowSec() - t0) * 1000.0);
                    alphaVal = m_simulation->alpha();
                    active = m_simulation->isActive();
                    didTick = true;
                    const auto afterTick = std::chrono::steady_clock::now();
                    if (warmup) {
                        if (afterTick >= nextRenderUpdate) {
                            requestRender = true;
                            nextRenderUpdate = afterTick + interval;
                        }
                    } else {
                        requestRender = true;
                        nextTick = afterTick + interval;
                    }
                } else {
                    shouldSleep = true;
                    sleepFor = std::chrono::duration_cast<std::chrono::milliseconds>(nextTick - now);
                    if (sleepFor > interval) {
                        sleepFor = interval;
                    }
                }
                lastWarmup = warmup;
            } else {
                lastWarmup = false;
            }
        }

        m_simActive.store(active, std::memory_order_release);
        if (didTick) {
            emit tickTimeUpdated(elapsed);
            emit alphaUpdated(alphaVal);
            if (requestRender) {
                requestCanvasUpdate();
            }
        }
        if (!active && lastActive) {
            emit simulationStopped();
        }
        lastActive = active;

        if (active && !didTick) {
            if (shouldSleep) {
                std::this_thread::sleep_for(sleepFor);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        } else if (!active) {
            std::unique_lock<std::mutex> lock(m_simMutex);
            m_simCv.wait(lock, [this]() {
                return !m_simThreadRunning.load(std::memory_order_acquire)
                    || (m_simulation && m_simulation->isActive());
            });
        }
    }
}



void ForceViewRhiWidget::applySimulationBackendPreference()
{
    if (m_activeSimulationBackend != QStringLiteral("cpu")) {
        m_activeSimulationBackend = QStringLiteral("cpu");
        emit simulationBackendChanged(QStringLiteral("cpu"));
    }
    if (m_physicsState) {
        switchToCpuSimulation();
    }
}

void ForceViewRhiWidget::switchToCpuSimulation()
{
    stopSimThread();
    bool active = false;
    if (m_activeSimulationBackend != QStringLiteral("cpu")) {
        m_activeSimulationBackend = QStringLiteral("cpu");
        emit simulationBackendChanged(QStringLiteral("cpu"));
    }
    {
        std::lock_guard<std::mutex> lock(m_simMutex);
        rebuildSimulation();
        if (m_simulation) {
            m_simulation->start();
            active = m_simulation->isActive();
        }
    }
    if (active) {
        startSimThread();
        m_simActive.store(true, std::memory_order_release);
        requestCanvasUpdate();
        emit simulationStarted();
    } else {
        m_simActive.store(false, std::memory_order_release);
    }
}

void ForceViewRhiWidget::requestCanvasUpdate()
{
    QMetaObject::invokeMethod(this, [this]() {
        if (m_canvas) {
            m_canvas->update();
        }
    }, Qt::QueuedConnection);
}

void ForceViewRhiWidget::pauseSimulation()
{
    {
        std::lock_guard<std::mutex> lock(m_simMutex);
        if (m_simulation) {
            m_simulation->pause();
        }
    }
    m_simActive.store(false, std::memory_order_release);
    emit simulationStopped();
}

void ForceViewRhiWidget::resumeSimulation()
{
    bool active = false;
    {
        std::lock_guard<std::mutex> lock(m_simMutex);
        if (m_simulation) {
            m_simulation->resume();
            active = m_simulation->isActive();
        }
    }
    if (active) {
        startSimThread();
        m_simCv.notify_all();
        requestCanvasUpdate();
        emit simulationStarted();
    }
}

void ForceViewRhiWidget::restartSimulation()
{
    m_allowWarmup.store(false, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(m_simMutex);
        if (m_simulation) {
            m_simulation->restart();
        }
    }
    startSimThread();
    m_simCv.notify_all();
    requestCanvasUpdate();
    emit alphaUpdated(1.0f);
    emit simulationStarted();
}

void ForceViewRhiWidget::setSimulationBackendMode(const QString& mode)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, mode]() {
            setSimulationBackendMode(mode);
        }, Qt::QueuedConnection);
        return;
    }
    const QString normalized = mode.trimmed().toLower();
    if (normalized != QStringLiteral("cpu")
        && normalized != QStringLiteral("auto")) {
        qWarning() << "ForceViewRhiWidget: unsupported simulation backend mode:" << mode;
        return;
    }
    const bool modeChanged = m_simulationBackendMode != normalized;
    m_simulationBackendMode = normalized;
    if (modeChanged) {
        emit simulationBackendModeChanged(normalized);
    }
    if (!modeChanged && m_activeSimulationBackend == QStringLiteral("cpu")) {
        return;
    }
    applySimulationBackendPreference();
}

QString ForceViewRhiWidget::simulationBackendMode() const
{
    return m_simulationBackendMode;
}

QString ForceViewRhiWidget::activeSimulationBackend() const
{
    return m_activeSimulationBackend;
}

void ForceViewRhiWidget::setManyBodyStrength(float value)
{
    m_manyBodyStrength = value;
    {
        std::lock_guard<std::mutex> lock(m_simMutex);
        if (m_simulation) {
            if (auto* f = dynamic_cast<ManyBodyForce*>(m_simulation->getForce("manybody"))) {
                f->setStrength(value);
            }
        }
    }
    restartSimulation();
}

void ForceViewRhiWidget::setCenterStrength(float value)
{
    m_centerStrength = value;
    {
        std::lock_guard<std::mutex> lock(m_simMutex);
        if (m_simulation) {
            if (auto* f = dynamic_cast<CenterForce*>(m_simulation->getForce("center"))) {
                f->setStrength(value);
            }
        }
    }
    restartSimulation();
}

void ForceViewRhiWidget::setLinkStrength(float value)
{
    m_linkStrength = value;
    {
        std::lock_guard<std::mutex> lock(m_simMutex);
        if (m_simulation) {
            if (auto* f = dynamic_cast<LinkForce*>(m_simulation->getForce("link"))) {
                f->setK(value);
            }
        }
    }
    restartSimulation();
}

void ForceViewRhiWidget::setLinkDistance(float value)
{
    m_linkDistance = value;
    {
        std::lock_guard<std::mutex> lock(m_simMutex);
        if (m_simulation) {
            if (auto* f = dynamic_cast<LinkForce*>(m_simulation->getForce("link"))) {
                f->setDistance(value);
            }
        }
    }
    restartSimulation();
}

void ForceViewRhiWidget::setCollisionRadius(float value)
{
    m_collisionRadius = value;
    {
        std::lock_guard<std::mutex> lock(m_simMutex);
        if (m_simulation) {
            if (auto* f = dynamic_cast<CollisionForce*>(m_simulation->getForce("collision"))) {
                f->setRadius(value);
            }
        }
    }
    restartSimulation();
}

void ForceViewRhiWidget::setCollisionStrength(float value)
{
    m_collisionStrength = value;
    {
        std::lock_guard<std::mutex> lock(m_simMutex);
        if (m_simulation) {
            if (auto* f = dynamic_cast<CollisionForce*>(m_simulation->getForce("collision"))) {
                f->setStrength(value);
            }
        }
    }
    restartSimulation();
}

void ForceViewRhiWidget::updateFactor()
{
    m_sideWidth = m_sideWidthBase * m_sideWidthFactor;
    const int n = m_showRadiiBase.size();
    m_showRadii.resize(n);
    for (int i = 0; i < n; ++i) {
        m_showRadii[i] = m_showRadiiBase[i] * m_radiusFactor;
    }
}

void ForceViewRhiWidget::setRadiusFactor(float f)
{
    m_radiusFactor = f;
    updateFactor();
    update();
}

void ForceViewRhiWidget::setSideWidthFactor(float f)
{
    m_sideWidthFactor = f;
    updateFactor();
    update();
}

void ForceViewRhiWidget::setTextThresholdFactor(float f)
{
    m_textThresholdFactor = f;
    update();
}

void ForceViewRhiWidget::setNodeColors(const QVector<QColor>& colors)
{
    if (!m_physicsState || colors.size() != m_physicsState->nNodes) {
        return;
    }
    m_nodeColors = colors;
    update();
}

void ForceViewRhiWidget::setArrowScale(float f)
{
    m_arrowScale = std::max(0.2f, std::min(f, 5.0f));
    update();
}

void ForceViewRhiWidget::setArrowEnabled(bool enabled)
{
    m_arrowEnabled = enabled;
    update();
}

void ForceViewRhiWidget::setNeighborDepth(int depth)
{
    m_neighborDepth = qBound(1, depth, 5);
    if (m_hoverIndex >= 0) {
        updateNeighborMaskForHover(m_hoverIndex);
    }
    update();
}

void ForceViewRhiWidget::setBackgroundColor(const QColor& color)
{
    if (color.isValid()) {
        m_backgroundColor = color;
        update();
    }
}

void ForceViewRhiWidget::setEdgeColor(const QColor& c) { m_edgeColor = c; update(); }
QColor ForceViewRhiWidget::edgeColor() const { return m_edgeColor; }
void ForceViewRhiWidget::setEdgeDimColor(const QColor& c) { m_edgeDimColor = c; update(); }
QColor ForceViewRhiWidget::edgeDimColor() const { return m_edgeDimColor; }
void ForceViewRhiWidget::setBaseColor(const QColor& c) { m_baseColor = c; update(); }
QColor ForceViewRhiWidget::baseColor() const { return m_baseColor; }
void ForceViewRhiWidget::setDimColor(const QColor& c) { m_dimColor = c; update(); }
QColor ForceViewRhiWidget::dimColor() const { return m_dimColor; }
void ForceViewRhiWidget::setHoverColor(const QColor& c) { m_hoverColor = c; update(); }
QColor ForceViewRhiWidget::hoverColor() const { return m_hoverColor; }
void ForceViewRhiWidget::setTextColor(const QColor& c) { m_textColor = c; update(); }
QColor ForceViewRhiWidget::textColor() const { return m_textColor; }
void ForceViewRhiWidget::setTextDimColor(const QColor& c) { m_textDimColor = c; update(); }
QColor ForceViewRhiWidget::textDimColor() const { return m_textDimColor; }

void ForceViewRhiWidget::setFontPath(const QString& path)
{
    if (m_fontPath == path) {
        return;
    }
    m_fontPath = path;
    if (!m_fontAtlas) {
        m_fontAtlas = std::make_unique<MsdfFontAtlas>();
    }
    m_fontAtlas->initialize(makeFontConfig());
    m_labelLayoutCache.clear();
    m_labelLayoutByIndex.clear();
    m_textVertices.clear();
    m_textAtlasUploadedGeneration = -1;
    ++m_msdfAtlasBuildId;
    {
        std::lock_guard<std::mutex> lock(m_msdfAtlasMutex);
        m_msdfAtlasResultReady = false;
    }
    if (!m_labels.isEmpty()) {
        startMsdfAtlasBuildAsync();
    }
    update();
}

QString ForceViewRhiWidget::fontPath() const
{
    return m_fontPath;
}

QStringList ForceViewRhiWidget::getNodeIds() const
{
    return m_ids;
}

QPointF ForceViewRhiWidget::getNodePosition(const QString& nodeId) const
{
    if (!m_physicsState || nodeId.isEmpty()) {
        return {};
    }
    for (int i = 0; i < m_ids.size(); ++i) {
        if (m_ids[i] == nodeId) {
            std::vector<float> snapshot;
            m_physicsState->copyRenderPos(snapshot);
            if (snapshot.size() < static_cast<size_t>(2 * m_physicsState->nNodes)) {
                return {};
            }
            const float* pos = snapshot.data();
            return QPointF(pos[2 * i], pos[2 * i + 1]);
        }
    }
    return {};
}

QRectF ForceViewRhiWidget::getContentRect() const
{
    if (!m_physicsState || m_physicsState->nNodes <= 0) {
        return {};
    }
    const int n = m_physicsState->nNodes;
    std::vector<float> snapshot;
    m_physicsState->copyRenderPos(snapshot);
    if (snapshot.size() < static_cast<size_t>(2 * n)) {
        return {};
    }
    const float* pos = snapshot.data();
    auto radiusAt = [this](int i) {
        return (i >= 0 && i < m_showRadii.size()) ? m_showRadii[i] : kDefaultNodeRadius;
    };

    float r0 = radiusAt(0);
    float minX = pos[0] - r0;
    float maxX = pos[0] + r0;
    float minY = pos[1] - r0;
    float maxY = pos[1] + r0;
    for (int i = 1; i < n; ++i) {
        const float r = radiusAt(i);
        const float x = pos[2 * i];
        const float y = pos[2 * i + 1];
        minX = std::min(minX, x - r);
        maxX = std::max(maxX, x + r);
        minY = std::min(minY, y - r);
        maxY = std::max(maxY, y + r);
    }
    const float w = maxX - minX;
    const float h = maxY - minY;
    const float mx = std::max(w * kContentPaddingRatio, kContentPaddingAbs);
    const float my = std::max(h * kContentPaddingRatio, kContentPaddingAbs);
    return QRectF(minX - mx, minY - my, w + 2 * mx, h + 2 * my);
}

void ForceViewRhiWidget::fitViewToContent()
{
    if (!m_physicsState || m_physicsState->nNodes <= 0) {
        return;
    }
    const QRectF r = getContentRect();
    if (r.isEmpty() || m_viewportW <= 0 || m_viewportH <= 0) {
        return;
    }
    const int n = m_physicsState->nNodes;
    float margin = kFitViewScaleMargin;
    if (n <= 2) {
        margin = 0.65f;
    } else if (n <= 10) {
        margin = 0.75f;
    } else if (n <= 50) {
        margin = 0.85f;
    }
    float z = std::min(
        static_cast<float>(m_viewportW) / static_cast<float>(r.width()),
        static_cast<float>(m_viewportH) / static_cast<float>(r.height())) * margin;

    float fitZoomUpper = kFitViewZoomMax;
    if (n <= 2) {
        fitZoomUpper = std::min(fitZoomUpper, 1.2f);
    } else if (n <= 10) {
        fitZoomUpper = std::min(fitZoomUpper, 1.8f);
    } else if (n <= 50) {
        fitZoomUpper = std::min(fitZoomUpper, 2.8f);
    }
    if (n <= 50) {
        float maxNodeRadius = 0.0f;
        for (float radius : m_showRadii) {
            if (std::isfinite(radius)) {
                maxNodeRadius = std::max(maxNodeRadius, radius);
            }
        }
        if (maxNodeRadius > 0.0f) {
            fitZoomUpper = std::min(fitZoomUpper, kFitViewMaxNodeRadiusPx / maxNodeRadius);
        }
    }
    m_zoom = std::max(kFitViewZoomMin, std::min(fitZoomUpper, z));
    m_panX = static_cast<float>(r.center().x());
    m_panY = static_cast<float>(r.center().y());
    emit scaleChanged(m_zoom);
    update();
}

void ForceViewRhiWidget::rebuildNeighborsFromEdges()
{
    const int n = m_physicsState ? m_physicsState->nNodes : 0;
    m_neighbors.assign(static_cast<size_t>(std::max(0, n)), {});
    if (!m_physicsState || n <= 0) {
        return;
    }
    const int eCount = m_physicsState->edgeCount();
    const int* edges = m_physicsState->edges.data();
    for (int e = 0; e < eCount; ++e) {
        const int s = edges[2 * e];
        const int d = edges[2 * e + 1];
        if (s >= 0 && s < n && d >= 0 && d < n && s != d) {
            m_neighbors[static_cast<size_t>(s)].push_back(d);
            m_neighbors[static_cast<size_t>(d)].push_back(s);
        }
    }
}

void ForceViewRhiWidget::updateNeighborMaskForHover(int hoverIndex)
{
    const int n = m_physicsState ? m_physicsState->nNodes : 0;
    m_neighborMask.assign(static_cast<size_t>(std::max(0, n)), 0);
    if (hoverIndex < 0 || hoverIndex >= n) {
        return;
    }
    std::vector<int> frontier = {hoverIndex};
    std::vector<uint8_t> visited(static_cast<size_t>(n), 0);
    visited[static_cast<size_t>(hoverIndex)] = 1;
    for (int level = 0; level < m_neighborDepth && !frontier.empty(); ++level) {
        std::vector<int> next;
        for (int u : frontier) {
            if (u < 0 || u >= static_cast<int>(m_neighbors.size())) {
                continue;
            }
            for (int nb : m_neighbors[static_cast<size_t>(u)]) {
                if (nb >= 0 && nb < n && !visited[static_cast<size_t>(nb)]) {
                    visited[static_cast<size_t>(nb)] = 1;
                    m_neighborMask[static_cast<size_t>(nb)] = 1;
                    next.push_back(nb);
                }
            }
        }
        frontier = std::move(next);
    }
    m_lastHoverIndex = hoverIndex;
    m_lastNeighborMask = m_neighborMask;
}

void ForceViewRhiWidget::advanceHover()
{
    const float previous = m_hoverGlobal;
    if (m_dragging) {
        m_hoverGlobal = 1.0f;
    } else {
        const float target = (m_hoverIndex >= 0) ? 1.0f : 0.0f;
        if (target > m_hoverGlobal) {
            m_hoverGlobal = std::min(1.0f, m_hoverGlobal + m_hoverStep);
        } else if (target < m_hoverGlobal) {
            m_hoverGlobal = std::max(0.0f, m_hoverGlobal - m_hoverStep);
        }
    }

    if (std::abs(m_hoverGlobal - previous) > 1e-4f) {
        requestCanvasUpdate();
    }
}

void ForceViewRhiWidget::screenToScene(float sx, float sy, float& outX, float& outY) const
{
    outX = m_panX + (sx - m_viewportW * 0.5f) / std::max(m_zoom, 1e-6f);
    outY = m_panY + (sy - m_viewportH * 0.5f) / std::max(m_zoom, 1e-6f);
}

int ForceViewRhiWidget::pickNodeAt(float sceneX, float sceneY) const
{
    if (!m_physicsState) {
        return -1;
    }
    const int n = m_physicsState->nNodes;
    std::vector<float> snapshot;
    m_physicsState->copyRenderPos(snapshot);
    if (snapshot.size() < static_cast<size_t>(2 * n)) {
        return -1;
    }
    const float* pos = snapshot.data();
    for (int i = n - 1; i >= 0; --i) {
        const float r = (i < m_showRadii.size()) ? m_showRadii[i] : kDefaultNodeRadius;
        const float dx = sceneX - pos[2 * i];
        const float dy = sceneY - pos[2 * i + 1];
        const float pickRadius = r;
        if (dx * dx + dy * dy <= pickRadius * pickRadius) {
            return i;
        }
    }
    return -1;
}

void ForceViewRhiWidget::wheelEvent(QWheelEvent* event)
{
    const QPointF p = event->position();
    float beforeX = 0.0f;
    float beforeY = 0.0f;
    screenToScene(static_cast<float>(p.x()), static_cast<float>(p.y()), beforeX, beforeY);
    const float factor = event->angleDelta().y() > 0 ? kZoomWheelFactor : 1.0f / kZoomWheelFactor;
    m_zoom = std::max(kZoomMin, std::min(kZoomMax, m_zoom * factor));
    float afterX = 0.0f;
    float afterY = 0.0f;
    screenToScene(static_cast<float>(p.x()), static_cast<float>(p.y()), afterX, afterY);
    m_panX += beforeX - afterX;
    m_panY += beforeY - afterY;
    emit scaleChanged(m_zoom);
    update();
    event->accept();
}

void ForceViewRhiWidget::mousePressEvent(QMouseEvent* event)
{
    if (!m_physicsState || m_physicsState->nNodes == 0) {
        QWidget::mousePressEvent(event);
        return;
    }

    float sx = 0.0f;
    float sy = 0.0f;
    screenToScene(static_cast<float>(event->position().x()),
                  static_cast<float>(event->position().y()),
                  sx,
                  sy);

    const int idx = pickNodeAt(sx, sy);
    if (idx >= 0) {
        m_selectedIndex = idx;
        m_pressedButton = event->button();
        m_hoverIndex = idx;
        updateNeighborMaskForHover(idx);
        m_lastHoverIndex = idx;
        std::vector<float> snapshot;
        m_physicsState->copyRenderPos(snapshot);
        const float* pos = snapshot.data();
        m_dragOffsetX = pos[2 * idx] - sx;
        m_dragOffsetY = pos[2 * idx + 1] - sy;
        setCursor(Qt::PointingHandCursor);
        emit nodePressed(m_ids.value(idx));
        update();
        event->accept();
        return;
    }

    m_selectedIndex = -1;
    m_pressedButton = Qt::NoButton;
    m_isPanning = true;
    m_panStartX = static_cast<float>(event->position().x());
    m_panStartY = static_cast<float>(event->position().y());
    m_panStartPanX = m_panX;
    m_panStartPanY = m_panY;
    setCursor(Qt::ClosedHandCursor);
    event->accept();
}

void ForceViewRhiWidget::mouseMoveEvent(QMouseEvent* event)
{
    const float ex = static_cast<float>(event->position().x());
    const float ey = static_cast<float>(event->position().y());
    float sx = 0.0f;
    float sy = 0.0f;
    screenToScene(ex, ey, sx, sy);

    if (m_selectedIndex >= 0 && m_physicsState) {
        const float x = sx + m_dragOffsetX;
        const float y = sy + m_dragOffsetY;
        {
            std::lock_guard<std::mutex> lock(m_simMutex);
            if (m_physicsState && m_selectedIndex < m_physicsState->nNodes) {
                m_physicsState->setDragPos(m_selectedIndex, x, y);
                m_physicsState->pos[2 * m_selectedIndex] = x;
                m_physicsState->pos[2 * m_selectedIndex + 1] = y;
                m_physicsState->publishRenderPos();
            }
        }
        if (!m_dragging) {
            setDragging(m_selectedIndex, true);
            m_dragging = true;
            setCursor(Qt::PointingHandCursor);
        }
        emit nodeDragged(m_ids.value(m_selectedIndex));
        emit nodeHoveredWithInfo(
            m_ids.value(m_selectedIndex),
            x,
            y,
            (m_selectedIndex < m_showRadii.size()) ? m_showRadii[m_selectedIndex] : kDefaultNodeRadius,
            m_zoom,
            true);
        update();
        event->accept();
        return;
    }

    if (m_isPanning) {
        m_panX = m_panStartPanX - (ex - m_panStartX) / std::max(m_zoom, 1e-6f);
        m_panY = m_panStartPanY - (ey - m_panStartY) / std::max(m_zoom, 1e-6f);
        setCursor(Qt::ClosedHandCursor);
        update();
        event->accept();
        return;
    }

    const int idx = pickNodeAt(sx, sy);
    if (idx != m_hoverIndex) {
        if (idx < 0 && m_hoverIndex >= 0) {
            m_lastHoverIndex = m_hoverIndex;
            m_lastNeighborMask = m_neighborMask;
        }
        m_hoverIndex = idx;
        updateNeighborMaskForHover(idx);
        if (idx >= 0) {
            m_lastHoverIndex = idx;
        }
        emit nodeHovered(idx >= 0 ? m_ids.value(idx) : QString());
        if (idx >= 0 && m_physicsState) {
            std::vector<float> snapshot;
            m_physicsState->copyRenderPos(snapshot);
            const float* pos = snapshot.data();
            emit nodeHoveredWithInfo(
                m_ids.value(idx),
                pos[2 * idx],
                pos[2 * idx + 1],
                (idx < m_showRadii.size()) ? m_showRadii[idx] : kDefaultNodeRadius,
                m_zoom,
                false);
        } else {
            emit nodeHoveredWithInfo(QString(), 0.0f, 0.0f, 0.0f, m_zoom, false);
        }
        update();
    }
    setCursor(m_hoverIndex >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    event->accept();
}

void ForceViewRhiWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_selectedIndex >= 0) {
        const int idx = m_selectedIndex;
        const bool wasDragging = m_dragging;
        if (wasDragging) {
            setDragging(idx, false);
        } else if (m_pressedButton == Qt::LeftButton) {
            emit nodeLeftClicked(m_ids.value(idx));
        } else if (m_pressedButton == Qt::RightButton) {
            emit nodeRightClicked(m_ids.value(idx));
        }
        m_dragging = false;
        m_selectedIndex = -1;
        m_pressedButton = Qt::NoButton;
        emit nodeReleased(m_ids.value(idx));
        setCursor(Qt::ArrowCursor);
        update();
        event->accept();
        return;
    }
    m_isPanning = false;
    m_pressedButton = Qt::NoButton;
    setCursor(Qt::ArrowCursor);
    event->accept();
}

void ForceViewRhiWidget::leaveEvent(QEvent* event)
{
    if (m_hoverIndex >= 0) {
        m_lastHoverIndex = m_hoverIndex;
        m_lastNeighborMask = m_neighborMask;
    }
    m_hoverIndex = -1;
    m_neighborMask.assign(m_neighborMask.size(), 0);
    emit nodeHovered(QString());
    emit nodeHoveredWithInfo(QString(), 0.0f, 0.0f, 0.0f, 0.0f, false);
    setCursor(Qt::ArrowCursor);
    update();
    QWidget::leaveEvent(event);
}

void ForceViewRhiWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_viewportW = std::max(1, event->size().width());
    m_viewportH = std::max(1, event->size().height());
    m_pipelineDirty = true;
}

void ForceViewRhiWidget::setDragging(int nodeId, bool dragging)
{
    bool needRestart = false;
    {
        std::lock_guard<std::mutex> lock(m_simMutex);
        if (!m_physicsState || nodeId < 0 || nodeId >= m_physicsState->nNodes) {
            return;
        }
        m_physicsState->dragging[static_cast<size_t>(nodeId)] = dragging ? 1 : 0;
        if (dragging) {
            std::vector<float> snapshot;
            m_physicsState->copyRenderPos(snapshot);
            const float* pos = snapshot.data();
            m_physicsState->setDragPos(nodeId, pos[2 * nodeId], pos[2 * nodeId + 1]);
            needRestart = true;
        }
    }
    if (needRestart) {
        restartSimulation();
    }
}

QColor ForceViewRhiWidget::mixColor(const QColor& c1, const QColor& c2, float t) const
{
    if (t <= 0.0f) {
        return c1;
    }
    if (t >= 1.0f) {
        return c2;
    }
    return QColor(
        c1.red() + static_cast<int>((c2.red() - c1.red()) * t),
        c1.green() + static_cast<int>((c2.green() - c1.green()) * t),
        c1.blue() + static_cast<int>((c2.blue() - c1.blue()) * t),
        c1.alpha() + static_cast<int>((c2.alpha() - c1.alpha()) * t));
}

void ForceViewRhiWidget::startMsdfAtlasBuildAsync()
{
    if (!m_fontAtlas || !m_fontAtlas->isReady() || m_labels.isEmpty()) {
        return;
    }
    if (m_msdfAtlasThreadRunning.load(std::memory_order_acquire)) {
        return;
    }
    if (m_msdfAtlasThread.joinable()) {
        m_msdfAtlasThread.join();
    }

    const MsdfFontAtlas::Config cfg = makeFontConfig();
    const QStringList labels = m_labels;
    const int buildId = m_msdfAtlasBuildId;

    m_msdfAtlasThreadRunning.store(true, std::memory_order_release);
    m_msdfAtlasThread = std::thread([this, cfg, labels, buildId]() {
        MsdfAtlasBuildResult result;
        result.buildId = buildId;
        QString error;
        MsdfFontAtlas::AtlasData data;
        if (MsdfFontAtlas::BuildAtlasStandalone(cfg, labels, data, &error)) {
            result.success = true;
            result.data = std::move(data);
        } else {
            result.success = false;
            result.error = error;
        }
        {
            std::lock_guard<std::mutex> lock(m_msdfAtlasMutex);
            m_msdfAtlasResult = std::move(result);
            m_msdfAtlasThreadRunning.store(false, std::memory_order_release);
            m_msdfAtlasResultReady = true;
        }
        QMetaObject::invokeMethod(this, [this]() {
            requestCanvasUpdate();
        }, Qt::QueuedConnection);
    });
}

void ForceViewRhiWidget::applyMsdfAtlasResultIfReady()
{
    MsdfAtlasBuildResult result;
    {
        std::lock_guard<std::mutex> lock(m_msdfAtlasMutex);
        if (!m_msdfAtlasResultReady) {
            return;
        }
        result = std::move(m_msdfAtlasResult);
        m_msdfAtlasResultReady = false;
    }

    if (result.buildId != m_msdfAtlasBuildId) {
        startMsdfAtlasBuildAsync();
        return;
    }
    if (result.success && m_fontAtlas) {
        m_fontAtlas->applyAtlasData(std::move(result.data));
        rebuildLabelLayoutCache();
        m_textAtlasUploadedGeneration = -1;
    }
}

void ForceViewRhiWidget::invalidateTextLayoutForLabels()
{
    m_labelLayoutCache.clear();
    m_labelLayoutByIndex.clear();
    m_textVertices.clear();
    m_textDimFirst = 0;
    m_textDimCount = 0;
    m_textRestFirst = 0;
    m_textRestCount = 0;
    m_textHoverFirst = 0;
    m_textHoverCount = 0;
    {
        std::lock_guard<std::mutex> lock(m_msdfAtlasMutex);
        m_msdfAtlasResultReady = false;
    }
    ++m_msdfAtlasBuildId;
    startMsdfAtlasBuildAsync();
}

void ForceViewRhiWidget::rebuildMsdfAtlas()
{
    if (!m_fontAtlas || !m_fontAtlas->isReady()) {
        return;
    }
    m_fontAtlas->buildForLabels(m_labels);
    rebuildLabelLayoutCache();
    m_textAtlasUploadedGeneration = -1;
}

void ForceViewRhiWidget::rebuildLabelLayoutCache()
{
    m_labelLayoutCache.clear();
    m_labelLayoutByIndex.clear();
    if (!m_fontAtlas || !m_fontAtlas->isReady()) {
        return;
    }

    for (int i = 0; i < m_labels.size(); ++i) {
        const std::string key = m_labels[i].toStdString();
        if (m_labelLayoutCache.count(key)) {
            continue;
        }
        const QList<uint> cps = m_labels[i].toUcs4();
        LabelLayoutEntry entry;
        float cursorX = 0.0f;
        uint32_t prev = 0;
        bool hasPrev = false;
        for (const uint cpQ : cps) {
            const uint32_t cp = static_cast<uint32_t>(cpQ);
            const MsdfFontAtlas::GlyphInfo* g = m_fontAtlas->findGlyph(cp);
            if (!g) {
                continue;
            }
            if (hasPrev) {
                cursorX += m_fontAtlas->kerning(prev, cp);
            }
            if (g->drawable) {
                GlyphQuad q;
                q.x0 = cursorX + g->planeLeft;
                q.y0 = g->planeBottom;
                q.x1 = cursorX + g->planeRight;
                q.y1 = g->planeTop;
                q.u0 = g->u0;
                q.v0 = g->v0;
                q.u1 = g->u1;
                q.v1 = g->v1;
                entry.quads.push_back(q);
            }
            cursorX += g->advance;
            prev = cp;
            hasPrev = true;
        }
        entry.totalWidth = cursorX;
        m_labelLayoutCache[key] = std::move(entry);
    }

    m_labelLayoutByIndex.resize(m_labels.size());
    for (int i = 0; i < m_labels.size(); ++i) {
        auto it = m_labelLayoutCache.find(m_labels[i].toStdString());
        m_labelLayoutByIndex[static_cast<size_t>(i)] =
            (it != m_labelLayoutCache.end()) ? &it->second : nullptr;
    }
}

QColor ForceViewRhiWidget::nodeColorFor(int i) const
{
    if (i >= 0 && i < m_nodeColors.size() && m_nodeColors[i].isValid()) {
        return m_nodeColors[i];
    }
    return m_baseColor;
}

void ForceViewRhiWidget::appendLineQuad(
    std::vector<Vertex>& out,
    int s,
    int d,
    const QColor& color,
    const float* pos)
{
    if (!pos) {
        return;
    }
    const float x0 = pos[2 * s];
    const float y0 = pos[2 * s + 1];
    const float x1 = pos[2 * d];
    const float y1 = pos[2 * d + 1];
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len <= kLineMinLength) {
        return;
    }
    const float r0 = (s < m_showRadii.size()) ? m_showRadii[s] : kDefaultNodeRadius;
    const float r1 = (d < m_showRadii.size()) ? m_showRadii[d] : kDefaultNodeRadius;
    if (len <= r0 + r1 + kLineMinLength) {
        return;
    }
    const float dirX = dx / len;
    const float dirY = dy / len;
    const float ax = x0 + dirX * r0;
    const float ay = y0 + dirY * r0;
    const float bx = x1 - dirX * r1;
    const float by = y1 - dirY * r1;
    const float segX = bx - ax;
    const float segY = by - ay;
    const float segLen = std::sqrt(segX * segX + segY * segY);
    if (segLen <= kLineMinLength) {
        return;
    }
    const float nx = -segY / segLen;
    const float ny = segX / segLen;
    const float halfWidth = std::max(0.5f, 0.5f * m_sideWidth);
    const float ox = nx * halfWidth;
    const float oy = ny * halfWidth;
    const float cr = color.redF();
    const float cg = color.greenF();
    const float cb = color.blueF();
    const float ca = color.alphaF();
    const Vertex v0{ax + ox, ay + oy, 1.0f, 0.0f, 1.0f, cr, cg, cb, ca};
    const Vertex v1{ax - ox, ay - oy, -1.0f, 0.0f, 1.0f, cr, cg, cb, ca};
    const Vertex v2{bx - ox, by - oy, -1.0f, 0.0f, 1.0f, cr, cg, cb, ca};
    const Vertex v3{bx + ox, by + oy, 1.0f, 0.0f, 1.0f, cr, cg, cb, ca};
    out.push_back(v0);
    out.push_back(v1);
    out.push_back(v2);
    out.push_back(v0);
    out.push_back(v2);
    out.push_back(v3);
}

void ForceViewRhiWidget::appendArrow(
    std::vector<Vertex>& out,
    int s,
    int d,
    const QColor& color,
    const float* pos)
{
    if (!m_arrowEnabled || !pos) {
        return;
    }
    const float x0 = pos[2 * s];
    const float y0 = pos[2 * s + 1];
    const float x1 = pos[2 * d];
    const float y1 = pos[2 * d + 1];
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len <= kLineMinLength) {
        return;
    }
    const float r0 = (s < m_showRadii.size()) ? m_showRadii[s] : kDefaultNodeRadius;
    const float r1 = (d < m_showRadii.size()) ? m_showRadii[d] : kDefaultNodeRadius;
    if (len <= r0 + r1 + kLineMinLength) {
        return;
    }
    const float dirX = dx / len;
    const float dirY = dy / len;
    const float tipX = x1 - dirX * r1;
    const float tipY = y1 - dirY * r1;
    const float arrowLen = 4.0f * m_arrowScale;
    const float arrowHalfWidth = 1.5f * m_arrowScale;
    const float nx = -dirY;
    const float ny = dirX;
    const float baseX = tipX - dirX * arrowLen;
    const float baseY = tipY - dirY * arrowLen;
    const float cr = color.redF();
    const float cg = color.greenF();
    const float cb = color.blueF();
    const float ca = color.alphaF();
    out.push_back(Vertex{tipX, tipY, 0.0f, 0.0f, 0.0f, cr, cg, cb, ca});
    out.push_back(Vertex{
        baseX + nx * arrowHalfWidth,
        baseY + ny * arrowHalfWidth,
        0.0f,
        0.0f,
        0.0f,
        cr,
        cg,
        cb,
        ca});
    out.push_back(Vertex{
        baseX - nx * arrowHalfWidth,
        baseY - ny * arrowHalfWidth,
        0.0f,
        0.0f,
        0.0f,
        cr,
        cg,
        cb,
        ca});
}

void ForceViewRhiWidget::appendCircle(
    std::vector<Vertex>& out,
    float x,
    float y,
    float radius,
    const QColor& color)
{
    const float cr = color.redF();
    const float cg = color.greenF();
    const float cb = color.blueF();
    const float ca = color.alphaF();
    constexpr float paddingScale = 1.2f;
    auto makeVertex = [=](float localX, float localY) {
        return Vertex{
            x + localX * radius,
            y + localY * radius,
            localX,
            localY,
            2.0f,
            cr,
            cg,
            cb,
            ca};
    };

    const Vertex v0 = makeVertex(-paddingScale, -paddingScale);
    const Vertex v1 = makeVertex( paddingScale, -paddingScale);
    const Vertex v2 = makeVertex( paddingScale,  paddingScale);
    const Vertex v3 = makeVertex(-paddingScale,  paddingScale);
    out.push_back(v0);
    out.push_back(v1);
    out.push_back(v2);
    out.push_back(v0);
    out.push_back(v2);
    out.push_back(v3);
}

void ForceViewRhiWidget::appendTextLabel(
    std::vector<TextVertex>& out,
    int index,
    const QColor& color,
    float alpha,
    float fontScale,
    const float* pos)
{
    if (!pos || index < 0 || !m_physicsState || index >= m_physicsState->nNodes) {
        return;
    }
    if (index >= static_cast<int>(m_labelLayoutByIndex.size())) {
        return;
    }
    const LabelLayoutEntry* layout = m_labelLayoutByIndex[static_cast<size_t>(index)];
    if (!layout || layout->quads.empty()) {
        return;
    }

    const float nodeX = pos[2 * index];
    const float nodeY = pos[2 * index + 1];
    const float radius = (index < m_showRadii.size()) ? m_showRadii[index] : kDefaultNodeRadius;
    const float fontSize = m_msdfFontSize * fontScale;
    const float descent = m_fontAtlas ? static_cast<float>(m_fontAtlas->descender()) : 0.0f;
    const float labelW = layout->totalWidth * fontSize;
    float baseX = nodeX - labelW * 0.5f;
    float baseY = nodeY + radius - descent * fontSize + fontSize * 1.25f;

    const float dpr = m_canvas ? static_cast<float>(m_canvas->devicePixelRatioF())
                               : static_cast<float>(devicePixelRatioF());
    const float invZoomDpr = 1.0f / (std::max(m_zoom, 1e-6f) * dpr);
    baseX = std::round(baseX * m_zoom * dpr) * invZoomDpr;
    baseY = std::round(baseY * m_zoom * dpr) * invZoomDpr;

    const float cr = color.redF();
    const float cg = color.greenF();
    const float cb = color.blueF();
    const float ca = color.alphaF() * alpha;
    auto push = [&out, cr, cg, cb, ca, fontScale](float x, float y, float u, float v) {
        out.push_back(TextVertex{x, y, u, v, cr, cg, cb, ca, fontScale});
    };

    for (const GlyphQuad& q : layout->quads) {
        const float x0 = baseX + q.x0 * fontSize;
        const float y0 = baseY - q.y1 * fontSize;
        const float x1 = baseX + q.x1 * fontSize;
        const float y1 = baseY - q.y0 * fontSize;

        push(x0, y0, q.u0, q.v1);
        push(x0, y1, q.u0, q.v0);
        push(x1, y1, q.u1, q.v0);
        push(x0, y0, q.u0, q.v1);
        push(x1, y1, q.u1, q.v0);
        push(x1, y0, q.u1, q.v1);
    }
}

void ForceViewRhiWidget::prepareTextVertices()
{
    m_textVertices.clear();
    m_textDimFirst = 0;
    m_textDimCount = 0;
    m_textRestFirst = 0;
    m_textRestCount = 0;
    m_textHoverFirst = 0;
    m_textHoverCount = 0;
    if (!m_physicsState || m_physicsState->nNodes <= 0 || !m_fontAtlas || !m_fontAtlas->isReady()) {
        return;
    }
    if (m_labelLayoutByIndex.size() != static_cast<size_t>(m_labels.size())
        || m_labels.size() != m_physicsState->nNodes) {
        return;
    }

    const float thresholdOff = std::max(0.001f, kTextThresholdBase * m_textThresholdFactor);
    if (m_zoom <= thresholdOff) {
        return;
    }
    const float thresholdShow = std::max(thresholdOff + 0.001f, thresholdOff * kTextThresholdShowMul);
    float baseAlpha = 1.0f;
    if (m_zoom < thresholdShow) {
        baseAlpha = (m_zoom - thresholdOff) / (thresholdShow - thresholdOff);
    }

    const int n = m_physicsState->nNodes;
    if (m_renderPosSnapshot.size() < static_cast<size_t>(2 * m_physicsState->nNodes)) {
        return;
    }
    const float* pos = m_renderPosSnapshot.data();

    const float halfW = (m_viewportW / 2.0f) / std::max(m_zoom, 1e-6f);
    const float halfH = (m_viewportH / 2.0f) / std::max(m_zoom, 1e-6f);
    const float left = m_panX - halfW - kViewPadding;
    const float right = m_panX + halfW + kViewPadding;
    const float top = m_panY - halfH - kViewPadding;
    const float bottom = m_panY + halfH + kViewPadding;

    std::vector<TextVertex> dimText;
    std::vector<TextVertex> restText;
    std::vector<TextVertex> hoverText;
    dimText.reserve(static_cast<size_t>(n) * 18);
    restText.reserve(static_cast<size_t>(n) * 18);
    hoverText.reserve(36);
    const float hoverT = std::clamp(m_hoverGlobal, 0.0f, 1.0f);
    const bool hasCurrentHover = m_hoverIndex >= 0;
    const int displayHover = hasCurrentHover ? m_hoverIndex : ((hoverT > 0.0f) ? m_lastHoverIndex : -1);
    const std::vector<uint8_t>& displayMask = hasCurrentHover ? m_neighborMask : m_lastNeighborMask;
    for (int i = 0; i < n; ++i) {
        const float radius = (i < m_showRadii.size()) ? m_showRadii[i] : kDefaultNodeRadius;
        const float x = pos[2 * i];
        const float y = pos[2 * i + 1];
        if (x + radius < left || x - radius > right || y + radius < top || y - radius > bottom) {
            continue;
        }

        QColor color = m_textColor;
        float alpha = baseAlpha;
        float fontScale = 1.0f;
        std::vector<TextVertex>* target = &restText;
        if (displayHover >= 0 && hoverT > 0.0f) {
            if (i == displayHover) {
                if (hasCurrentHover) {
                    alpha = 1.0f;
                    fontScale = 1.0f + 2.0f * hoverT;
                    target = &hoverText;
                }
            } else if (i >= static_cast<int>(displayMask.size())
                       || !displayMask[static_cast<size_t>(i)]) {
                color = mixColor(m_textColor, m_textDimColor, hoverT);
                target = &dimText;
            }
        }
        appendTextLabel(*target, i, color, alpha, fontScale, pos);
    }

    m_textVertices.reserve(dimText.size() + restText.size() + hoverText.size());
    auto appendBatch = [this](const std::vector<TextVertex>& batch,
                              quint32& first,
                              quint32& count) {
        first = static_cast<quint32>(m_textVertices.size());
        count = static_cast<quint32>(batch.size());
        m_textVertices.insert(m_textVertices.end(), batch.begin(), batch.end());
    };
    // hover/fade 时，暗化文字先于高亮图形绘制；普通文字在图形之后绘制；
    // 当前 hover 标签最后绘制。正在淡出的上一个 hover 标签会回到普通文字批次，
    appendBatch(dimText, m_textDimFirst, m_textDimCount);
    appendBatch(restText, m_textRestFirst, m_textRestCount);
    appendBatch(hoverText, m_textHoverFirst, m_textHoverCount);
}

void ForceViewRhiWidget::prepareFrameVertices()
{
    m_frameVertices.clear();
    m_geometryDimFirst = 0;
    m_geometryDimCount = 0;
    m_geometryHighlightFirst = 0;
    m_geometryHighlightCount = 0;
    m_geometryRestFirst = 0;
    m_geometryRestCount = 0;
    if (!m_physicsState || m_physicsState->nNodes <= 0) {
        return;
    }
    const int n = m_physicsState->nNodes;
    if (m_renderPosSnapshot.size() < static_cast<size_t>(2 * m_physicsState->nNodes)) {
        return;
    }
    const float* pos = m_renderPosSnapshot.data();

    const float halfW = (m_viewportW / 2.0f) / std::max(m_zoom, 1e-6f);
    const float halfH = (m_viewportH / 2.0f) / std::max(m_zoom, 1e-6f);
    const float left = m_panX - halfW - kViewPadding;
    const float right = m_panX + halfW + kViewPadding;
    const float top = m_panY - halfH - kViewPadding;
    const float bottom = m_panY + halfH + kViewPadding;
    std::vector<uint8_t> visible(static_cast<size_t>(n), 0);
    for (int i = 0; i < n; ++i) {
        const float r = (i < m_showRadii.size()) ? m_showRadii[i] : kDefaultNodeRadius;
        const float x = pos[2 * i];
        const float y = pos[2 * i + 1];
        visible[static_cast<size_t>(i)] =
            (x + r >= left && x - r <= right && y + r >= top && y - r <= bottom) ? 1 : 0;
    }

    const int eCount = m_physicsState->edgeCount();
    m_frameVertices.reserve(static_cast<size_t>(eCount) * 9 + static_cast<size_t>(n) * 6);
    const int* edges = m_physicsState->edges.data();
    const float hoverT = std::clamp(m_hoverGlobal, 0.0f, 1.0f);
    const bool hasCurrentHover = m_hoverIndex >= 0;
    const int displayHover = hasCurrentHover ? m_hoverIndex : ((hoverT > 0.0f) ? m_lastHoverIndex : -1);
    const std::vector<uint8_t>& displayMask = hasCurrentHover ? m_neighborMask : m_lastNeighborMask;

    auto validEdge = [edges, n](int e, int& s, int& d) {
        s = edges[2 * e];
        d = edges[2 * e + 1];
        return s >= 0 && s < n && d >= 0 && d < n;
    };
    auto edgeVisible = [&visible](int s, int d) {
        return visible[static_cast<size_t>(s)] || visible[static_cast<size_t>(d)];
    };
    auto edgeHighlighted = [&displayMask, displayHover](int s, int d) {
        return (s == displayHover || d == displayHover)
            || (s < static_cast<int>(displayMask.size())
                && d < static_cast<int>(displayMask.size())
                && displayMask[static_cast<size_t>(s)]
                && displayMask[static_cast<size_t>(d)]);
    };
    auto appendEdge = [this, pos, &validEdge, &edgeVisible](std::vector<Vertex>& target,
                                                           int e,
                                                           const QColor& color) {
        int s = -1;
        int d = -1;
        if (!validEdge(e, s, d) || !edgeVisible(s, d)) {
            return;
        }
        appendLineQuad(target, s, d, color, pos);
        appendArrow(target, s, d, color, pos);
    };

    if (displayHover < 0 || hoverT <= 0.0f) {
        for (int e = 0; e < eCount; ++e) {
            appendEdge(m_frameVertices, e, m_edgeColor);
        }
        for (int i = 0; i < n; ++i) {
            if (!visible[static_cast<size_t>(i)]) {
                continue;
            }
            const QColor color = nodeColorFor(i);
            const float r = (i < m_showRadii.size()) ? m_showRadii[i] : kDefaultNodeRadius;
            appendCircle(m_frameVertices, pos[2 * i], pos[2 * i + 1], r, color);
        }
        // 后面再绘制 prepareTextVertices() 生成的普通文字批次。这里复用 "dim" 区间名
        // 只是为了避免再增加第四个 geometry range，不代表视觉上是暗化层。
        m_geometryDimFirst = 0;
        m_geometryDimCount = static_cast<quint32>(m_frameVertices.size());
        return;
    }

    std::vector<Vertex> dimEdges;
    std::vector<Vertex> highlightEdges;
    std::vector<Vertex> dimNodes;
    std::vector<Vertex> restNodes;
    dimEdges.reserve(static_cast<size_t>(eCount) * 9);
    highlightEdges.reserve(static_cast<size_t>(std::min(eCount, 64)) * 9);
    dimNodes.reserve(static_cast<size_t>(n) * 6);
    restNodes.reserve(static_cast<size_t>(n) * 6);

    const QColor dimEdgeColor = mixColor(m_edgeColor, m_edgeDimColor, hoverT);
    const QColor highlightEdgeColor = mixColor(m_edgeColor, m_hoverColor, hoverT);

    if (hasCurrentHover) {
        m_lastDimEdges.clear();
        m_lastHighlightEdges.clear();
        for (int e = 0; e < eCount; ++e) {
            int s = -1;
            int d = -1;
            if (!validEdge(e, s, d)) {
                continue;
            }
            if (edgeHighlighted(s, d)) {
                m_lastHighlightEdges.push_back(e);
                appendEdge(highlightEdges, e, highlightEdgeColor);
            } else {
                m_lastDimEdges.push_back(e);
                appendEdge(dimEdges, e, dimEdgeColor);
            }
        }
    } else {
        auto appendCachedEdges = [&](const std::vector<int>& cached, std::vector<Vertex>& target, const QColor& color) {
            for (int e : cached) {
                if (e >= 0 && e < eCount) {
                    appendEdge(target, e, color);
                }
            }
        };
        if (!m_lastDimEdges.empty() || !m_lastHighlightEdges.empty()) {
            appendCachedEdges(m_lastDimEdges, dimEdges, dimEdgeColor);
            appendCachedEdges(m_lastHighlightEdges, highlightEdges, highlightEdgeColor);
        } else {
            for (int e = 0; e < eCount; ++e) {
                int s = -1;
                int d = -1;
                if (!validEdge(e, s, d)) {
                    continue;
                }
                const bool highlight = edgeHighlighted(s, d);
                appendEdge(highlight ? highlightEdges : dimEdges,
                           e,
                           highlight ? highlightEdgeColor : dimEdgeColor);
            }
        }
    }

    for (int i = 0; i < n; ++i) {
        if (!visible[static_cast<size_t>(i)]) {
            continue;
        }
        QColor color = nodeColorFor(i);
        float radiusScale = 1.0f;
        const bool inHoverGroup = i == displayHover
            || (i < static_cast<int>(displayMask.size()) && displayMask[static_cast<size_t>(i)]);
        if (i == displayHover) {
            color = mixColor(color, m_hoverColor, hoverT);
            radiusScale = kHoverRadiusScale;
        } else if (!inHoverGroup) {
            color = mixColor(color, m_dimColor, hoverT);
        }
        const float r = ((i < m_showRadii.size()) ? m_showRadii[i] : kDefaultNodeRadius) * radiusScale;
        appendCircle(inHoverGroup ? restNodes : dimNodes, pos[2 * i], pos[2 * i + 1], r, color);
    }

    auto appendBatch = [this](const std::vector<Vertex>& batch,
                              quint32& first,
                              quint32& count) {
        first = static_cast<quint32>(m_frameVertices.size());
        count = static_cast<quint32>(batch.size());
        m_frameVertices.insert(m_frameVertices.end(), batch.begin(), batch.end());
    };
    std::vector<Vertex> dimGeometry;
    dimGeometry.reserve(dimEdges.size() + dimNodes.size());
    dimGeometry.insert(dimGeometry.end(), dimEdges.begin(), dimEdges.end());
    dimGeometry.insert(dimGeometry.end(), dimNodes.begin(), dimNodes.end());
    // 1) 暗化边/箭头 + 暗化节点；2) 高亮边/箭头；
    // 3) 基础节点 + hover 节点。文字区间在 renderRhi() 中交错插入。
    appendBatch(dimGeometry, m_geometryDimFirst, m_geometryDimCount);
    appendBatch(highlightEdges, m_geometryHighlightFirst, m_geometryHighlightCount);
    appendBatch(restNodes, m_geometryRestFirst, m_geometryRestCount);
}

void ForceViewRhiWidget::add_node_runtime(const QString& nodeId,
                                          float x,
                                          float y,
                                          const QString& label,
                                          float radius,
                                          const QColor& color)
{
    if (!m_physicsState || nodeId.isEmpty()) {
        return;
    }
    if (m_ids.contains(nodeId)) {
        return;
    }
    stopSimThread();

    const int oldN = m_physicsState->nNodes;
    m_physicsState->nNodes = oldN + 1;
    m_physicsState->pos.push_back(x);
    m_physicsState->pos.push_back(y);
    m_physicsState->vel.push_back(0.0f);
    m_physicsState->vel.push_back(0.0f);
    m_physicsState->mass.push_back(1.0f);
    m_physicsState->dragging.push_back(0);
    m_physicsState->dragPos.push_back(x);
    m_physicsState->dragPos.push_back(y);
    m_physicsState->syncRenderPosFromPos();

    m_ids.append(nodeId);
    m_labels.append(label.isEmpty() ? nodeId : label);
    m_showRadiiBase.append((std::isfinite(radius) && radius > 0.0f) ? radius : 7.0f);
    m_nodeColors.append(color.isValid() ? color : m_baseColor);
    invalidateTextLayoutForLabels();
    updateFactor();
    rebuildNeighborsFromEdges();
    rebuildSimulation();
    restartSimulation();
}

void ForceViewRhiWidget::update_node_runtime(const QString& nodeId,
                                             const QString& label,
                                             float radius,
                                             const QColor& color)
{
    const int index = m_ids.indexOf(nodeId);
    if (!m_physicsState || index < 0) {
        return;
    }

    bool labelsChanged = false;
    bool radiiChanged = false;
    if (!label.isEmpty() && m_labels[index] != label) {
        m_labels[index] = label;
        labelsChanged = true;
    }
    if (std::isfinite(radius) && radius > 0.0f
        && m_showRadiiBase[index] != radius) {
        m_showRadiiBase[index] = radius;
        radiiChanged = true;
    }
    if (color.isValid()) {
        m_nodeColors[index] = color;
    }
    if (labelsChanged) {
        invalidateTextLayoutForLabels();
    }
    if (radiiChanged) {
        updateFactor();
    }
    requestCanvasUpdate();
}

void ForceViewRhiWidget::resetInteractionStateForGraphMutation()
{
    const int n = m_physicsState ? std::max(0, m_physicsState->nNodes) : 0;
    m_hoverIndex = -1;
    m_lastHoverIndex = -1;
    m_hoverGlobal = 0.0f;
    m_selectedIndex = -1;
    m_dragging = false;
    m_pressedButton = Qt::NoButton;
    m_isPanning = false;
    m_dragOffsetX = 0.0f;
    m_dragOffsetY = 0.0f;
    m_neighborMask.assign(static_cast<size_t>(n), 0);
    m_lastNeighborMask.assign(static_cast<size_t>(n), 0);
    m_lastDimEdges.clear();
    m_lastHighlightEdges.clear();
    setCursor(Qt::ArrowCursor);
}

bool ForceViewRhiWidget::removeNodeInternal(int indexToRemove, bool restartAfterChange)
{
    if (!m_physicsState || indexToRemove < 0 || indexToRemove >= m_physicsState->nNodes) {
        return false;
    }
    if (restartAfterChange) {
        stopSimThread();
    }
    const int n = m_physicsState->nNodes;
    auto erasePair = [indexToRemove](std::vector<float>& values) {
        values.erase(values.begin() + 2 * indexToRemove, values.begin() + 2 * indexToRemove + 2);
    };
    erasePair(m_physicsState->pos);
    erasePair(m_physicsState->vel);
    erasePair(m_physicsState->dragPos);
    m_physicsState->mass.erase(m_physicsState->mass.begin() + indexToRemove);
    m_physicsState->dragging.erase(m_physicsState->dragging.begin() + indexToRemove);
    std::vector<int> newEdges;
    newEdges.reserve(m_physicsState->edges.size());
    for (int e = 0; e < m_physicsState->edgeCount(); ++e) {
        int s = m_physicsState->edges[2 * e];
        int d = m_physicsState->edges[2 * e + 1];
        if (s == indexToRemove || d == indexToRemove) {
            continue;
        }
        if (s > indexToRemove) {
            --s;
        }
        if (d > indexToRemove) {
            --d;
        }
        newEdges.push_back(s);
        newEdges.push_back(d);
    }
    m_physicsState->nNodes = n - 1;
    m_physicsState->edges = std::move(newEdges);
    m_physicsState->syncRenderPosFromPos();

    m_ids.removeAt(indexToRemove);
    m_labels.removeAt(indexToRemove);
    m_showRadiiBase.removeAt(indexToRemove);
    m_nodeColors.removeAt(indexToRemove);
    invalidateTextLayoutForLabels();
    updateFactor();
    resetInteractionStateForGraphMutation();
    rebuildNeighborsFromEdges();
    if (restartAfterChange) {
        rebuildSimulation();
        restartSimulation();
    }
    return true;
}

void ForceViewRhiWidget::remove_node_runtime(const QString& nodeId)
{
    const int idx = m_ids.indexOf(nodeId);
    if (idx >= 0) {
        removeNodeInternal(idx);
    }
}

void ForceViewRhiWidget::add_edge_runtime(const QString& uNodeId, const QString& vNodeId)
{
    if (!m_physicsState) {
        return;
    }
    const int u = m_ids.indexOf(uNodeId);
    const int v = m_ids.indexOf(vNodeId);
    if (u < 0 || v < 0 || u == v) {
        return;
    }
    for (int e = 0; e < m_physicsState->edgeCount(); ++e) {
        const int s = m_physicsState->edges[2 * e];
        const int d = m_physicsState->edges[2 * e + 1];
        if ((s == u && d == v) || (s == v && d == u)) {
            return;
        }
    }
    stopSimThread();
    m_physicsState->edges.push_back(u);
    m_physicsState->edges.push_back(v);
    rebuildNeighborsFromEdges();
    rebuildSimulation();
    restartSimulation();
}

void ForceViewRhiWidget::remove_edge_runtime(const QString& uNodeId, const QString& vNodeId)
{
    if (!m_physicsState) {
        return;
    }
    const int u = m_ids.indexOf(uNodeId);
    const int v = m_ids.indexOf(vNodeId);
    if (u < 0 || v < 0) {
        return;
    }
    stopSimThread();
    std::vector<int> edges;
    edges.reserve(m_physicsState->edges.size());
    for (int e = 0; e < m_physicsState->edgeCount(); ++e) {
        const int s = m_physicsState->edges[2 * e];
        const int d = m_physicsState->edges[2 * e + 1];
        if ((s == u && d == v) || (s == v && d == u)) {
            continue;
        }
        edges.push_back(s);
        edges.push_back(d);
    }
    m_physicsState->edges = std::move(edges);
    rebuildNeighborsFromEdges();
    rebuildSimulation();
    restartSimulation();
}

void ForceViewRhiWidget::apply_diff_runtime(const QVariantList& diffList)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, diffList]() {
            apply_diff_runtime(diffList);
        }, Qt::QueuedConnection);
        return;
    }
    if (!m_physicsState || diffList.isEmpty()) {
        return;
    }

    bool stopped = false;
    auto ensureStopped = [this, &stopped]() {
        if (!stopped) {
            stopSimThread();
            stopped = true;
        }
    };
    bool changed = false;
    bool topologyChanged = false;
    bool labelsChanged = false;
    bool radiiChanged = false;

    for (const QVariant& item : diffList) {
        const QVariantMap map = item.toMap();
        const QString op = map.value(QStringLiteral("op")).toString();
        if (op == QStringLiteral("add_node")) {
            const QVariantMap attr = map.value(QStringLiteral("attr")).toMap();
            const QString nodeId = map.value(QStringLiteral("id")).toString();
            if (nodeId.isEmpty() || m_ids.contains(nodeId)) {
                continue;
            }

            const int oldN = m_physicsState->nNodes;
            const float x = map.value(QStringLiteral("x"), 0.0f).toFloat();
            const float y = map.value(QStringLiteral("y"), 0.0f).toFloat();
            const float radius = attr.value(QStringLiteral("radius"), 7.0f).toFloat();
            const QColor color = attr.value(QStringLiteral("color")).value<QColor>();
            const QString label = attr.value(QStringLiteral("label")).toString();

            ensureStopped();
            m_physicsState->nNodes = oldN + 1;
            m_physicsState->pos.push_back(x);
            m_physicsState->pos.push_back(y);
            m_physicsState->vel.push_back(0.0f);
            m_physicsState->vel.push_back(0.0f);
            m_physicsState->mass.push_back(1.0f);
            m_physicsState->dragging.push_back(0);
            m_physicsState->dragPos.push_back(x);
            m_physicsState->dragPos.push_back(y);
            m_physicsState->syncRenderPosFromPos();

            m_ids.append(nodeId);
            m_labels.append(label.isEmpty() ? nodeId : label);
            m_showRadiiBase.append(
                (std::isfinite(radius) && radius > 0.0f) ? radius : 7.0f);
            m_nodeColors.append(color.isValid() ? color : m_baseColor);
            changed = true;
            topologyChanged = true;
            labelsChanged = true;
            radiiChanged = true;
        } else if (op == QStringLiteral("del_node") || op == QStringLiteral("remove_node")) {
            const int idx = m_ids.indexOf(map.value(QStringLiteral("id")).toString());
            if (idx >= 0) {
                ensureStopped();
            }
            if (idx >= 0 && removeNodeInternal(idx, false)) {
                changed = true;
                topologyChanged = true;
                labelsChanged = true;
                radiiChanged = true;
            }
        } else if (op == QStringLiteral("update_node")) {
            const int index = m_ids.indexOf(map.value(QStringLiteral("id")).toString());
            if (index < 0) {
                continue;
            }
            const QVariantMap attr = map.value(QStringLiteral("attr")).toMap();
            const QString label = attr.value(QStringLiteral("label")).toString();
            const float radius = attr.value(QStringLiteral("radius"),
                                            m_showRadiiBase[index]).toFloat();
            const QColor color = attr.value(QStringLiteral("color")).value<QColor>();
            if (!label.isEmpty() && m_labels[index] != label) {
                m_labels[index] = label;
                labelsChanged = true;
                changed = true;
            }
            if (std::isfinite(radius) && radius > 0.0f
                && m_showRadiiBase[index] != radius) {
                m_showRadiiBase[index] = radius;
                radiiChanged = true;
                changed = true;
            }
            if (color.isValid() && m_nodeColors[index] != color) {
                m_nodeColors[index] = color;
                changed = true;
            }
        } else if (op == QStringLiteral("add_edge")) {
            const int u = m_ids.indexOf(map.value(QStringLiteral("u")).toString());
            const int v = m_ids.indexOf(map.value(QStringLiteral("v")).toString());
            if (u < 0 || v < 0 || u == v) {
                continue;
            }
            bool exists = false;
            for (int e = 0; e < m_physicsState->edgeCount(); ++e) {
                const int s = m_physicsState->edges[2 * e];
                const int d = m_physicsState->edges[2 * e + 1];
                if ((s == u && d == v) || (s == v && d == u)) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                ensureStopped();
                m_physicsState->edges.push_back(u);
                m_physicsState->edges.push_back(v);
                changed = true;
                topologyChanged = true;
            }
        } else if (op == QStringLiteral("update_edge")) {
            const int u = m_ids.indexOf(map.value(QStringLiteral("u")).toString());
            const int v = m_ids.indexOf(map.value(QStringLiteral("v")).toString());
            if (u < 0 || v < 0) {
                continue;
            }
            for (int e = 0; e < m_physicsState->edgeCount(); ++e) {
                const int s = m_physicsState->edges[2 * e];
                const int d = m_physicsState->edges[2 * e + 1];
                if ((s == u && d == v) || (s == v && d == u)) {
                    // Physics topology is unchanged.  Keep the operation explicit so
                    // renderers that later style edges by relation type receive it.
                    changed = true;
                    break;
                }
            }
        } else if (op == QStringLiteral("del_edge") || op == QStringLiteral("remove_edge")) {
            const int u = m_ids.indexOf(map.value(QStringLiteral("u")).toString());
            const int v = m_ids.indexOf(map.value(QStringLiteral("v")).toString());
            if (u < 0 || v < 0) {
                continue;
            }

            bool removed = false;
            std::vector<int> edges;
            edges.reserve(m_physicsState->edges.size());
            for (int e = 0; e < m_physicsState->edgeCount(); ++e) {
                const int s = m_physicsState->edges[2 * e];
                const int d = m_physicsState->edges[2 * e + 1];
                if ((s == u && d == v) || (s == v && d == u)) {
                    removed = true;
                    continue;
                }
                edges.push_back(s);
                edges.push_back(d);
            }
            if (removed) {
                ensureStopped();
                m_physicsState->edges = std::move(edges);
                changed = true;
                topologyChanged = true;
            }
        }
    }

    if (!changed) {
        return;
    }

    if (labelsChanged) {
        invalidateTextLayoutForLabels();
    }
    if (radiiChanged) {
        updateFactor();
    }
    if (topologyChanged) {
        resetInteractionStateForGraphMutation();
        rebuildNeighborsFromEdges();
        rebuildSimulation();
        restartSimulation();
    } else {
        requestCanvasUpdate();
    }
}
