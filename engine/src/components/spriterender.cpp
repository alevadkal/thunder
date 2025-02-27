#include "components/spriterender.h"
#include "components/actor.h"
#include "components/transform.h"

#include "resources/material.h"
#include "resources/mesh.h"
#include "resources/sprite.h"

#include "commandbuffer.h"
#include "pipelinecontext.h"

#include <math.h>

#define MATERIAL "Material"
#define BASEMAP "BaseMap"

#define OVERRIDE "texture0"
#define COLOR "uni.color0"

#define DEFAULTSPRITE ".embedded/DefaultSprite.mtl"

static hash<string> hash_str;

class SpriteRenderPrivate : public Resource::IObserver {
public:
    SpriteRenderPrivate() :
            m_sprite(nullptr),
            m_texture(nullptr),
            m_material(nullptr),
            m_mesh(PipelineContext::defaultPlane()),
            m_customMesh(nullptr),
            m_color(1.0f),
            m_size(1.0f),
            m_hash(0),
            m_drawMode(0),
            m_layer(0) {

    }

    ~SpriteRenderPrivate() {
        if(m_sprite) {
            m_sprite->unsubscribe(this);
        }
    }

    void resourceUpdated(const Resource *resource, Resource::ResourceState state) override {
        if(resource == m_sprite){
            if(state == Resource::Ready) {
                m_material->setTexture(OVERRIDE, m_sprite->texture());
                composeMesh();
            } else if(state == Resource::ToBeDeleted) {
                m_sprite = nullptr;
                m_material->setTexture(OVERRIDE, nullptr);
                composeMesh();
            }
        }
    }

    void composeMesh(bool resetSize = false) {
        if(m_sprite) {
            if(m_customMesh == nullptr) {
                m_customMesh = Engine::objectCreate<Mesh>("");
            }

            bool result = SpriteRender::composeMesh(m_sprite, m_hash, m_customMesh, m_size, m_drawMode, resetSize);
            if(result) {
                return;
            }
        }
        Engine::unloadResource(m_customMesh);
        m_customMesh = nullptr;
    }

    Sprite *m_sprite;

    Texture *m_texture;

    MaterialInstance *m_material;

    Mesh *m_mesh;
    Mesh *m_customMesh;

    Vector4 m_color;

    Vector2 m_size;

    string m_item;
    int m_hash;

    int m_drawMode;

    int m_layer;
};
/*!
    \class SpriteRender
    \brief Draws a sprite for the 2D graphics.
    \inmodule Engine

    The SpriteRender component allows you to display images as sprites to use in both 2D and 3D scenes.
*/

/*!
    \enum SpriteRender::DrawMode

    \value Sliced \c The SpriteRender will respect 9 patch slicing rules for the size property.
    \value Tiled \c The SpriteRender will duplicate sprites to fill the size property rectangle.
*/

SpriteRender::SpriteRender() :
        p_ptr(new SpriteRenderPrivate) {

}

