// Offline migration utility only. The new renderer never links Quick3DUtils.
#include <QtQuick3DUtils/private/qssgmesh_p.h>
#include <QtQuick3DUtils/private/qssgutils_p.h>
#include <QColor>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>
#include <limits>
#include <cstring>
#include <algorithm>

namespace {
QJsonArray linearColor(const char *argb)
{
    // QML colors use #AARRGGBB. Preserve Qt's color conversion, including alpha.
    const auto color = QSSGUtils::color::sRGBToLinear(QColor(QLatin1String(argb)));
    return {color.x(), color.y(), color.z(), color.w()};
}

struct Exporter {
    QByteArray binary;
    QJsonArray views, accessors, meshes;

    int appendView(const QByteArray &data, int target)
    {
        while (binary.size() % 4)
            binary.append(char(0));
        const int index = int(views.size());
        views.append(QJsonObject{{"buffer", 0}, {"byteOffset", int(binary.size())},
                                  {"byteLength", int(data.size())}, {"target", target}});
        binary.append(data);
        return index;
    }

    int appendAttribute(const QSSGMesh::Mesh::VertexBuffer &vertices,
                        const QByteArray &name, int components)
    {
        for (const auto &entry : vertices.entries) {
            if (entry.name != name)
                continue;
            if (entry.componentType != QSSGMesh::Mesh::ComponentType::Float32
                || entry.componentCount != quint32(components))
                return -1;
            const int count = int(vertices.data.size() / vertices.stride);
            QByteArray packed(count * components * int(sizeof(float)), char(0));
            QJsonArray lower, upper;
            float lo[3] = {1e30f,1e30f,1e30f}, hi[3] = {-1e30f,-1e30f,-1e30f};
            for (int i = 0; i < count; ++i) {
                const auto *src = vertices.data.constData() + i * vertices.stride + entry.offset;
                std::memcpy(packed.data() + i * components * sizeof(float), src, components * sizeof(float));
                if (name == "attr_uv0") {
                    float v;
                    std::memcpy(&v, src + sizeof(float), sizeof(float));
                    v = 1.0f - v; // Qt mesh bottom-up UV -> glTF top-down image UV
                    std::memcpy(packed.data() + (i * components + 1) * sizeof(float), &v, sizeof(float));
                }
                if (name == "attr_pos")
                    for (int axis = 0; axis < 3; ++axis) {
                        float value;
                        std::memcpy(&value, src + axis * sizeof(float), sizeof(float));
                        lo[axis] = std::min(lo[axis], value);
                        hi[axis] = std::max(hi[axis], value);
                    }
            }
            const int view = appendView(packed, 34962);
            QJsonObject accessor{{"bufferView", view}, {"componentType", 5126},
                                 {"count", count}, {"type", QString("VEC%1").arg(components)}};
            if (name == "attr_pos") {
                for (int axis = 0; axis < 3; ++axis) { lower.append(lo[axis]); upper.append(hi[axis]); }
                accessor.insert("min", lower);
                accessor.insert("max", upper);
            }
            const int result = int(accessors.size());
            accessors.append(accessor);
            return result;
        }
        return -1;
    }

