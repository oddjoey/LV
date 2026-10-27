#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragWorldPosition;

layout(set = 1, binding = 0) uniform sampler2D textures[];

#include "scene_data.glsl"
layout(set = 2, binding = 1) uniform sampler2DArrayShadow shadowMap;
// One layer per shadowed point light.
layout(set = 2, binding = 2) uniform sampler2DArrayShadow lightShadowMaps;

// Must match ObjectPushConstants in Utility.h.
layout(push_constant) uniform PushData {
    vec4 tint;
    float emissive;
    uint textureIndex;
    uint shading;           // 0 lit, 1 unlit, 2 sky
};

layout(location = 0) out vec4 outColor;
#ifdef MOTION_VECTORS
layout(location = 4) in vec4 fragClipPosition;
layout(location = 5) in vec4 fragPreviousClipPosition;
// How far this pixel's surface moved since last frame, towards where it was, in screen UV.
layout(location = 1) out vec2 outMotion;
#endif

const uint SHADING_UNLIT = 1u;
const uint SHADING_SKY = 2u;

// Narkowicz's ACES fit: keeps bright sunlight from clipping to flat white.
vec3 Tonemap(vec3 color) {
    color *= scene.cameraPosition.w;
    return clamp((color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14), 0.0, 1.0);
}

vec3 SkyColor(vec3 direction) {
    float up = direction.y;
    vec3 sky = mix(scene.skyHorizon.rgb, scene.skyZenith.rgb, pow(clamp(up, 0.0, 1.0), 0.45));
    // Below the horizon the sky fades into ground haze.
    vec3 haze = mix(scene.skyHorizon.rgb, scene.groundColor.rgb, 0.5);
    sky = mix(sky, haze, clamp(-up * 6.0, 0.0, 1.0));

    // Sun disc and the glow around it.
    float sunDot = max(dot(direction, scene.sunDirection.xyz), 0.0);
    float aboveHorizon = smoothstep(-0.03, 0.02, up);
    sky += scene.sunColor.rgb * (smoothstep(0.9995, 0.9998, sunDot) * 12.0 + pow(sunDot, 16.0) * 0.15) * aboveHorizon;
    return sky;
}

bool Inside(vec3 position) {
    return scene.interiorAmbient.w > 0.5
        && all(greaterThan(position, scene.interiorMin.xyz))
        && all(lessThan(position, scene.interiorMax.xyz));
}

// 3x3 taps of the hardware 2x2 comparison filter.
float SampleCascade(int cascade, vec2 uv, float depth) {
    float texel = scene.shadowParams.w;
    float lit = 0.0;
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
            lit += texture(shadowMap, vec4(uv + vec2(x, y) * texel, float(cascade), depth));
    return lit / 9.0;
}

// How much sun reaches the point: 0 shadowed to 1 lit, or -1 when no cascade covers it.
float SunShadow(vec3 position, vec3 normal, float facing) {
    for (int cascade = 0; cascade < SHADOW_CASCADES; cascade++) {
        // Look up a little off the surface, more on surfaces turned away from the sun, so a
        // surface doesn't shadow itself.
        float texelWorld = cascade == 0 ? scene.shadowParams.x : scene.shadowParams.y;
        vec3 offsetPosition = position + normal * texelWorld * (1.5 + 2.5 * (1.0 - facing));
        vec3 projected = (scene.lightViewProj[cascade] * vec4(offsetPosition, 1.0)).xyz;
        vec2 uv = projected.xy * 0.5 + 0.5;
        float margin = 2.0 * scene.shadowParams.w;
        if (all(greaterThan(uv, vec2(margin))) && all(lessThan(uv, vec2(1.0 - margin))) && projected.z > 0.0 && projected.z < 1.0)
            return SampleCascade(cascade, uv, projected.z);
    }
    return -1.0;
}

// How much of a shadowed lamp's light reaches the point: 0 shadowed to 1 lit. Outside the lamp's
// shadow cone it's lit.
float LampShadow(int light, vec3 position, vec3 normal, float distance, float facing) {
    vec4 shadow = scene.lightShadow[light];
    int layer = int(shadow.x + 0.5);
    if (shadow.x < 0.0)
        return 1.0;

    // A shadow texel covers more surface the further it is from the lamp, so the offset off the
    // surface grows with distance (and on surfaces turned away from the light).
    float texelWorld = shadow.y * distance;
    vec3 offsetPosition = position + normal * texelWorld * (1.5 + 2.5 * (1.0 - facing));
    vec4 clip = scene.lightShadowViewProj[layer] * vec4(offsetPosition, 1.0);
    if (clip.w <= 0.0)
        return 1.0;
    vec3 projected = clip.xyz / clip.w;
    vec2 uv = projected.xy * 0.5 + 0.5;
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))) || projected.z <= 0.0 || projected.z >= 1.0)
        return 1.0;

    float texel = shadow.z;
    float lit = 0.0;
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
            lit += texture(lightShadowMaps, vec4(uv + vec2(x, y) * texel, float(layer), projected.z));
    return lit / 9.0;
}

