// Every object's data, and the frame's lists of which ones each pass draws. Shared by shader.vert and
// shadow.vert. InstanceData must match InstanceData in Utility.h (std430).
struct InstanceData {
    mat4 model;
    mat4 previousModel;     // last frame's, for motion vectors
    vec4 tint;
    float emissive;
    uint textureIndex;
    uint shading;           // 0 lit, 1 unlit, 2 sky
    uint flags;             // INSTANCE_IN_FRONT
};
const uint INSTANCE_IN_FRONT = 1u;

layout(std430, set = 0, binding = 0) readonly buffer Instances {
    InstanceData instances[];
};

// A draw instances a run of these, one per object of the same model: gl_InstanceIndex (which counts
// from the draw's first instance) picks the object's slot.
layout(std430, set = 0, binding = 1) readonly buffer DrawList {
    uint slots[];
};
