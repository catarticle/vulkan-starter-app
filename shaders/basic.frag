#version 450

// Входные данные из вершинного шейдера
layout(location = 0) in vec3 fragWorldPosition;
layout(location = 1) in vec3 fragWorldNormal;

// Итоговый цвет фрагмента
layout(location = 0) out vec4 outColor;

// Общие данные сцены
layout(std140, set = 0, binding = 0) uniform SceneUniforms {
    mat4 view_projection;
    vec3 camera_position;
    vec3 directional_light_direction;
    vec3 directional_light_color;
    vec3 ambient_light;
    uint point_light_count;
    uint spot_light_count;
} scene_uniforms;

// Данные материала модели
layout(std140, set = 1, binding = 0) uniform ModelUniform {
    mat4 model;
    vec3 albedo;
    vec3 specular;
    float alpha;
} model_uniforms;

void main() {
    // Нормализуем нормаль поверхности
    vec3 n = normalize(fragWorldNormal);

    // Направление от поверхности к источнику света
    vec3 lightVector =
        -scene_uniforms.directional_light_direction;

    float lightLength = length(lightVector);

    vec3 l = lightLength > 0.0001
        ? lightVector / lightLength
        : vec3(0.0);

    // Направление от поверхности к камере
    vec3 v = normalize(
        scene_uniforms.camera_position - fragWorldPosition
    );

    // Диффузная составляющая по закону Ламберта
    float diffuseFactor = max(dot(n, l), 0.0);

    vec3 diffuse =
        model_uniforms.albedo * diffuseFactor;

    // Зеркальная составляющая по Блинну — Фонгу
    float specularFactor = 0.0;

    if (diffuseFactor > 0.0) {
        // Половинчатый вектор между светом и наблюдателем
        vec3 sum = l + v;

        if (length(sum) > 0.0001) {
            vec3 h = normalize(sum);

            specularFactor = pow(
                max(dot(n, h), 0.0),
                model_uniforms.alpha
            );
        }
    }

    vec3 specular =
        model_uniforms.specular * specularFactor;

    // Суммируем диффузное и зеркальное отражения
    vec3 directionalLight =
        scene_uniforms.directional_light_color *
        (diffuse + specular);

    // Фоновое освещение
    vec3 ambient =
        scene_uniforms.ambient_light *
        model_uniforms.albedo;

    // Итоговый цвет по модели Блинна — Фонга
    vec3 result = ambient + directionalLight;

    outColor = vec4(result, 1.0);
}
