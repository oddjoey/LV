// The per-frame scene block, shared by shader.vert and shader.frag. Must match SceneUniforms in
// Lighting.h (std140).
const int MAX_POINT_LIGHTS = 32;
const int SHADOW_CASCADES = 2;
const int MAX_SHADOWED_LIGHTS = 8;
const int MAX_INTERIORS = 4;
layout(set = 2, binding = 0) uniform SceneData {
    mat4 lightViewProj[SHADOW_CASCADES];
    vec4 shadowParams;      // xy: shadow texel size in metres per cascade, z: shadows on, w: 1 / resolution
    vec4 cameraPosition;    // w: exposure
    vec4 sunDirection;
    vec4 sunColor;
    vec4 skyZenith;
    vec4 skyHorizon;
    vec4 groundColor;
    vec4 interiorAmbient;   // w: how many interiors there are
    vec4 interiorMin[MAX_INTERIORS];
    vec4 interiorMax[MAX_INTERIORS];
    vec4 fogColor;          // w: density
    ivec4 lightCount;
    vec4 lightPositionRange[MAX_POINT_LIGHTS];
    vec4 lightColorZone[MAX_POINT_LIGHTS];
    mat4 previousViewProj;  // last frame's, without jitter: for motion vectors
    vec4 renderParams;      // xy: this frame's jitter in clip space, z: texture LOD bias
    mat4 lightShadowViewProj[MAX_SHADOWED_LIGHTS];
    vec4 lightShadow[MAX_POINT_LIGHTS];   // x: shadow map layer or -1, y: shadow texel size per metre of distance, z: 1 / resolution
    mat4 view;
    mat4 proj;              // with this frame's jitter
} scene;
