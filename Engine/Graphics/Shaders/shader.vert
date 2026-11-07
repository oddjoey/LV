#version 450
#extension GL_GOOGLE_include_directive : require

#include "instance_data.glsl"
#include "scene_data.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragWorldPosition;
#ifdef MOTION_VECTORS
// Where the vertex is on screen this frame and was last frame, for DLSS's motion vectors.
layout(location = 4) out vec4 fragClipPosition;
layout(location = 5) out vec4 fragPreviousClipPosition;
#endif
// The object's own shading, the same across the whole of it.
layout(location = 6) flat out vec4 fragTint;
layout(location = 7) flat out uvec2 fragTextureShading;
layout(location = 8) flat out float fragEmissive;

void main() {
    InstanceData object = instances[slots[gl_InstanceIndex]];
    vec4 worldPosition = object.model * vec4(inPosition, 1.0);
    gl_Position = scene.proj * scene.view * worldPosition;
    // In front of everything: its depth squeezed up against the near plane, so it passes the depth
    // test against the world but still sorts against itself.
    if ((object.flags & INSTANCE_IN_FRONT) != 0u)
        gl_Position.z *= 0.01;
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    // Good enough for the uniform and near-uniform scales the engine uses.
    fragNormal = mat3(object.model) * inNormal;
    fragWorldPosition = worldPosition.xyz;
#ifdef MOTION_VECTORS
    fragClipPosition = gl_Position;
    fragPreviousClipPosition = scene.previousViewProj * object.previousModel * vec4(inPosition, 1.0);
#endif
    fragTint = object.tint;
    fragTextureShading = uvec2(object.textureIndex, object.shading);
    fragEmissive = object.emissive;
}
