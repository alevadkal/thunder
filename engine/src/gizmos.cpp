#include "gizmos.h"

#include "components/camera.h"
#include "components/transform.h"

#include "resources/material.h"
#include "resources/mesh.h"

#include "commandbuffer.h"

#define OVERRIDE "texture0"

Mesh *Gizmos::s_Wire = nullptr;
Mesh *Gizmos::s_Solid = nullptr;

MaterialInstance *Gizmos::s_WireMaterial = nullptr;
MaterialInstance *Gizmos::s_SolidMaterial = nullptr;

Material *Gizmos::s_SpriteMaterial = nullptr;

struct SpriteBatches {
    Mesh *mesh;
    MaterialInstance *material;
};
static unordered_map<string, SpriteBatches> s_Sprites;

void Gizmos::init() {
    if(s_WireMaterial == nullptr) {
        Material *m = Engine::loadResource<Material>(".embedded/gizmo.shader");
        if(m) {
            s_WireMaterial = m->createInstance();
        }
    }
    if(s_SolidMaterial == nullptr) {
        Material *m = Engine::loadResource<Material>(".embedded/solid.shader");
        if(m) {
            s_SolidMaterial = m->createInstance();
        }
    }
    if(s_SpriteMaterial == nullptr) {
        s_SpriteMaterial = Engine::loadResource<Material>(".embedded/DefaultSprite.mtl");
    }

    if(s_Wire == nullptr) {
        s_Wire = Engine::objectCreate<Mesh>("Lines");
        s_Wire->makeDynamic();
    }
    if(s_Solid == nullptr) {
        s_Solid = Engine::objectCreate<Mesh>("Solid");
        s_Solid->makeDynamic();
    }
}

void Gizmos::beginDraw() {
    s_Wire->clear();
    s_Solid->clear();

    for(auto &it : s_Sprites) {
        delete it.second.mesh;
        delete it.second.material;
    }
    s_Sprites.clear();
}

void Gizmos::endDraw(CommandBuffer *buffer) {
    if(CommandBuffer::isInited()) {
        Matrix4 v, p;
        Camera *cam = Camera::current();
        if(cam) {
            v = cam->viewMatrix();
            p = cam->projectionMatrix();
        }

        buffer->setViewProjection(v, p);
        for(auto &it : s_Sprites) {
            buffer->drawMesh(Matrix4(), it.second.mesh, 0, CommandBuffer::TRANSLUCENT, it.second.material);
        }
        if(!s_Solid->vertices().empty()) {
            buffer->drawMesh(Matrix4(), s_Solid, 0, CommandBuffer::TRANSLUCENT, s_SolidMaterial);
        }
        if(!s_Wire->vertices().empty()) {
            buffer->drawMesh(Matrix4(), s_Wire, 0, CommandBuffer::TRANSLUCENT, s_WireMaterial);
        }
    }
}

void Gizmos::drawBox(const Vector3 &center, const Vector3 &size, const Vector4 &color, const Matrix4 &transform) {
    Vector3 min(center - size * 0.5f);
    Vector3 max(center + size * 0.5f);

    Mesh mesh;
    mesh.setVertices({
        Vector3(min.x, min.y, min.z),
        Vector3(max.x, min.y, min.z),
        Vector3(max.x, min.y, max.z),
        Vector3(min.x, min.y, max.z),

        Vector3(min.x, max.y, min.z),
        Vector3(max.x, max.y, min.z),
        Vector3(max.x, max.y, max.z),
        Vector3(min.x, max.y, max.z)
    });
    mesh.setIndices({0, 1, 2, 0, 2, 3, // bottom
                     4, 6, 5, 4, 7, 6, // top
                     0, 1, 5, 0, 5, 4, // front
                     3, 6, 2, 3, 7, 6, // back
                     0, 7, 3, 0, 4, 7, // left
                     1, 6, 2, 1, 5, 6, // right
                    });
    mesh.setColors(Vector4Vector(mesh.vertices().size(), color));

    s_Solid->batchMesh(mesh, &transform);
}