    bool appendMesh(const QString &path, const QString &name)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return false;
        const auto mesh = QSSGMesh::Mesh::loadMesh(&file);
        if (!mesh.isValid() || mesh.drawMode() != QSSGMesh::Mesh::DrawMode::Triangles)
            return false;
        const auto vertices = mesh.vertexBuffer();
        if (!vertices.stride || vertices.data.size() % vertices.stride)
            return false;
        const int position = appendAttribute(vertices, "attr_pos", 3);
        const int normal = appendAttribute(vertices, "attr_norm", 3);
        const int uv = appendAttribute(vertices, "attr_uv0", 2);
        if (position < 0 || normal < 0 || uv < 0)
            return false;
        QJsonObject attributes{{"POSITION", position}, {"NORMAL", normal}, {"TEXCOORD_0", uv}};
        const auto indices = mesh.indexBuffer();
        const bool shortIndices = indices.componentType == QSSGMesh::Mesh::ComponentType::UnsignedInt16;
        if (!shortIndices && indices.componentType != QSSGMesh::Mesh::ComponentType::UnsignedInt32)
            return false;
        const int stride = shortIndices ? 2 : 4;
        const int indexView = appendView(indices.data, 34963);
        QJsonArray primitives;
        int subsetNumber = 0;
        for (const auto &subset : mesh.subsets()) {
            const int indexAccessor = int(accessors.size());
            accessors.append(QJsonObject{{"bufferView", indexView},
                {"byteOffset", int(subset.offset) * stride}, {"count", int(subset.count)},
                {"componentType", shortIndices ? 5123 : 5125}, {"type", "SCALAR"}});
            // Mirrors Dvd.qml's ordered material list: first cover, then shell;
            // the disc has an additional metallic inner/rainbow subset.
            const int material = subsetNumber == 0 ? 0
                : (name == "disc" ? std::min(subsetNumber + 1, 3) : 1);
            primitives.append(QJsonObject{{"attributes", attributes}, {"indices", indexAccessor},
                                          {"material", material}, {"mode", 4}});
            ++subsetNumber;
        }
        meshes.append(QJsonObject{{"name", name}, {"primitives", primitives}});
        return true;
    }
};
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({"output", "Output GLB path.", "path"});
    parser.process(app);
    if (!parser.isSet("output"))
        return 2;
    const QDir root(QStringLiteral(DARKEYE_BASELINE_SOURCE_DIR));
    Exporter exporter;
    for (const auto &part : {qMakePair("back", "back"), qMakePair("spine", "spine"),
                            qMakePair("front", "front"), qMakePair("cD", "disc")}) {
        const auto path = root.filePath(QString("resources/meshes/%1.mesh").arg(part.first));
        if (!exporter.appendMesh(path, QLatin1String(part.second))) {
            qCritical("Could not export %s", qPrintable(path));
            return 3;
        }
    }
    const QJsonArray materials{
        QJsonObject{{"name", "cover"}, {"doubleSided", true}, {"pbrMetallicRoughness",
            QJsonObject{{"baseColorFactor", QJsonArray{1,1,1,1}}, {"metallicFactor", 0}, {"roughnessFactor", 0.9}}}},
        QJsonObject{{"name", "shell"}, {"doubleSided", true}, {"pbrMetallicRoughness",
            QJsonObject{{"baseColorFactor", QJsonArray{1,1,1,1}}, {"metallicFactor", 0}, {"roughnessFactor", 0.9}}}},
        QJsonObject{{"name", "disc-transparent"}, {"doubleSided", true}, {"alphaMode", "BLEND"},
            {"pbrMetallicRoughness", QJsonObject{{"baseColorFactor", linearColor("#ffffffcc")},
                {"metallicFactor", 0}, {"roughnessFactor", 0.9}}}},
        QJsonObject{{"name", "disc-metal"}, {"doubleSided", true}, {"pbrMetallicRoughness",
            QJsonObject{{"baseColorFactor", linearColor("#ffcccccc")}, {"metallicFactor", 0.15}, {"roughnessFactor", 0.85}}}}
    };
    // Exact closed-box local hierarchy from Dvd.qml. Disc is its sibling;
    // the runtime DVD controller controls hinge rotations and disc visibility.
    QJsonArray nodes{
        QJsonObject{{"name", "dvd"}, {"children", QJsonArray{1,4}}},
        QJsonObject{{"name", "back"}, {"mesh", 0}, {"translation", QJsonArray{-0.0067071,0.095,-0.000478134}}, {"children", QJsonArray{2}}},
        QJsonObject{{"name", "spine"}, {"mesh", 1}, {"children", QJsonArray{3}}},
        QJsonObject{{"name", "front"}, {"mesh", 2}, {"translation", QJsonArray{0.0134142,0,0}}},
        QJsonObject{{"name", "disc"}, {"mesh", 3}, {"translation", QJsonArray{-0.0048184,0.095,-0.0667828}}}
    };
    const QJsonObject document{{"asset", QJsonObject{{"version", "2.0"},
        {"generator", "DarkEye offline Qt mesh migration; original geometry and UV preserved"}}},
        {"scene", 0}, {"scenes", QJsonArray{QJsonObject{{"nodes", QJsonArray{0}}}}},
        {"nodes", nodes}, {"meshes", exporter.meshes}, {"materials", materials},
        {"bufferViews", exporter.views}, {"accessors", exporter.accessors},
        {"buffers", QJsonArray{QJsonObject{{"byteLength", int(exporter.binary.size())}}}}};
    QByteArray json = QJsonDocument(document).toJson(QJsonDocument::Compact);
    while (json.size() % 4) json.append(' ');
    while (exporter.binary.size() % 4) exporter.binary.append(char(0));
    QFile output(parser.value("output"));
    if (!QDir().mkpath(QFileInfo(output).absolutePath()) || !output.open(QIODevice::WriteOnly))
        return 4;
    QDataStream stream(&output);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << quint32(0x46546c67) << quint32(2)
           << quint32(12 + 8 + json.size() + 8 + exporter.binary.size());
    stream << quint32(json.size()) << quint32(0x4e4f534a);
    stream.writeRawData(json.constData(), int(json.size()));
    stream << quint32(exporter.binary.size()) << quint32(0x004e4942);
    stream.writeRawData(exporter.binary.constData(), int(exporter.binary.size()));
    return stream.status() == QDataStream::Ok ? 0 : 5;
}