// Lamps, with a smooth cut-off at their range. Shadowed ones keep a little of their light in
// shadow, standing in for what bounces off the walls and floor.
vec3 PointLights(vec3 position, vec3 normal, bool inside) {
    vec3 total = vec3(0.0);
    for (int i = 0; i < scene.lightCount.x; i++) {
        vec4 positionRange = scene.lightPositionRange[i];
        vec4 colorZone = scene.lightColorZone[i];
        int zone = int(colorZone.w + 0.5);
        if ((zone == 1 && !inside) || (zone == 2 && inside))
            continue;

        vec3 toLight = positionRange.xyz - position;
        float distance = length(toLight);
        if (distance >= positionRange.w)
            continue;
        float ratio = distance / positionRange.w;
        float window = clamp(1.0 - ratio * ratio * ratio * ratio, 0.0, 1.0);
        float attenuation = window * window / (1.0 + distance * distance);
        float facing = max(dot(normal, toLight / max(distance, 1e-4)), 0.0);
        float shadow = LampShadow(i, position, normal, distance, facing);
        total += colorZone.rgb * attenuation * (facing * 0.85 + 0.15) * mix(0.06, 1.0, shadow);
    }
    return total;
}

vec4 Shade() {
    if (shading == SHADING_SKY) {
        vec3 direction = normalize(fragWorldPosition - scene.cameraPosition.xyz);
        return vec4(Tonemap(SkyColor(direction)), 1.0);
    }

    vec3 albedo = fragColor * tint.rgb;
    // With DLSS the scene renders smaller, so textures are biased to the mip the window would use.
    if (textureIndex != 0)
        albedo *= texture(textures[nonuniformEXT(textureIndex) - 1], fragTexCoord, scene.renderParams.z).rgb;

    if (shading == SHADING_UNLIT)
        return vec4(Tonemap(albedo * max(emissive, 1.0)), tint.a);

    vec3 normal = normalize(fragNormal);
    if (!gl_FrontFacing)
        normal = -normal;   // the back of a two-sided pane
    vec3 position = fragWorldPosition;
    bool inside = Inside(position);

    // Sun, through the shadow map. Without shadows the roof can't block it, so indoors gets none.
    vec3 sunDirection = scene.sunDirection.xyz;
    float sunFacing = dot(normal, sunDirection);
    float shadow = inside ? 0.0 : 1.0;
    if (scene.shadowParams.z > 0.5) {
        float sampled = SunShadow(position, normal, clamp(sunFacing, 0.0, 1.0));
        if (sampled >= 0.0)
            shadow = sampled;
    }
    vec3 sun = scene.sunColor.rgb * max(sunFacing, 0.0) * shadow;

    vec3 ambient;
    if (inside)
        ambient = scene.interiorAmbient.rgb * (0.8 + 0.2 * normal.y);
    else
        ambient = mix(scene.groundColor.rgb, mix(scene.skyHorizon.rgb, scene.skyZenith.rgb, 0.6), normal.y * 0.5 + 0.5);

    vec3 lamps = PointLights(position, normal, inside);

    // A soft sun highlight so metal and glass catch the light.
    vec3 toCamera = scene.cameraPosition.xyz - position;
    float cameraDistance = length(toCamera);
    vec3 halfway = normalize(sunDirection + toCamera / max(cameraDistance, 1e-4));
    float specular = pow(max(dot(normal, halfway), 0.0), 48.0) * 0.12 * shadow * step(0.0, sunFacing);

    vec3 lit = albedo * (ambient + sun + lamps) + scene.sunColor.rgb * specular + albedo * emissive;

    float fog = clamp(1.0 - exp(-scene.fogColor.w * cameraDistance), 0.0, 1.0);
    lit = mix(lit, scene.fogColor.rgb, fog);

    return vec4(Tonemap(lit), tint.a);
}

void main() {
    outColor = Shade();
#ifdef MOTION_VECTORS
    // Both positions without this frame's jitter, so only real movement remains.
    vec2 current = fragClipPosition.xy / fragClipPosition.w - scene.renderParams.xy;
    vec2 previous = fragPreviousClipPosition.xy / fragPreviousClipPosition.w;
    outMotion = (previous - current) * 0.5;
#endif
}
