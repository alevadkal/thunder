<Shader>
    <Properties>
        <Property name="rgbMap" type="texture2D" binding="1" target="true"/>
    </Properties>
    <Fragment>
<![CDATA[
#version 450 core

#include "ShaderLayout.h"

#define FXAA_SPAN_MAX 16.0
#define FXAA_REDUCE_MUL   (1.0/FXAA_SPAN_MAX)
#define FXAA_REDUCE_MIN   (1.0/128.0)
#define FXAA_SUBPIX_SHIFT (1.0/4.0)

layout(binding = UNIFORM + 1) uniform sampler2D rgbMap;

layout(location = 1) in vec2 _uv0;

layout(location = 0) out vec3 rgb;

void main (void) {
    vec2 uv1 = _uv0 - (g.cameraScreen.zw * (0.5 + FXAA_SUBPIX_SHIFT));
        
    vec3 rgbNW = texture( rgbMap, uv1.xy ).xyz;
    vec3 rgbNE = texture( rgbMap, uv1.xy + vec2(g.cameraScreen.z, 0.0) ).xyz;
    vec3 rgbSW = texture( rgbMap, uv1.xy + vec2(0.0, g.cameraScreen.w) ).xyz;
    vec3 rgbSE = texture( rgbMap, uv1.xy + vec2(g.cameraScreen.z, g.cameraScreen.w) ).xyz;
    vec3 rgbM  = texture( rgbMap, _uv0 ).xyz;

    float lumaNW = luminanceApprox(rgbNW);
    float lumaNE = luminanceApprox(rgbNE);
    float lumaSW = luminanceApprox(rgbSW);
    float lumaSE = luminanceApprox(rgbSE);
    float lumaM  = luminanceApprox(rgbM);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max( (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN );
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

    dir = min(vec2( FXAA_SPAN_MAX,  FXAA_SPAN_MAX),
          max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
          dir * rcpDirMin)) * g.cameraScreen.zw;

    vec3 rgbA = (1.0/2.0) * (
        textureLod(rgbMap, _uv0.xy + dir * (1.0/3.0 - 0.5), 0.0).xyz +
        textureLod(rgbMap, _uv0.xy + dir * (2.0/3.0 - 0.5), 0.0).xyz);

    vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (
        textureLod(rgbMap, _uv0.xy + dir * (0.0/3.0 - 0.5), 0.0).xyz +
        textureLod(rgbMap, _uv0.xy + dir * (3.0/3.0 - 0.5), 0.0).xyz);

    float lumaB = luminanceApprox(rgbB);

    if((lumaB < lumaMin) || (lumaB > lumaMax)) {
        rgb = rgbA;
    } else {
        rgb = rgbB;
    }
}
]]>
    </Fragment>
    <Pass type="PostProcess" blendMode="Opaque" lightModel="Unlit" depthTest="false" depthWrite="false" twoSided="true"/>
</Shader>
