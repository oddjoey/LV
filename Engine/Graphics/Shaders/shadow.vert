#version 450
#extension GL_GOOGLE_include_directive : require

// Depth-only pass for the sun's and lamps' shadow maps. The light's matrix is a push constant, set
// once per pass; each object's comes from the instance buffer, so a map draws every object of a
// model in one instanced draw.
#include "instance_data.glsl"

layout(push_constant) uniform ShadowPush {
    mat4 lightViewProj;
};

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = lightViewProj * instances[slots[gl_InstanceIndex]].model * vec4(inPosition, 1.0);
}