SpriteRender::~SpriteRender() {
    delete p_ptr;
    p_ptr = nullptr;
}
/*!
    \internal
*/
void SpriteRender::draw(CommandBuffer &buffer, uint32_t layer) {
    Actor *a = actor();
    if(p_ptr->m_mesh && layer & a->layers()) {
        if(layer & CommandBuffer::RAYCAST) {
            buffer.setColor(CommandBuffer::idToColor(a->uuid()));
        }

        buffer.drawMesh(a->transform()->worldTransform(),
                        (p_ptr->m_customMesh) ? p_ptr->m_customMesh : p_ptr->m_mesh,
                        0, layer, p_ptr->m_material);
        buffer.setColor(Vector4(1.0f));
    }
}
/*!
    \internal
*/
AABBox SpriteRender::localBound() const {
    if(p_ptr->m_customMesh) {
        return p_ptr->m_customMesh->bound();
    } else if(p_ptr->m_mesh) {
        return p_ptr->m_mesh->bound();
    }
    return Renderable::localBound();
}
/*!
    Returns an instantiated Material assigned to SpriteRender.
*/
Material *SpriteRender::material() const {
    if(p_ptr->m_material) {
        return p_ptr->m_material->material();
    }
    return nullptr;
}
/*!
    Creates a new instance of \a material and assigns it.
*/
void SpriteRender::setMaterial(Material *material) {
    if(!p_ptr->m_material || p_ptr->m_material->material() != material) {
        if(p_ptr->m_material) {
            delete p_ptr->m_material;
            p_ptr->m_material = nullptr;
        }

        if(material) {
            p_ptr->m_material = material->createInstance();
            p_ptr->m_material->setTexture(OVERRIDE, texture());
            p_ptr->m_material->setVector4(COLOR, &p_ptr->m_color);
        }
    }
}
/*!
    Returns a sprite.
*/
Sprite *SpriteRender::sprite() const {
    return p_ptr->m_sprite;
}
/*!
    Replaces current \a sprite with a new one.
*/
void SpriteRender::setSprite(Sprite *sprite) {
    if(p_ptr->m_sprite) {
        p_ptr->m_sprite->unsubscribe(p_ptr);
    }
    p_ptr->m_sprite = sprite;
    if(p_ptr->m_sprite) {
        p_ptr->m_sprite->subscribe(p_ptr);
        p_ptr->composeMesh();
        if(p_ptr->m_material) {
            p_ptr->m_material->setTexture(OVERRIDE, texture());
        }
    }
}
/*!
    Returns current assigned texture.
*/
Texture *SpriteRender::texture() const {
    if(p_ptr->m_sprite) {
        return p_ptr->m_sprite->texture();
    }
    return p_ptr->m_texture;
}
/*!
    Replaces current \a texture with a new one.
*/
void SpriteRender::setTexture(Texture *texture) {
    p_ptr->m_texture = texture;
    if(p_ptr->m_material) {
        p_ptr->composeMesh();
        p_ptr->m_material->setTexture(OVERRIDE, SpriteRender::texture());
    }
}
/*!
    Returns the color of the sprite to be drawn.
*/
Vector4 &SpriteRender::color() const {
    return p_ptr->m_color;
}
/*!
    Changes the \a color of the sprite to be drawn.
*/
void SpriteRender::setColor(const Vector4 color) {
    p_ptr->m_color = color;
    if(p_ptr->m_material) {
        p_ptr->m_material->setVector4(COLOR, &p_ptr->m_color);
    }
}
/*!
    Returns the current item name of sprite from the sprite sheet.
*/
string SpriteRender::item() const {
    return p_ptr->m_item;
}
/*!
    Sets the current sub \a item name of sprite from the sprite sheet.
*/
void SpriteRender::setItem(const string item) {
    p_ptr->m_item = item;
    p_ptr->m_hash = hash_str(p_ptr->m_item);
    p_ptr->composeMesh(true);
}
/*!
    Returns size of sprite.
*/
Vector2 &SpriteRender::size() const {
    return p_ptr->m_size;
}
/*!
    Sets a new \a size of sprite.
*/
void SpriteRender::setSize(const Vector2 size) {
    p_ptr->m_size = size;
    p_ptr->composeMesh();
}
/*!
    Returns a draw mode for the sprite.
    Please check SpriteRender::DrawMode for more details.
*/
int SpriteRender::drawMode() const {
    return p_ptr->m_drawMode;
}
/*!
    Sets a draw \a mode for the sprite.
    Please check SpriteRender::DrawMode for more details.
*/
void SpriteRender::setDrawMode(int mode) {
    p_ptr->m_drawMode = mode;
    p_ptr->composeMesh();
}
/*!
    Returns the order layer for the sprite.
*/
int SpriteRender::layer() const {
    return p_ptr->m_layer;
}
/*!
    Sets the order \a layer for the sprite.
*/
void SpriteRender::setLayer(int layer) {
    p_ptr->m_layer = layer;
}
/*!
    \internal
*/
void SpriteRender::loadUserData(const VariantMap &data) {
    Component::loadUserData(data);
    {
        auto it = data.find(MATERIAL);
        if(it != data.end()) {
            setMaterial(Engine::loadResource<Material>((*it).second.toString()));
        }
    }
    {
        auto it = data.find(BASEMAP);
        if(it != data.end()) {
            setSprite(Engine::loadResource<Sprite>((*it).second.toString()));
        }
    }
}
/*!
    \internal
*/
VariantMap SpriteRender::saveUserData() const {
    VariantMap result = Component::saveUserData();
    {
        Material *m = material();
        string ref = Engine::reference(m);
        if(!ref.empty()) {
            result[MATERIAL] = ref;
        }
    }
    {
        Sprite *t = sprite();
        string ref = Engine::reference(t);
        if(!ref.empty()) {
            result[BASEMAP] = ref;
        }
    }
    return result;
}
/*!
    \internal
*/
bool SpriteRender::composeMesh(Sprite *sprite, int key, Mesh *spriteMesh, Vector2 &size, int mode, bool resetSize, float scale) {
    if(mode == Sliced || mode == Tiled) {
        if(!sprite) {
            return false;
        }
        Mesh *m = sprite->mesh(key);
        if(m) {
            spriteMesh->setVertices(m->vertices());
            spriteMesh->setIndices(m->indices());
            spriteMesh->setColors(m->colors());
            spriteMesh->setUv0(m->uv0());

            Vector3Vector &verts = spriteMesh->vertices();

            Vector3 delta(verts[15] * scale - verts[0] * scale);
            if(resetSize) {
                size = Vector2(delta.x, delta.y);
            }

            if(mode == Sliced && !composeSliced(spriteMesh, size, delta, scale)) {
                return false;
            } else if(mode == Tiled && !composeTiled(spriteMesh, size, delta, scale)) {
                return false;
            }

            if(spriteMesh->colors().empty()) {
                spriteMesh->setColors(Vector4Vector(spriteMesh->vertices().size(), Vector4(1.0f)));
            }

            spriteMesh->recalcBounds();
            return true;
        }
    } else if(mode == Simple) {
        if(sprite) {
            Mesh *m = sprite->mesh(key);
            if(m) {
                spriteMesh->setVertices(m->vertices());
                spriteMesh->setIndices(m->indices());
                spriteMesh->setColors(m->colors());
                spriteMesh->setUv0(m->uv0());
                spriteMesh->recalcBounds();
            } else {
                return false;
            }
        } else {
            spriteMesh->setVertices({
                {  0.0f,   0.0f, 0.0f},
                {  0.0f, size.y, 0.0f},
                {size.x, size.y, 0.0f},
                {size.x,   0.0f, 0.0f},
            });
            spriteMesh->setUv0({
                {0.0f, 0.0f},
                {0.0f, 1.0f},
                {1.0f, 1.0f},
                {1.0f, 0.0f},
            });
            spriteMesh->setColors({
                {1.0f, 1.0f, 1.0f, 1.0f},
                {1.0f, 1.0f, 1.0f, 1.0f},
                {1.0f, 1.0f, 1.0f, 1.0f},
                {1.0f, 1.0f, 1.0f, 1.0f},
            });
            spriteMesh->setIndices({0, 1, 2, 0, 3, 2});
            spriteMesh->recalcBounds();
        }
        return true;
    }
    return false;
}

