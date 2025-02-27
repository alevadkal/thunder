<Shader>
    <Properties>
        <Property name="matrix" type="mat4"/>
        <Property name="tiles" type="vec4"/>
        <Property name="color" type="vec4"/>
        <Property name="params" type="vec4"/>
        <Property name="bias" type="vec4"/>
        <Property name="position" type="vec4"/>
        <Property name="direction" type="vec4"/>
        <Property name="shadows" type="float"/>
        <Property name="normalsMap" type="texture2D" binding="1" target="true"/>
        <Property name="diffuseMap" type="texture2D" binding="2" target="true"/>
        <Property name="paramsMap" type="texture2D" binding="3" target="true"/>
        <Property name="depthMap" type="texture2D" binding="4" target="true"/>
        <Property name="shadowMap" type="texture2D" binding="5" target="true"/>
    </Properties>
    <Fragment>
<![CDATA[
#version 450 core

#include "ShaderLayout.h"
#include "BRDF.h"

layout(binding = UNIFORM) uniform Uniforms {
    mat4 matrix;
    vec4 tiles;
    vec4 color;
    vec4 params; // x - brightness, y - radius/width, z - length/height, w - cutoff
    vec4 bias;
    vec4 position;
    vec4 direction;
    float shadows;
} uni;

layout(binding = UNIFORM + 1) uniform sampler2D normalsMap;
layout(binding = UNIFORM + 2) uniform sampler2D diffuseMap;
layout(binding = UNIFORM + 3) uniform sampler2D paramsMap;
layout(binding = UNIFORM + 4) uniform sampler2D depthMap;
layout(binding = UNIFORM + 5) uniform sampler2D shadowMap;

layout(location = 0) in vec4 _vertex;

layout(location = 0) out vec4 rgb;

void main (void) {
    vec2 proj = ((_vertex.xyz / _vertex.w) * 0.5 + 0.5).xy;

    vec4 slice0 = texture(normalsMap,  proj);

    // Light model LIT
    if(slice0.w > 0.33) {
        float depth = texture(depthMap, proj).x;
        vec3 world = getWorld(g.cameraScreenToWorld, proj, depth);

        vec3 n = normalize(slice0.xyz * 2.0 - 1.0);

        vec3 dir = uni.position.xyz - world;
        vec3 l = normalize(dir);
        float dist = length(dir);

        float spot = dot(l, uni.direction.xyz);
        float fall = 0.0;
        if(spot > uni.params.w) {
            fall = 1.0 - (1.0 - spot) / (1.0 - uni.params.w);
            fall = getAttenuation(dist, uni.params.y) * uni.params.x * fall;
        }

        vec4 slice1 = texture(paramsMap, proj);
        float rough = slice1.x;
        float metal = slice1.z;
        float spec  = slice1.w;

        vec4 slice2 = texture(diffuseMap, proj);
        vec3 albedo = slice2.xyz;

        vec3 v = normalize(g.cameraPosition.xyz - world);
        vec3 h = normalize(l + v);

        float cosTheta = clamp(dot(l, n), 0.0, 1.0);

        float shadow = 1.0;
        if(uni.shadows == 1.0) {
            vec4 offset = uni.tiles;
            vec4 proj   = uni.matrix * vec4(world, 1.0);
            vec3 coord  = (proj.xyz / proj.w);
            shadow = getShadow(shadowMap, (coord.xy * offset.zw) + offset.xy, coord.z - uni.bias.x);
        }

        vec3 refl = mix(vec3(spec), albedo, metal) * getCookTorrance(n, v, h, cosTheta, rough);
        vec3 result = albedo * (1.0 - metal) + refl;
        float diff = max(PI * getLambert(cosTheta, uni.params.x) * fall * shadow, 0.0);

        rgb = vec4(uni.color.xyz * result * diff, 1.0);
        return;
    }
    rgb = vec4(vec3(0.0), 1.0);
}
]]>
    </Fragment>
    <Pass type="LightFunction" blendMode="Additive" lightModel="Unlit" depthTest="false" depthWrite="false" twoSided="false"/>
</Shader>
