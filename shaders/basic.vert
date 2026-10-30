#version 450

// Входные атрибуты вершины
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

// Мировая позиция вершины для фрагментного шейдера
layout(location = 0) out vec3 fragWorldPosition;

// Нормаль в мировых координатах
layout(location = 1) out vec3 fragWorldNormal;

// Общие данные сцены (набор 0)
layout(std140, set = 0, binding = 0) uniform SceneUniforms {
    mat4 view_projection;
    vec3 camera_position;
    vec3 directional_light_direction;
    vec3 directional_light_color;
    vec3 ambient_light;
    uint point_light_count;
    uint spot_light_count;
} scene_uniforms;

// Данные модели (набор 1)
layout(std140, set = 1, binding = 0) uniform ModelUniform {
    mat4 model;
    vec3 albedo;
    vec3 specular;
    float alpha;
} model_uniforms;

void main() {
    // Переводим вершину из локальных координат в мировые
    vec4 worldPosition =
        model_uniforms.model * vec4(inPosition, 1.0);

    // Преобразуем мировую позицию в координаты отсечения
    gl_Position =
        scene_uniforms.view_projection * worldPosition;

    // Передаём мировую позицию во фрагментный шейдер
    fragWorldPosition = worldPosition.xyz;

    // Преобразуем нормаль в мировые координаты
    fragWorldNormal =
        mat3(model_uniforms.model) * inNormal;
}