bool SpriteRender::composeSliced(Mesh *mesh, Vector2 &size, Vector3 &delta, float scale) {
    Vector3Vector &verts = mesh->vertices();

    if(scale != 1.0f) {
        for(int i = 0; i < 16; i++) {
            verts[i] *= scale;
        }
    }

    Vector2 scl(size.x / delta.x, size.y / delta.y);
    {
        float wl = (verts[1].x - verts[0].x);
        float wr = (verts[3].x - verts[2].x);
        float borders = wl + wr;

        float vl = verts[0].x * scl.x;
        float vr = verts[3].x * scl.x;

        verts[ 0].x = vl; verts[ 3].x = vr;
        verts[ 4].x = vl; verts[ 7].x = vr;
        verts[ 8].x = vl; verts[11].x = vr;
        verts[12].x = vl; verts[15].x = vr;

        float dl = verts[0].x + MIN(size.x * (wl / borders), wl);
        float dr = verts[3].x - MIN(size.x * (wr / borders), wr);

        verts[ 1].x = dl; verts[ 2].x = dr;
        verts[ 5].x = dl; verts[ 6].x = dr;
        verts[ 9].x = dl; verts[10].x = dr;
        verts[13].x = dl; verts[14].x = dr;
    }
    {
        float hb = (verts[ 4].y - verts[0].y);
        float ht = (verts[12].y - verts[8].y);
        float borders = hb + ht;

        float vb = verts[ 0].y * scl.y;
        float vt = verts[12].y * scl.y;

        verts[ 0].y = vb; verts[12].y = vt;
        verts[ 1].y = vb; verts[13].y = vt;
        verts[ 2].y = vb; verts[14].y = vt;
        verts[ 3].y = vb; verts[15].y = vt;

        float db = verts[ 0].y + MIN(size.y * (hb / borders), hb);
        float dt = verts[12].y - MIN(size.y * (ht / borders), ht);

        verts[ 4].y = db; verts[ 8].y = dt;
        verts[ 5].y = db; verts[ 9].y = dt;
        verts[ 6].y = db; verts[10].y = dt;
        verts[ 7].y = db; verts[11].y = dt;
    }
    return true;
}

