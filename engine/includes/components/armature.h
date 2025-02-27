#ifndef ARMATURE_H
#define ARMATURE_H

#include "nativebehaviour.h"

class Texture;
class Pose;
class ArmaturePrivate;

class ENGINE_EXPORT Armature : public NativeBehaviour {
    A_REGISTER(Armature, NativeBehaviour, Components/Animation);

    A_PROPERTIES(
        A_PROPERTYEX(Pose *, bindPose, Armature::bindPose, Armature::setBindPose, "editor=Asset")
    )
    A_NOMETHODS()

public:
    Armature();
    ~Armature() override;

    Pose *bindPose() const;
    void setBindPose(Pose *pose);

private:
    void update() override;

    void loadUserData(const VariantMap &data) override;
    VariantMap saveUserData() const override;

    Texture *texture() const;

    AABBox recalcBounds(const AABBox &aabb) const;

    void drawGizmosSelected() override;

private:
    friend class SkinnedMeshRender;

    ArmaturePrivate *p_ptr;
};

#endif // ARMATURE_H