void Gizmos::drawIcon(const Vector3 &center, const Vector2 &size, const string &name, const Vector4 &color, const Matrix4 &transform) {
    Matrix4 model(center, Quaternion(), Vector3(size, size.x));
    Matrix4 q = model * Matrix4(Camera::current()->transform()->quaternion().toMatrix());

    Mesh mesh;
    mesh.setIndices({0, 1, 2, 0, 2, 3});
    mesh.setVertices({Vector3(-0.5f,-0.5f, 0.0f),
                      Vector3(-0.5f, 0.5f, 0.0f),
                      Vector3( 0.5f, 0.5f, 0.0f),
                      Vector3( 0.5f,-0.5f, 0.0f)});
    mesh.setUv0({Vector2(0.0f, 0.0f),
                 Vector2(0.0f, 1.0f),
                 Vector2(1.0f, 1.0f),
                 Vector2(1.0f, 0.0f)});
    mesh.setColors(Vector4Vector(4, color));

    auto it = s_Sprites.find(name);
    if(it != s_Sprites.end()) {
        it->second.mesh->batchMesh(mesh, &q);
    } else {
        if(s_SpriteMaterial) {
            SpriteBatches batch;
            batch.mesh = Engine::objectCreate<Mesh>(name);
            batch.mesh->batchMesh(mesh, &q);
            batch.material = s_SpriteMaterial->createInstance();
            batch.material->setTexture(OVERRIDE, Engine::loadResource<Texture>(name));
            s_Sprites[name] = batch;
        }
    }
}

void Gizmos::drawMesh(Mesh &mesh, const Vector4 &color, const Matrix4 &transform) {
    Mesh m;
    m.setVertices(mesh.vertices());
    m.setIndices(mesh.indices());
    m.setNormals(mesh.normals());
    m.setTangents(mesh.tangents());
    m.setUv0(mesh.uv0());
    m.setColors(Vector4Vector(m.vertices().size(), color));

    s_Solid->batchMesh(m, &transform);
}

void Gizmos::drawSphere(const Vector3 &center, float radius, const Vector4 &color, const Matrix4 &transform) {

}

void Gizmos::drawDisk(const Vector3 &center, float radius, float start, float angle, const Vector4 &color, const Matrix4 &transform) {
    Mesh mesh;
    mesh.setVertices(Mathf::pointsArc(Quaternion(), radius, start, angle, 180, true));
    uint32_t size = mesh.vertices().size();

    IndexVector indices;
    indices.resize((size - 1) * 3);
    for(int i = 0; i < size - 1; i++) {
        indices[i * 3] = 0;
        indices[i * 3 + 1] = i;
        indices[i * 3 + 2] = i+1;
    }
    mesh.setIndices(indices);
    mesh.setColors(Vector4Vector(size, color));

    Matrix4 t(transform);
    t[12] += center.x;
    t[13] += center.y;
    t[14] += center.z;

    s_Solid->batchMesh(mesh, &t);
}

void Gizmos::drawLines(const Vector3Vector &points, const IndexVector &indices, const Vector4 &color, const Matrix4 &transform) {
    Mesh mesh;
    mesh.setVertices(points);
    mesh.setIndices(indices);
    mesh.setColors(Vector4Vector(points.size(), color));

    s_Wire->batchMesh(mesh, &transform);
}

void Gizmos::drawArc(const Vector3 &center, float radius, float from, float to, const Vector4 &color, const Matrix4 &transform) {
    Mesh mesh;
    mesh.setVertices(Mathf::pointsArc(Quaternion(), radius, from, to, 180));
    uint32_t size = mesh.vertices().size();

    IndexVector indices;
    indices.resize((size - 1) * 2);
    for(int i = 0; i < size - 1; i++) {
        indices[i * 2] = i;
        indices[i * 2 + 1] = i+1;
    }
    mesh.setIndices(indices);
    mesh.setColors(Vector4Vector(size, color));

    Matrix4 t(transform);
    t[12] += center.x;
    t[13] += center.y;
    t[14] += center.z;

    s_Wire->batchMesh(mesh, &t);
}

void Gizmos::drawCircle(const Vector3 &center, float radius, const Vector4 &color, const Matrix4 &transform) {
    drawArc(center, radius, 0, 360, color, transform);
}

