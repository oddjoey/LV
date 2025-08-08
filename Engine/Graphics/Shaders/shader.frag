#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(set = 1, binding = 0) uniform sampler2D textures[];

layout(push_constant) uniform PushData {
    uint textureIndex;
};

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(textures[nonuniformEXT(textureIndex)], fragTexCoord);
    //outColor = vec4(1, 1, 1, 1);
}