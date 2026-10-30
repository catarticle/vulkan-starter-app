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

// Структура точечного источника света
struct PointLight {
    vec3 position;
    vec3 color;
};

// Массив точечных источников в Storage Buffer
layout(std430, set = 0, binding = 1) buffer PointLights {
    PointLight point_lights[];
};

// Структура прожекторного источника света
struct SpotLight {
    vec3 position;
    vec3 color;
    vec3 direction;
    float inner_angle;
    float outer_angle;
};

// Массив прожекторов в Storage Buffer
layout(std430, set = 0, binding = 2) readonly buffer SpotLights {
    SpotLight spot_lights[];
};

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

    // Вычисляем вклад каждого точечного источника света
    for (uint i = 0u; i < scene_uniforms.point_light_count; ++i) {

        // Вектор от фрагмента к точечному источнику
        vec3 lightVector =
            point_lights[i].position - fragWorldPosition;

        // Квадрат расстояния до источника
        float distanceSquared = dot(lightVector, lightVector);

        // Исключаем деление на ноль
        if (distanceSquared < 0.0001) {
            continue;
        }

        // Единичный вектор направления на источник
        vec3 lPoint = normalize(lightVector);

        // Диффузная составляющая по закону Ламберта
        float diffusePoint = max(dot(n, lPoint), 0.0);

        // Зеркальная составляющая по Блинну — Фонгу
        float specularPoint = 0.0;

        if (diffusePoint > 0.0) {
            vec3 halfVector = lPoint + v;

            if (length(halfVector) > 0.0001) {
                vec3 h = normalize(halfVector);

                specularPoint = pow(
                    max(dot(n, h), 0.0),
                    model_uniforms.alpha
                );
            }
        }

        // Затухание по закону обратных квадратов
        float attenuation = 1.0 / max(distanceSquared, 0.01);

        // Итоговый вклад точечного источника
        vec3 pointColor =
            point_lights[i].color * attenuation *
            (
                model_uniforms.albedo * diffusePoint +
                model_uniforms.specular * specularPoint
            );

        // Добавляем освещение к уже вычисленному цвету
        result += pointColor;
    }

    // Перебираем активные прожекторные источники
    for (uint i = 0u; i < scene_uniforms.spot_light_count; ++i) {

        // Вектор от фрагмента к прожектору
        vec3 lightVector =
            spot_lights[i].position - fragWorldPosition;

        // Квадрат расстояния до прожектора
        float distanceSquared = dot(lightVector, lightVector);

        // Исключаем нулевое расстояние
        if (distanceSquared < 0.000001) {
            continue;
        }

        // Направление от фрагмента к прожектору
        vec3 lSpot = normalize(lightVector);

        // Направление оси светового конуса
        vec3 rawDirection = spot_lights[i].direction;

        if (length(rawDirection) < 0.0001) {
            continue;
        }

        vec3 spotDirection = normalize(rawDirection);

        // Косинус угла между осью конуса и лучом к фрагменту
        float cosTheta = dot(-lSpot, spotDirection);

        // Переводим углы в радианы и вычисляем косинусы
        float cosInner = cos(
            radians(spot_lights[i].inner_angle)
        );

        float cosOuter = cos(
            radians(spot_lights[i].outer_angle)
        );

        // Вычисляем коэффициент попадания внутрь конуса
        float coneFactor;

        if (spot_lights[i].outer_angle >
            spot_lights[i].inner_angle) {

            // Плавный переход между внутренним и внешним углами
            coneFactor = smoothstep(
                cosOuter,
                cosInner,
                cosTheta
            );
        } else {
            // Резкая граница при равных углах
            coneFactor = step(cosInner, cosTheta);
        }

        // Если фрагмент вне конуса, свет отсутствует
        if (coneFactor <= 0.0) {
            continue;
        }

        // Диффузное освещение по закону Ламберта
        float diffuseSpot = max(dot(n, lSpot), 0.0);

        // Зеркальное отражение по Блинну — Фонгу
        float specularSpot = 0.0;

        if (diffuseSpot > 0.0) {
            vec3 halfVector = lSpot + v;

            if (length(halfVector) > 0.0001) {
                vec3 h = normalize(halfVector);

                specularSpot = pow(
                    max(dot(n, h), 0.0),
                    model_uniforms.alpha
                );
            }
        }

        // Затухание по закону обратных квадратов
        float attenuation = 1.0 / max(distanceSquared, 0.01);

        // Вычисляем вклад прожекторного источника
        vec3 spotColor =
            spot_lights[i].color *
            attenuation *
            coneFactor *
            (
                model_uniforms.albedo * diffuseSpot +
                model_uniforms.specular * specularSpot
            );

        // Прибавляем вклад к итоговому освещению
        result += spotColor;
    }

    outColor = vec4(result, 1.0);
}
