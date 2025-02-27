#ifndef PIPELINECONTEXT
#define PIPELINECONTEXT

#include <cstdint>
#include <unordered_map>

#include <amath.h>

#include "resource.h"

class CommandBuffer;

class Scene;
class Camera;

class Mesh;
class MaterialInstance;
class Texture;
class RenderTarget;
class PipelinePass;
class GuiLayer;

class Widget;
class BaseLight;
class Renderable;
class PostProcessVolume;
class PostProcessSettings;

class ENGINE_EXPORT PipelineContext : public Object {
    A_REGISTER(PipelineContext, Object, System)

public:
    PipelineContext();
    ~PipelineContext();

    CommandBuffer *buffer() const;

    void analizeGraph(World *world);

    void draw(Camera *camera);

    void cameraReset();

    void drawRenderers(uint32_t layer, const list<Renderable *> &list);

    void setMaxTexture(uint32_t size);

    RenderTarget *defaultTarget();
    void setDefaultTarget(RenderTarget *target);

    Texture *textureBuffer(const string &string);

    void insertRenderPass(PipelinePass *pass, PipelinePass *before = nullptr);

    const list<PipelinePass *> &renderPasses() const;

    list<string> renderTextures() const;

    list<Renderable *> &sceneComponents();
    list<Renderable *> &culledComponents();
    list<BaseLight *> &sceneLights();
    list<Widget *> &uiComponents();

    list<Renderable *> frustumCulling(const array<Vector3, 8> &frustum, list<Renderable *> &list, AABBox &bb);

    AABBox worldBound() const;

    void setCurrentCamera(Camera *camera);
    Camera *currentCamera() const;

    void showUiAsSceneView();

    void resize(int32_t width, int32_t height);

    static Mesh *defaultPlane();

protected:
    typedef map<string, Texture *> BuffersMap;
    typedef map<string, RenderTarget *> TargetsMap;

    Matrix4 m_cameraView;
    Matrix4 m_cameraProjection;

    AABBox m_worldBound;

    list<Renderable *> m_sceneComponents;
    list<Renderable *> m_culledComponents;
    list<BaseLight *> m_sceneLights;
    list<Widget *> m_uiComponents;

    BuffersMap m_textureBuffers;

    list<PipelinePass *> m_renderPasses;

    CommandBuffer *m_buffer;

    PostProcessSettings *m_postProcessSettings;

    MaterialInstance *m_finalMaterial;

    RenderTarget *m_defaultTarget;

    Texture *m_final;

    Camera *m_camera;

    GuiLayer *m_guiLayer;

    int32_t m_width;
    int32_t m_height;

    bool m_frustumCulling;

};

#endif // PIPELINECONTEXT
