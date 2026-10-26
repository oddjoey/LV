#version 450

// Depth-only pass for the sun's shadow map. Uses the same per-object uniform slot as shader.vert.
layout(set = 0, binding = 0) uniform UniformBufferObject
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(push_constant) uniform ShadowPush {
    mat4 lightViewProj;
};

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = lightViewProj * ubo.model * vec4(inPosition, 1.0);
}
