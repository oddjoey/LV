#version 450

// Puts DLSS's upscaled frame on the swap chain. It is already tone mapped; the sRGB swap chain
// does the encoding.
layout(set = 0, binding = 0) uniform sampler2D upscaled;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(texture(upscaled, fragUV).rgb, 1.0);
}
