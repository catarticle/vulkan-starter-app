#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosition;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 dx = dFdx(fragPosition);
    vec3 dy = dFdy(fragPosition);

    vec3 normal = normalize(cross(dx, dy));

    vec3 light = normalize(vec3(0.4, 0.6, 1.0));

    float brightness = 0.3 + 0.7 * abs(dot(normal, light));

    outColor = vec4(fragColor * brightness, 1.0);
}