void Gizmos::drawRectangle(const Vector3 &center, const Vector2 &size, const Vector4 &color, const Matrix4 &transform) {
    Vector2 min(Vector2(center.x, center.y) - size * 0.5f);
    Vector2 max(Vector2(center.x, center.y) + size * 0.5f);

    Mesh mesh;
    mesh.setVertices({
        Vector3(min.x, min.y, center.z),
        Vector3(max.x, min.y, center.z),
        Vector3(max.x, max.y, center.z),
        Vector3(min.x, max.y, center.z)
    });
    mesh.setIndices({
        0, 1, 1, 2, 2, 3, 3, 0
    });
    mesh.setColors(Vector4Vector(mesh.vertices().size(), color));

    s_Wire->batchMesh(mesh, &transform);
}

void Gizmos::drawWireBox(const Vector3 &center, const Vector3 &size, const Vector4 &color, const Matrix4 &transform) {
    Vector3 min(center - size * 0.5f);
    Vector3 max(center + size * 0.5f);

    Mesh mesh;
    mesh.setVertices({
        Vector3(min.x, min.y, min.z),
        Vector3(max.x, min.y, min.z),
        Vector3(max.x, min.y, max.z),
        Vector3(min.x, min.y, max.z),

        Vector3(min.x, max.y, min.z),
        Vector3(max.x, max.y, min.z),
        Vector3(max.x, max.y, max.z),
        Vector3(min.x, max.y, max.z)
    });
    mesh.setIndices({0, 1, 1, 2, 2, 3, 3, 0,
                     4, 5, 5, 6, 6, 7, 7, 4,
                     0, 4, 1, 5, 2, 6, 3, 7});
    mesh.setColors(Vector4Vector(mesh.vertices().size(), color));

    s_Wire->batchMesh(mesh, &transform);
}

void Gizmos::drawWireMesh(Mesh &mesh, const Vector4 &color, const Matrix4 &transform) {
    Mesh m;
    m.setVertices(mesh.vertices());
    m.setIndices(mesh.indices());
    m.setColors(Vector4Vector(m.vertices().size(), color));

    s_Wire->batchMesh(m, &transform);
}

void Gizmos::drawWireSphere(const Vector3 &center, float radius, const Vector4 &color, const Matrix4 &transform) {
    drawCircle(center, radius, color, transform);
    Matrix4 t = transform * Matrix4(Quaternion(Vector3(1, 0, 0), 90).toMatrix());
    drawCircle(center, radius, color, t);
    t = transform * Matrix4(Quaternion(Vector3(0, 0, 1), 90).toMatrix());
    drawCircle(center, radius, color, t);
}

void Gizmos::drawWireCapsule(const Vector3 &center, float radius, float height, const Vector4 &color, const Matrix4 &transform) {
    float half = height * 0.5f - radius;
    {
        Vector3 cap(0, half, 0);
        Matrix4 t;
        t = transform * Matrix4(cap, Quaternion(), Vector3(1.0f));
        drawCircle(Vector3(), radius, color, t);

        t = transform * Matrix4(cap, Quaternion(Vector3(-90,  0, 0)), Vector3(1.0f));
        drawArc(Vector3(), radius, 0, 180, color, t);

        t = transform * Matrix4(cap, Quaternion(Vector3(-90, 90, 0)), Vector3(1.0f));
        drawArc(Vector3(), radius, 0, 180, color, t);
    }
    {
        Vector3 cap(0,-half, 0);
        Matrix4 t(transform);
        t = transform * Matrix4(cap, Quaternion(), Vector3(1.0f));
        drawCircle(Vector3(), radius, color, t);

        t = transform * Matrix4(cap, Quaternion(Vector3(-90,  0, 0)), Vector3(1.0f));
        drawArc(Vector3(), radius, 0, 180, color, t);

        t = transform * Matrix4(cap, Quaternion(Vector3(90, 90, 0)), Vector3(1.0f));
        drawArc(Vector3(), radius, 0, 180, color, t);
    }

    Vector3Vector points = { Vector3( radius, half, 0),
                             Vector3( radius,-half, 0),
                             Vector3(-radius, half, 0),
                             Vector3(-radius,-half, 0),
                             Vector3( 0, half, radius),
                             Vector3( 0,-half, radius),
                             Vector3( 0, half,-radius),
                             Vector3( 0,-half,-radius)};

    IndexVector indices = {0, 1, 2, 3, 4, 5, 6, 7};

    drawLines(points, indices, color, transform);
}