bool SpriteRender::composeTiled(Mesh *mesh, Vector2 &size, Vector3 &delta, float scale) {
    Vector3Vector &verts = mesh->vertices();
    Vector2Vector &uvs = mesh->uv0();
    IndexVector &indices = mesh->indices();

    Vector2 ubl(uvs[0]);
    Vector2 utr(uvs[15]);

    int width = ceilf(size.x / delta.x);
    int height = ceilf(size.y / delta.y);

    if(width == 0 || height == 0) {
        return false;
    }

    verts.resize(width * height * 4);
    uvs.resize(width * height * 4);
    indices.resize(width * height * 6);

    Vector3 bl(Vector3(size, 0.0f) * -0.5f);

    int i = 0;
    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            int index = (y * width + x) * 4;

            Vector2 f(1.0f);
            if(x == width - 1) {
                f.x = MIN((size.x * 0.5f - bl.x) / delta.x, 1.0f);
            }
            if(y == height - 1) {
                f.y = MIN((size.y * 0.5f - bl.y) / delta.y, 1.0f);
            }

            verts[index] = bl;
            verts[index + 1] = bl + Vector3(delta.x * f.x, 0.0f, 0.0f);
            verts[index + 2] = bl + Vector3(delta.x * f.x, delta.y * f.y, 0.0f);
            verts[index + 3] = bl + Vector3(0.0f, delta.y * f.y, 0.0f);

            uvs[index] = ubl;
            uvs[index + 1] = ubl + Vector2((utr.x - ubl.x) * f.x, 0.0f);
            uvs[index + 2] = ubl + Vector2((utr.x - ubl.x) * f.x, (utr.y - ubl.y) * f.y);
            uvs[index + 3] = ubl + Vector2(0.0f, (utr.y - ubl.y) * f.y);

            indices[i]     = index;
            indices[i + 1] = index + 1;
            indices[i + 2] = index + 2;
            indices[i + 3] = index;
            indices[i + 4] = index + 2;
            indices[i + 5] = index + 3;

            bl.x += delta.x;

            i += 6;
        }
        bl.x = size.x * -0.5f;
        bl.y += delta.y;
    }
    return true;
}

void SpriteRender::composeComponent() {
    setMaterial(Engine::loadResource<Material>(DEFAULTSPRITE));
}

int SpriteRender::priority() const {
    return p_ptr->m_layer;
}
