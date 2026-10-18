#version 450
layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 fragColor;

layout(location = 1) out vec3 fragPosition;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 mvp;
    vec4 tint;
} ubo;

void main() {
    gl_Position = ubo.mvp * vec4(inPosition, 1.0);

    fragColor = (inPosition * 0.5 + 0.5) * ubo.tint.rgb;

    fragPosition = inPosition;
}