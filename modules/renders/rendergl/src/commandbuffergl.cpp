#include "commandbuffergl.h"

#include <cstring>

#include "agl.h"

#include "resources/meshgl.h"
#include "resources/materialgl.h"
#include "resources/rendertargetgl.h"
#include "resources/computeshadergl.h"

#include <log.h>
#include <timer.h>

CommandBufferGL::CommandBufferGL() :
        m_globalUbo(0),
        m_localUbo(0) {
    PROFILE_FUNCTION();

}

CommandBufferGL::~CommandBufferGL() {

}

void CommandBufferGL::begin() {
    PROFILE_FUNCTION();

    if(m_globalUbo == 0) {
        glGenBuffers(1, &m_globalUbo);
        glBindBuffer(GL_UNIFORM_BUFFER, m_globalUbo);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(Global), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    glBindBufferBase(GL_UNIFORM_BUFFER, GLOBAL_BIND, m_globalUbo);

    m_global.time = Timer::time();
    m_global.deltaTime = Timer::deltaTime();
    m_global.clip = 0.99f;

    glBindBuffer(GL_UNIFORM_BUFFER, m_globalUbo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Global), &m_globalUbo);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    if(m_localUbo == 0) {
        glGenBuffers(1, &m_localUbo);
        glBindBuffer(GL_UNIFORM_BUFFER, m_localUbo);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(Local), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        glBindBufferBase(GL_UNIFORM_BUFFER, LOCAL_BIND, m_localUbo);
    }
}

void CommandBufferGL::clearRenderTarget(bool clearColor, const Vector4 &color, bool clearDepth, float depth) {
    PROFILE_FUNCTION();

    uint32_t flags = 0;
    if(clearColor) {
        flags |= GL_COLOR_BUFFER_BIT;
        glClearColor(color.x, color.y, color.z, color.w);
    }
    if(clearDepth) {
        glDepthMask(GL_TRUE);
        flags |= GL_DEPTH_BUFFER_BIT;
        glClearDepthf(depth);
    }
    glClear(flags);
}

void CommandBufferGL::dispatchCompute(ComputeInstance *shader, int32_t groupsX, int32_t groupsY, int32_t groupsZ) {
#ifndef THUNDER_MOBILE
    PROFILE_FUNCTION();
    if(shader) {
        ComputeInstanceGL *instance = static_cast<ComputeInstanceGL *>(shader);
        if(instance->bind(this)) {
            glDispatchCompute(groupsX, groupsY, groupsZ);

            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        }
    }
#endif
}

void CommandBufferGL::drawMesh(const Matrix4 &model, Mesh *mesh, uint32_t sub, uint32_t layer, MaterialInstance *material) {
    PROFILE_FUNCTION();
    A_UNUSED(sub);

    if(mesh && material) {
        MeshGL *m = static_cast<MeshGL *>(mesh);

        MaterialInstanceGL *instance = static_cast<MaterialInstanceGL *>(material);
        if(instance->bind(this, layer)) {
            m_local.model = model;

            glBindBuffer(GL_UNIFORM_BUFFER, m_localUbo);
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Local), &m_local);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);

            m->bindVao(this);

            if(m->indices().empty()) {
                uint32_t vert = m->vertices().size();
                int32_t glMode = (material->material()->wireframe()) ? GL_LINE_STRIP : GL_TRIANGLE_STRIP;
                glDrawArrays(glMode, 0, vert);
                PROFILER_STAT(POLYGONS, vert - 2);
            } else {
                uint32_t index = m->indices().size();
                int32_t glMode = (material->material()->wireframe()) ? GL_LINES : GL_TRIANGLES;
                glDrawElements(glMode, index, GL_UNSIGNED_INT, nullptr);
                PROFILER_STAT(POLYGONS, index / 3);
            }
            PROFILER_STAT(DRAWCALLS, 1);

            glBindVertexArray(0);
        }
    }
}

void CommandBufferGL::drawMeshInstanced(const Matrix4 *models, uint32_t count, Mesh *mesh, uint32_t sub, uint32_t layer, MaterialInstance *material) {
    PROFILE_FUNCTION();
    A_UNUSED(sub);

    if(mesh && material) {
        MeshGL *m = static_cast<MeshGL *>(mesh);

        m_local.model.identity();

        glBindBuffer(GL_UNIFORM_BUFFER, m_localUbo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Local), &m_local);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        MaterialInstanceGL *instance = static_cast<MaterialInstanceGL *>(material);
        if(instance->bind(this, layer)) {
            glBindBuffer(GL_ARRAY_BUFFER, m->instance());
            glBufferData(GL_ARRAY_BUFFER, count * sizeof(Matrix4), models, GL_DYNAMIC_DRAW);

            m->bindVao(this);

            if(m->indices().empty()) {
                int32_t glMode = (material->material()->wireframe()) ? GL_LINE_STRIP : GL_TRIANGLE_STRIP;
                uint32_t vert = m->vertices().size();
                glDrawArraysInstanced(glMode, 0, vert, count);
                PROFILER_STAT(POLYGONS, index - 2 * count);
            } else {
                uint32_t index = m->indices().size();
                int32_t glMode = (material->material()->wireframe()) ? GL_LINES : GL_TRIANGLES;
                glDrawElementsInstanced(glMode, index, GL_UNSIGNED_INT, nullptr, count);
                PROFILER_STAT(POLYGONS, (index / 3) * count);
            }
            PROFILER_STAT(DRAWCALLS, 1);

            glBindVertexArray(0);
        }
    }
}

void CommandBufferGL::setRenderTarget(RenderTarget *target, uint32_t level) {
    PROFILE_FUNCTION();

    RenderTargetGL *t = static_cast<RenderTargetGL *>(target);
    if(t) {
        t->bindBuffer(level);
    }
}

void CommandBufferGL::resetViewProjection() {
    PROFILE_FUNCTION();

    CommandBuffer::resetViewProjection();

    glBindBuffer(GL_UNIFORM_BUFFER, m_globalUbo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Global), &m_global);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void CommandBufferGL::setViewProjection(const Matrix4 &view, const Matrix4 &projection) {
    PROFILE_FUNCTION();

    CommandBuffer::setViewProjection(view, projection);

    glBindBuffer(GL_UNIFORM_BUFFER, m_globalUbo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Global), &m_global);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void CommandBufferGL::setViewport(int32_t x, int32_t y, int32_t width, int32_t height) {
    CommandBuffer::setViewport(x, y, width, height);

    glViewport(x, y, width, height);
}

void CommandBufferGL::enableScissor(int32_t x, int32_t y, int32_t width, int32_t height) {
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, width, height);
}

void CommandBufferGL::disableScissor() {
    glDisable(GL_SCISSOR_TEST);
}

void CommandBufferGL::finish() {
    glFinish();
}
