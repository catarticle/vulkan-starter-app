#include "application.hpp"

#include <imgui.h>
#include <cstring>

#include <fstream>
#include <vector>
#include <iostream>

#include <cmath>

#include <array>
#include <cstddef>
#include <cstdint>

namespace application {

    struct Vertex { // создаем собственный составной тип данных с названием
        float position[3]; // Координаты вершины X, Y, Z
        float normal[3];   // Компоненты нормали NX, NY, NZ
    };

    constexpr Vertex vertices[] = { // значение можно вычислить во время компиляции; тип элементов массива; название массива; компилятор сам определит кол-во эл-ов
        // Ребро AB
        {{ 1.0f,  1.0f / 3.0f,  1.0f / 3.0f}}, // 32-битные числа с плавающей точкой
        {{ 1.0f, -1.0f / 3.0f, -1.0f / 3.0f}},

        // Ребро AC
        {{ 1.0f / 3.0f,  1.0f,  1.0f / 3.0f}},
        {{-1.0f / 3.0f,  1.0f, -1.0f / 3.0f}},

        // Ребро AD
        {{ 1.0f / 3.0f,  1.0f / 3.0f,  1.0f}},
        {{-1.0f / 3.0f, -1.0f / 3.0f,  1.0f}},

        // Ребро BC
        {{ 1.0f / 3.0f, -1.0f / 3.0f, -1.0f}},
        {{-1.0f / 3.0f,  1.0f / 3.0f, -1.0f}},

        // Ребро BD
        {{ 1.0f / 3.0f, -1.0f, -1.0f / 3.0f}},
        {{-1.0f / 3.0f, -1.0f,  1.0f / 3.0f}},

        // Ребро CD
        {{-1.0f,  1.0f / 3.0f, -1.0f / 3.0f}},
        {{-1.0f, -1.0f / 3.0f,  1.0f / 3.0f}},
    };

    constexpr unsigned int indices[] = {
        // 4 треугольные грани
        // Около A
        0, 2, 4,

        // Около B
        1, 8, 6,

        // Около C
        3, 7, 10,

        // Около D
        5, 11, 9,

        // Шестиугольник ABC
        0, 1, 6,
        0, 6, 7,
        0, 7, 3,
        0, 3, 2,

        // Шестиугольник ABD
        0, 4, 5,
        0, 5, 9,
        0, 9, 8,
        0, 8, 1,

        // Шестиугольник ACD
        2, 3, 10,
        2, 10, 11,
        2, 11, 5,
        2, 5, 4,

        // Шестиугольник BCD
        6, 8, 9,
        6, 9, 11,
        6, 11, 10,
        6, 10, 7
    };

    // Количество вершин после разворачивания треугольников
    constexpr std::size_t expanded_vertex_count =
        sizeof(indices) / sizeof(indices[0]);

    // Вершины с вычисленными нормалями и новые индексы
    std::array<Vertex, expanded_vertex_count> vertices_with_normals{};
    std::array<uint32_t, expanded_vertex_count> expanded_indices{};

    // Процедурная генерация нормалей для всех треугольников
    void generateVertexNormals() {
        for (std::size_t i = 0; i < expanded_vertex_count; i += 3) {
            const Vertex& a = vertices[indices[i]];
            const Vertex& b = vertices[indices[i + 1]];
            const Vertex& c = vertices[indices[i + 2]];

            // Строим два вектора вдоль сторон треугольника
            float ux = b.position[0] - a.position[0];
            float uy = b.position[1] - a.position[1];
            float uz = b.position[2] - a.position[2];

            float vx = c.position[0] - a.position[0];
            float vy = c.position[1] - a.position[1];
            float vz = c.position[2] - a.position[2];

            // Векторное произведение двух сторон даёт нормаль
            float nx = uy * vz - uz * vy;
            float ny = uz * vx - ux * vz;
            float nz = ux * vy - uy * vx;

            // Нормализуем вектор до единичной длины
            float length = std::sqrt(nx * nx + ny * ny + nz * nz);

            if (length < 0.000001f) {
                continue;
            }

            nx /= length;
            ny /= length;
            nz /= length;

            // Находим центр треугольника
            float cx = (a.position[0] + b.position[0] + c.position[0]) / 3.0f;
            float cy = (a.position[1] + b.position[1] + c.position[1]) / 3.0f;
            float cz = (a.position[2] + b.position[2] + c.position[2]) / 3.0f;

            // Направляем нормаль наружу от центра фигуры
            if (nx * cx + ny * cy + nz * cz < 0.0f) {
                nx = -nx;
                ny = -ny;
                nz = -nz;
            }

            // Каждая вершина треугольника получает его нормаль
            for (std::size_t j = 0; j < 3; ++j) {
                std::size_t index = i + j;

                vertices_with_normals[index] = vertices[indices[index]];

                vertices_with_normals[index].normal[0] = nx;
                vertices_with_normals[index].normal[1] = ny;
                vertices_with_normals[index].normal[2] = nz;

                expanded_indices[index] = static_cast<uint32_t>(index);
            }
        }
    }

	VkBuffer vk_vertex_buffer = VK_NULL_HANDLE; // создаём переменную типа VkBuffer с названием vk_vertex_buffer; пока буфер не создан, переменная не содержит действительного дескриптора буфера
	VmaAllocation vk_vertex_buffer_allocation = nullptr; // тип, предоставляемый библиотекой Vulkan Memory Allocator (позволяет управлять выделенной для него памятью); объект, через который VMA управляет выделенной памятью; пока никакая память не выделена
    VkBuffer vk_index_buffer = VK_NULL_HANDLE; // создаём переменную типа VkBuffer с названием vk_index_buffer, в которой будет храниться дескриптор нашего индексного буфера.
    VmaAllocation vk_index_buffer_allocation = nullptr;

    // Буфер общих данных сцены
    VkBuffer vk_scene_uniform_buffer = VK_NULL_HANDLE;
    VmaAllocation vk_scene_uniform_buffer_allocation = nullptr;

    // Буфер данных модели
    VkBuffer vk_model_uniform_buffer = VK_NULL_HANDLE;
    VmaAllocation vk_model_uniform_buffer_allocation = nullptr;

    VkShaderModule vk_vertex_shader = VK_NULL_HANDLE;
    VkShaderModule vk_fragment_shader = VK_NULL_HANDLE;

    // Схемы наборов дескрипторов сцены и модели
    VkDescriptorSetLayout vk_scene_descriptor_set_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout vk_model_descriptor_set_layout = VK_NULL_HANDLE;

    // Общий пул для выделения наборов дескрипторов
    VkDescriptorPool vk_descriptor_pool = VK_NULL_HANDLE;

    // Наборы дескрипторов сцены и модели
    VkDescriptorSet vk_scene_descriptor_set = VK_NULL_HANDLE;
    VkDescriptorSet vk_model_descriptor_set = VK_NULL_HANDLE;

    VkPipelineLayout vk_pipeline_layout = VK_NULL_HANDLE; // объект, описывающий интерфейс передачи некоторых ресурсов и данных в шейдеры
    VkPipeline vk_pipeline = VK_NULL_HANDLE; //handle графического конвейера, содержащего настройки обработки и отрисовки


    // Общие данные сцены (descriptor set = 0)
    struct alignas(16) SceneUniforms {
        float view_projection[16];

        // Положение камеры в мировых координатах
        alignas(16) float camera_position[3];

        // Направление направленного источника света
        alignas(16) float directional_light_direction[3];

        // Цвет и интенсивность направленного света
        alignas(16) float directional_light_color[3];

        // Цвет и интенсивность фонового освещения
        alignas(16) float ambient_light[3];

        // Количество точечных источников и прожекторов
        uint32_t point_light_count;
        uint32_t spot_light_count;
    };

    // Данные отдельной модели (descriptor set = 1)
    struct alignas(16) ModelUniform {
        // Матрица преобразования модели
        float model[16];

        // Основной цвет материала
        alignas(16) float albedo[3];

        // Цвет зеркального блика
        alignas(16) float specular[3];

        // Коэффициент блеска материала
        float alpha;
    };

    // Проверяем размеры структур для std140
    static_assert(sizeof(SceneUniforms) == 144);
    static_assert(sizeof(ModelUniform) == 96);

    // Положение фигуры по осям X, Y, Z
    float object_position[3] = { 0.0f, 0.0f, 0.0f };

    // Поворот вокруг осей X, Y, Z
    float object_rotation[3] = { 0.0f, 0.0f, 0.0f };

    // Масштаб
    float object_scale = 1.0f;

    // Тип проекции
    bool use_perspective = true;

    // Положение камеры в мировых координатах
    float camera_position[3] = { 0.0f, 0.0f, 6.0f };

    // Углы поворота камеры вокруг осей X, Y и Z в градусах
    float camera_rotation[3] = { 0.0f, 0.0f, 0.0f };

    // Вертикальный угол обзора камеры в градусах (field of view)
    float camera_fov = 60.0f;

    // Цветовой множитель RGB
    float object_color[3] = { 1.0f, 1.0f, 1.0f };


    // Направление направленного источника света
    float light_direction[3] = { -0.5f, -1.0f, -0.5f };

    // Цвет направленного источника света
    float light_color[3] = { 1.0f, 1.0f, 1.0f };

    // Интенсивность направленного источника
    float light_intensity = 1.0f;

    // Фоновое освещение
    float ambient_color[3] = { 0.15f, 0.15f, 0.15f };

    // Цвет зеркального блика материала
    float material_specular[3] = { 1.0f, 1.0f, 1.0f };

    // Коэффициент блеска материала
    float material_alpha = 32.0f;

    bool animation_enabled = false;
    bool animation_paused = false;

    float animation_time = 0.0f;
    float animation_speed = 1.0f;

    float path_radius_x = 0.8f;
    float path_radius_y = 0.7f;
    float path_height = 0.6f;

    int path_frequency_y = 2;
    int path_frequency_z = 3;


    // откуда брать данные вершин и как переходить от одной вершины к следующей
    constexpr VkVertexInputBindingDescription vertex_binding{ //структура Vulkan, описывающая источник вершинных данных
        .binding = 0, // задаём номер привязки источника вершинных данных
        .stride = sizeof(Vertex), // шаг; указываем, что следующая вершина начинается через 24 байт
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX, // данные переходят к следующей вершине при обработке очередной вершины
    };

    // какие именно данные находятся внутри одной вершины и как их интерпретировать
    // Описываем два атрибута каждой вершины
    constexpr VkVertexInputAttributeDescription vertex_attributes[] = {
        {
            .location = 0, // координаты позиции
            .binding = 0, // номер вершинного буфера
            .format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, position), // смещение в байтах от начала структуры Vertex до поля position
        },
        {
			.location = 1, // компоненты нормали
			.binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, normal),
        },
    };

    VkShaderModule loadShader(const char* filename) {
        auto& context = graphics::internal::context;

        // Открываем файл с бинарным кодом шейдера
        std::ifstream file(filename, std::ios::binary | std::ios::ate);

        if (!file) {
            std::cerr << "Cannot open shader: " << filename << '\n';
            return VK_NULL_HANDLE;
        }

        // Определяем размер файла в байтах
        const auto file_size = file.tellg();

        if (file_size <= 0 || file_size % 4 != 0) {
            std::cerr << "Invalid shader size: " << filename << '\n';
            return VK_NULL_HANDLE;
        }

        std::vector<uint32_t> code(
            static_cast<size_t>(file_size) / sizeof(uint32_t)
        );

        file.seekg(0);

        if (!file.read(
            reinterpret_cast<char*>(code.data()),
            static_cast<std::streamsize>(file_size)
        )) {
            std::cerr << "Cannot read shader: " << filename << '\n';
            return VK_NULL_HANDLE;
        }

        // Описываем создаваемый шейдерный модуль
        const VkShaderModuleCreateInfo shader_info{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = code.size() * sizeof(uint32_t),
            .pCode = code.data(),
        };

        VkShaderModule shader = VK_NULL_HANDLE;

		// Создаём объект Vulkan ShaderModule, который будет содержать скомпилированный шейдерный код
        if (vkCreateShaderModule(
            context.device,
            &shader_info,
            nullptr,
            &shader
        ) != VK_SUCCESS) {
            std::cerr << "Cannot create shader module\n";
            return VK_NULL_HANDLE;
        }

        return shader;
    }

    struct Mat4 {
        float data[16];
    };

    // Единичная матрица
    Mat4 identity() {
        Mat4 result{};

        result.data[0] = 1.0f;
        result.data[5] = 1.0f;
        result.data[10] = 1.0f;
        result.data[15] = 1.0f;

        return result;
    }

    // Умножение двух матриц
    Mat4 multiply(const Mat4& a, const Mat4& b) {
        Mat4 result{};

        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                for (int k = 0; k < 4; ++k) {
                    result.data[col * 4 + row] +=
                        a.data[k * 4 + row] *
                        b.data[col * 4 + k];
                }
            }
        }

        return result;
    }

    // Матрица перемещения
    Mat4 translation(float x, float y, float z) {
        Mat4 result = identity();

        result.data[12] = x;
        result.data[13] = y;
        result.data[14] = z;

        return result;
    }

    // Матрица поворота вокруг оси X
    Mat4 rotationX(float angle) {
        Mat4 result = identity();

        float c = std::cos(angle);
        float s = std::sin(angle);

        result.data[5] = c;
        result.data[6] = s;
        result.data[9] = -s;
        result.data[10] = c;

        return result;
    }

    // Матрица поворота вокруг оси Y
    Mat4 rotationY(float angle) {
        Mat4 result = identity();

        float c = std::cos(angle);
        float s = std::sin(angle);

        result.data[0] = c;
        result.data[2] = -s;
        result.data[8] = s;
        result.data[10] = c;

        return result;
    }

    // Матрица поворота вокруг оси Z
    Mat4 rotationZ(float angle) {
        Mat4 result = identity();

        float c = std::cos(angle);
        float s = std::sin(angle);

        result.data[0] = c;
        result.data[1] = s;
        result.data[4] = -s;
        result.data[5] = c;

        return result;
    }

    // Матрица масштабирования
    Mat4 scaling(float scale) {
        Mat4 result = identity();

        result.data[0] = scale;
        result.data[5] = scale;
        result.data[10] = scale;

        return result;
    }

    // Матрица перспективной проекции
    Mat4 perspective(float aspect, float fov_degrees) {
        Mat4 result{};

        // Переводим угол обзора из градусов в радианы
        const float fov = fov_degrees * 3.14159265f / 180.0f;

        const float nearPlane = 0.1f;
        const float farPlane = 100.0f;

        float f = 1.0f / std::tan(fov / 2.0f);

        result.data[0] = f / aspect;
        result.data[5] = -f;
        result.data[10] = farPlane / (nearPlane - farPlane);
        result.data[11] = -1.0f;
        result.data[14] =
            (farPlane * nearPlane) / (nearPlane - farPlane);

        return result;
    }

	// Матрица ортографической проекции
    Mat4 orthographic(float aspect) {
        Mat4 result{};

        const float halfHeight = 2.5f;
        const float halfWidth = halfHeight * aspect;

        const float nearPlane = 0.1f;
        const float farPlane = 100.0f;

        result.data[0] = 1.0f / halfWidth;
        result.data[5] = -1.0f / halfHeight;
        result.data[10] = 1.0f / (nearPlane - farPlane);
        result.data[14] = nearPlane / (nearPlane - farPlane);
        result.data[15] = 1.0f;

        return result;
    }

    bool createGraphicsPipeline() {
        auto& context = graphics::internal::context;

        // Указываем шейдеры графического конвейера
        const VkPipelineShaderStageCreateInfo shader_stages[] = {
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vk_vertex_shader,
                .pName = "main",
            },
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = vk_fragment_shader,
                .pName = "main",
            },
        };

        // Описываем расположение данных вершин
        const VkPipelineVertexInputStateCreateInfo vertex_input{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &vertex_binding,
            .vertexAttributeDescriptionCount = 2,
            .pVertexAttributeDescriptions = vertex_attributes,
        };

        // Указываем, что рисуем отдельные треугольники (примитивы)
        const VkPipelineInputAssemblyStateCreateInfo input_assembly{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        };

        // Настройки области вывода изображения
        const VkPipelineViewportStateCreateInfo viewport_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };

        // Настройки растеризации
        const VkPipelineRasterizationStateCreateInfo rasterization{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .lineWidth = 1.0f,
        };

        // Настройки мультисэмплинга
        const VkPipelineMultisampleStateCreateInfo multisampling{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };

        // Проверка глубины
        const VkPipelineDepthStencilStateCreateInfo depth_stencil{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS,
        };

        // Запись цвета в изображение
        const VkPipelineColorBlendAttachmentState color_attachment{
            .blendEnable = VK_FALSE,
            .colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT,
        };

        const VkPipelineColorBlendStateCreateInfo color_blending{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &color_attachment,
        };

        // Viewport и Scissor будем задавать при отрисовке
        const VkDynamicState dynamic_states[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };

        const VkPipelineDynamicStateCreateInfo dynamic_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = 2,
            .pDynamicStates = dynamic_states,
        };

        // Схемы наборов дескрипторов для графического конвейера
        const VkDescriptorSetLayout descriptor_layouts[] = {
            vk_scene_descriptor_set_layout,
            vk_model_descriptor_set_layout,
        };

        // Графический конвейер использует два набора дескрипторов
        const VkPipelineLayoutCreateInfo pipeline_layout_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 2,
            .pSetLayouts = descriptor_layouts,
        };

        if (vkCreatePipelineLayout(
            context.device,
            &pipeline_layout_info,
            nullptr,
            &vk_pipeline_layout
        ) != VK_SUCCESS) {
            return false;
        }

        // Собираем настройки графического конвейера
        const VkGraphicsPipelineCreateInfo pipeline_info{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = shader_stages,
            .pVertexInputState = &vertex_input,
            .pInputAssemblyState = &input_assembly,
            .pViewportState = &viewport_state,
            .pRasterizationState = &rasterization,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depth_stencil,
            .pColorBlendState = &color_blending,
            .pDynamicState = &dynamic_state,
            .layout = vk_pipeline_layout,
            .renderPass = context.render_pass,
            .subpass = 0,
        };

        if (vkCreateGraphicsPipelines(
            context.device,
            VK_NULL_HANDLE,
            1,
            &pipeline_info,
            nullptr,
            &vk_pipeline
        ) != VK_SUCCESS) {
            vkDestroyPipelineLayout(
                context.device,
                vk_pipeline_layout,
                nullptr
            );
            vk_pipeline_layout = VK_NULL_HANDLE;
            return false;
        }

        return true;
    }

	bool initialize() {
        auto& context = graphics::internal::context;

        // Генерируем нормали до создания вершинного буфера
        generateVertexNormals();

        // Размеры новых массивов вершин и индексов
        const std::size_t vertex_data_size =
            vertices_with_normals.size() * sizeof(Vertex);

        const std::size_t index_data_size =
            expanded_indices.size() * sizeof(uint32_t);

        const VkBufferCreateInfo vertex_buffer_info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = vertex_data_size,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, // Для чего будет использоваться буфер
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE, // Режим владения буфером очередями GPU
        };

        const VmaAllocationCreateInfo vertex_allocation_info{
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };

		if (vmaCreateBuffer( // создаем буфер для хранения вершин; функция возвращает VK_SUCCESS, если буфер создан успешно
            context.allocator,
            &vertex_buffer_info,
            &vertex_allocation_info,
            &vk_vertex_buffer,
            &vk_vertex_buffer_allocation,
            nullptr
        ) != VK_SUCCESS) {
            return false;
        }

        VmaAllocationInfo allocation_info{}; // получили информацию о выделенной памяти, включая указатель на неё
        vmaGetAllocationInfo(
            context.allocator,
            vk_vertex_buffer_allocation,
            &allocation_info
        );

		std::memcpy( // копируем данные вершин в выделенную память; функция копирует sizeof(vertices) байт из массива vertices в память, на которую указывает allocation_info.pMappedData
            allocation_info.pMappedData,
            vertices_with_normals.data(),
            vertex_data_size
        );

		vmaFlushAllocation( // Обеспечиваем видимость записанных CPU данных для GPU
            context.allocator,
            vk_vertex_buffer_allocation,
            0,
            VK_WHOLE_SIZE
        );

        const VkBufferCreateInfo index_buffer_info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = index_data_size, // 60 индексов * 4 байта = 240 байт (занимают все индексы) размер буфера в байтах; sizeof возвращает размер в байтах, занимаемый объектом или типом данных; в данном случае, размер массива indices
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT, // для чего будет использоваться буфер
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        const VmaAllocationCreateInfo index_allocation_info{
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | // просим VMA предоставить отображённый в адресное пространство CPU участок памяти
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, // сообщаем, что CPU будет последовательно записывать данные.
            .usage = VMA_MEMORY_USAGE_AUTO, // VMA выбирает подходящий тип памяти
        };

        if (vmaCreateBuffer( // создаем Vulkan-буфер и выделяем для него память
            context.allocator,
            &index_buffer_info,
            &index_allocation_info,
            &vk_index_buffer,
            &vk_index_buffer_allocation,
            nullptr
        ) != VK_SUCCESS) {
            vmaDestroyBuffer(
                context.allocator,
                vk_vertex_buffer,
                vk_vertex_buffer_allocation
            );
            vk_vertex_buffer = VK_NULL_HANDLE;
            vk_vertex_buffer_allocation = nullptr;
            return false;
        }

        VmaAllocationInfo index_allocation{};

        vmaGetAllocationInfo( // получаем информацию о памяти
            context.allocator,
            vk_index_buffer_allocation,
            &index_allocation
        );

        std::memcpy( // копируем индексы
            index_allocation.pMappedData,
            expanded_indices.data(),
            index_data_size
        );

        vmaFlushAllocation( // flush отвечает именно за видимость записей в памяти, т.е. мы обеспечили видимость данных для GPU
            context.allocator,
            vk_index_buffer_allocation,
            0,
            VK_WHOLE_SIZE
        );

        // Общие параметры создания Uniform Buffer
        const VkBufferCreateInfo uniform_buffer_info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sizeof(SceneUniforms),
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        const VmaAllocationCreateInfo uniform_allocation_info{
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };

        // Описываем Uniform Buffer для общих данных сцены
        VkBufferCreateInfo scene_buffer_info = uniform_buffer_info;

        // Размер буфера равен размеру структуры SceneUniforms
        scene_buffer_info.size = sizeof(SceneUniforms);

        // Создаём буфер и выделяем память через VMA
        if (vmaCreateBuffer(
            context.allocator,
            &scene_buffer_info,
            &uniform_allocation_info,
            &vk_scene_uniform_buffer,
            &vk_scene_uniform_buffer_allocation,
            nullptr
        ) != VK_SUCCESS) {
            return false;
        }

        // Описываем Uniform Buffer для данных модели
        VkBufferCreateInfo model_buffer_info = uniform_buffer_info;

        // Размер буфера равен размеру структуры ModelUniform
        model_buffer_info.size = sizeof(ModelUniform);

        // Создаём буфер и выделяем память через VMA
        if (vmaCreateBuffer(
            context.allocator,
            &model_buffer_info,
            &uniform_allocation_info,
            &vk_model_uniform_buffer,
            &vk_model_uniform_buffer_allocation,
            nullptr
        ) != VK_SUCCESS) {
            return false;
        }

        // Привязка Uniform Buffer с общими данными сцены
        const VkDescriptorSetLayoutBinding scene_ubo_binding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                          VK_SHADER_STAGE_FRAGMENT_BIT,
        };

        // Привязка Uniform Buffer с данными модели
        const VkDescriptorSetLayoutBinding model_ubo_binding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT |
                          VK_SHADER_STAGE_FRAGMENT_BIT,
        };

        // Описываем схему набора дескрипторов сцены
        const VkDescriptorSetLayoutCreateInfo scene_layout_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &scene_ubo_binding,
        };

        // Создаём схему набора дескрипторов сцены
        if (vkCreateDescriptorSetLayout(
            context.device,
            &scene_layout_info,
            nullptr,
            &vk_scene_descriptor_set_layout
        ) != VK_SUCCESS) {
            return false;
        }

        // Описываем схему набора дескрипторов модели
        const VkDescriptorSetLayoutCreateInfo model_layout_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &model_ubo_binding,
        };

        // Создаём схему набора дескрипторов модели
        if (vkCreateDescriptorSetLayout(
            context.device,
            &model_layout_info,
            nullptr,
            &vk_model_descriptor_set_layout
        ) != VK_SUCCESS) {
            return false;
        }

        const VkDescriptorPoolSize pool_size{ // количество дескрипторов определённого типа
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 2,
        };

        const VkDescriptorPoolCreateInfo pool_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 2,
            .poolSizeCount = 1,
            .pPoolSizes = &pool_size,
        };

        if (vkCreateDescriptorPool(
            context.device,
            &pool_info,
            nullptr,
            &vk_descriptor_pool
        ) != VK_SUCCESS) {
            return false;
        }

        // Массив схем в порядке номеров set
        const VkDescriptorSetLayout descriptor_layouts[] = {
            vk_scene_descriptor_set_layout,
            vk_model_descriptor_set_layout,
        };

        // Описываем выделение двух наборов дескрипторов
        const VkDescriptorSetAllocateInfo allocate_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = vk_descriptor_pool,
            .descriptorSetCount = 2,
            .pSetLayouts = descriptor_layouts,
        };

        VkDescriptorSet descriptor_sets[2]{};

        // Выделяем оба набора из общего пула
        if (vkAllocateDescriptorSets(
            context.device,
            &allocate_info,
            descriptor_sets
        ) != VK_SUCCESS) {
            return false;
        }

        // Сохраняем полученные наборы
        vk_scene_descriptor_set = descriptor_sets[0];
        vk_model_descriptor_set = descriptor_sets[1];

        // Дескриптор буфера общих данных сцены
        const VkDescriptorBufferInfo scene_buffer_descriptor{
            .buffer = vk_scene_uniform_buffer,
            .offset = 0,
            .range = sizeof(SceneUniforms),
        };

        // Новый Uniform Buffer модели
        const VkDescriptorBufferInfo model_buffer_descriptor{
            .buffer = vk_model_uniform_buffer,
            .offset = 0,
            .range = sizeof(ModelUniform),
        };

        vk_vertex_shader = loadShader("shaders/basic.vert.spv");
        vk_fragment_shader = loadShader("shaders/basic.frag.spv");

        if (vk_vertex_shader == VK_NULL_HANDLE ||
            vk_fragment_shader == VK_NULL_HANDLE) {
            std::cerr << "Failed to load shaders\n";
            return false;
        }

        // Записываем ссылки на буферы в два набора дескрипторов
        const VkWriteDescriptorSet descriptor_writes[] = {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = vk_scene_descriptor_set,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &scene_buffer_descriptor,
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = vk_model_descriptor_set,
                .dstBinding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &model_buffer_descriptor,
            },
        };

        // Обновляем оба набора дескрипторов
        vkUpdateDescriptorSets(
            context.device,
            2,
            descriptor_writes,
            0,
            nullptr
        );

        if (!createGraphicsPipeline()) {
            std::cerr << "Failed to create graphics pipeline\n";
            return false;
        }

		return true;
	}

	void shutdown() { // функция освобождает все ресурсы, которые были созданы в initialize
		auto& context = graphics::internal::context;
		vkQueueWaitIdle(context.graphics_queue);

        vkDestroyPipeline(context.device, vk_pipeline, nullptr);

        vkDestroyPipelineLayout(context.device, vk_pipeline_layout, nullptr);

        vkDestroyDescriptorPool(
            context.device,
            vk_descriptor_pool,
            nullptr
        );

        // Уничтожаем схемы наборов дескрипторов
        vkDestroyDescriptorSetLayout(
            context.device,
            vk_scene_descriptor_set_layout,
            nullptr
        );

        vkDestroyDescriptorSetLayout(
            context.device,
            vk_model_descriptor_set_layout,
            nullptr
        );

        // Освобождаем буфер общих данных сцены
        vmaDestroyBuffer(
            context.allocator,
            vk_scene_uniform_buffer,
            vk_scene_uniform_buffer_allocation
        );

        // Освобождаем буфер данных модели
        vmaDestroyBuffer(
            context.allocator,
            vk_model_uniform_buffer,
            vk_model_uniform_buffer_allocation
        );

        vkDestroyShaderModule(context.device, vk_vertex_shader, nullptr);
        vkDestroyShaderModule(context.device, vk_fragment_shader, nullptr);

		vmaDestroyBuffer( // освобождаем память, выделенную для буфера вершин; функция освобождает память, выделенную для буфера вершин, и уничтожает сам буфер
            context.allocator,
            vk_vertex_buffer,
            vk_vertex_buffer_allocation
        );

		vmaDestroyBuffer( // освобождаем память, выделенную для буфера индексов; функция освобождает память, выделенную для буфера индексов, и уничтожает сам буфер
            context.allocator,
            vk_index_buffer,
            vk_index_buffer_allocation
        );
	}

	//void update([[maybe_unused]] double time) {
	//	ImGui::ShowDemoWindow();
	//}

    void update([[maybe_unused]] double time) {
        ImGui::Begin("Truncated Tetrahedron");

        // Выбор типа проекции
        if (ImGui::Button("Orthographic")) {
            use_perspective = false;
        }

        ImGui::SameLine();

        if (ImGui::Button("Perspective")) {
            use_perspective = true;
        }

        // Показываем текущий режим проекции
        ImGui::Text(
            "Current projection: %s",
            use_perspective ? "Perspective" : "Orthographic"
        );

        ImGui::Separator();
        ImGui::Text("Camera");

        // Управление положением камеры
        ImGui::SliderFloat3(
            "Camera Position",
            camera_position,
            -10.0f,
            10.0f
        );

        // Управление углами поворота камеры
        ImGui::SliderFloat3(
            "Camera Rotation",
            camera_rotation,
            -180.0f,
            180.0f
        );

        // Управление углом обзора перспективной камеры
        ImGui::SliderFloat(
            "Camera FOV",
            &camera_fov,
            20.0f,
            110.0f
        );

        ImGui::Separator();

        // Перемещение по трём осям
        ImGui::SliderFloat3(
            "Position",
            object_position,
            -2.0f,
            2.0f
        );

        // Вращение вокруг трёх осей
        ImGui::SliderFloat3(
            "Rotation",
            object_rotation,
            -180.0f,
            180.0f
        );

        // Масштаб
        ImGui::SliderFloat(
            "Scale",
            &object_scale,
            0.2f,
            2.0f
        );

        // Параметры материала
        ImGui::Separator();
        ImGui::Text("Material");

        ImGui::ColorEdit3("Albedo", object_color);
        ImGui::ColorEdit3("Specular", material_specular);

        ImGui::SliderFloat(
            "Shininess",
            &material_alpha,
            1.0f,
            256.0f
        );

        // Параметры освещения сцены
        ImGui::Separator();
        ImGui::Text("Lighting");

        ImGui::SliderFloat3(
            "Light Direction",
            light_direction,
            -1.0f,
            1.0f
        );

        ImGui::ColorEdit3("Light Color", light_color);

        ImGui::SliderFloat(
            "Light Intensity",
            &light_intensity,
            0.0f,
            5.0f
        );

        ImGui::ColorEdit3("Ambient Light", ambient_color);


        ImGui::Separator();
        ImGui::Text("Animation");

        // Включение движения
        ImGui::Checkbox("Animation", &animation_enabled);

        // Пауза или продолжение
        if (ImGui::Button(animation_paused ? "Resume" : "Pause")) {
            animation_paused = !animation_paused;
        }

        // Скорость движения
        ImGui::SliderFloat("Speed", &animation_speed, 0.1f, 3.0f);

        // Параметры траектории
        ImGui::SliderFloat("Radius X", &path_radius_x, 0.0f, 1.5f);
        ImGui::SliderFloat("Radius Y", &path_radius_y, 0.0f, 1.5f);
        ImGui::SliderFloat("Height", &path_height, 0.0f, 1.0f);

        // Частоты колебаний
        ImGui::SliderInt("Frequency Y", &path_frequency_y, 1, 5);
        ImGui::SliderInt("Frequency Z", &path_frequency_z, 1, 5);

        // Обновление времени движения
        if (animation_enabled && !animation_paused) {
            animation_time += ImGui::GetIO().DeltaTime * animation_speed;
        }

        ImGui::End();
    }

    void render(const graphics::internal::FrameData& fd) {
        auto& context = graphics::internal::context;

        // Начинаем запись команд
        VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };

        vkBeginCommandBuffer(fd.command_buffer, &beginInfo);

        // Задаём цвет фона и глубину
        VkClearValue clearValues[2]{};

        clearValues[0].color =
        { {0.1f, 0.1f, 0.1f, 1.0f} };

        clearValues[1].depthStencil =
        { 1.0f, 0 };

        // Описываем проход отрисовки
        VkRenderPassBeginInfo renderPassInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = context.render_pass,
            .framebuffer = fd.framebuffer,
            .renderArea = {
                .offset = {0, 0},
                .extent = context.swapchain_extent,
            },
            .clearValueCount = 2,
            .pClearValues = clearValues,
        };

        // Начинаем проход отрисовки
        vkCmdBeginRenderPass(
            fd.command_buffer,
            &renderPassInfo,
            VK_SUBPASS_CONTENTS_INLINE
        );

        // Указываем графический конвейер
        vkCmdBindPipeline(
            fd.command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            vk_pipeline
        );

        // Задаём область вывода
        const VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = float(context.swapchain_extent.width),
            .height = float(context.swapchain_extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };

        vkCmdSetViewport(fd.command_buffer, 0, 1, &viewport);

        // Ограничиваем рисование размерами изображения
        const VkRect2D scissor{
            .offset = {0, 0},
            .extent = context.swapchain_extent,
        };

        vkCmdSetScissor(fd.command_buffer, 0, 1, &scissor);

        // Подключаем Vertex Buffer
        const VkDeviceSize offset = 0;

        vkCmdBindVertexBuffers(
            fd.command_buffer,
            0,
            1,
            &vk_vertex_buffer,
            &offset
        );

        // Подключаем Index Buffer
        vkCmdBindIndexBuffer(
            fd.command_buffer,
            vk_index_buffer,
            0,
            VK_INDEX_TYPE_UINT32
        );

        // Переводим градусы в радианы
        const float degToRad = 3.14159265f / 180.0f;

        float angleX = object_rotation[0] * degToRad;
        float angleY = object_rotation[1] * degToRad;
        float angleZ = object_rotation[2] * degToRad;

        // Автоматический поворот во время анимации
        float spin_y = animation_enabled ? animation_time : 0.0f;
        float sway_z = animation_enabled
            ? 0.3f * std::sin(animation_time)
            : 0.0f;

        // Смещение по траектории
        float path_x = 0.0f;
        float path_y = 0.0f;
        float path_z = 0.0f;

        if (animation_enabled) {
            float t = animation_time;

            path_x = path_radius_x * std::sin(t);

            path_y = path_radius_y *
                std::sin(path_frequency_y * t);

            path_z = path_height *
                std::sin(path_frequency_z * t);
        }


        // Матрица модели: перенос, вращения, масштаб
        Mat4 model = multiply(
            translation(
                object_position[0] + path_x,
                object_position[1] + path_y,
                object_position[2] + path_z
            ),
            multiply(
                rotationZ(angleZ + sway_z),
                multiply(
                    rotationY(angleY + spin_y),
                    multiply(
                        rotationX(angleX),
                        scaling(object_scale)
                    )
                )
            )
        );

        // Переводим углы поворота камеры из градусов в радианы
        float camera_angle_x = camera_rotation[0] * degToRad;
        float camera_angle_y = camera_rotation[1] * degToRad;
        float camera_angle_z = camera_rotation[2] * degToRad;

        // Матрица вида — обратное преобразование камеры:
        // сначала обратное перемещение, затем обратные повороты
        Mat4 view = multiply(
            rotationZ(-camera_angle_z),
            multiply(
                rotationY(-camera_angle_y),
                multiply(
                    rotationX(-camera_angle_x),
                    translation(
                        -camera_position[0],
                        -camera_position[1],
                        -camera_position[2]
                    )
                )
            )
        );

        // Соотношение сторон окна
        float aspect =
            float(context.swapchain_extent.width) /
            float(context.swapchain_extent.height);

        // Выбираем матрицу проекции
        Mat4 projection = use_perspective
            ? perspective(aspect, camera_fov)
            : orthographic(aspect);

        // Матрица View-Projection общая для всех объектов сцены
        Mat4 view_projection = multiply(projection, view);

        // Заполняем общие данные сцены
        SceneUniforms scene_ubo{};

        std::memcpy(
            scene_ubo.view_projection,
            view_projection.data,
            sizeof(view_projection.data)
        );

        // Передаём положение камеры
        std::memcpy(
            scene_ubo.camera_position,
            camera_position,
            sizeof(camera_position)
        );

        // Передаём направление направленного источника
        std::memcpy(
            scene_ubo.directional_light_direction,
            light_direction,
            sizeof(light_direction)
        );

        // Цвет источника умножаем на его интенсивность
        for (int i = 0; i < 3; ++i) {
            scene_ubo.directional_light_color[i] =
                light_color[i] * light_intensity;
        }

        // Передаём фоновое освещение
        std::memcpy(
            scene_ubo.ambient_light,
            ambient_color,
            sizeof(ambient_color)
        );

        // Точечные источники и прожекторы добавим позже
        scene_ubo.point_light_count = 0;
        scene_ubo.spot_light_count = 0;

        // Заполняем данные конкретной модели
        ModelUniform model_ubo{};

        std::memcpy(
            model_ubo.model,
            model.data,
            sizeof(model.data)
        );

        // Основной цвет материала
        std::memcpy(
            model_ubo.albedo,
            object_color,
            sizeof(object_color)
        );

        // Цвет зеркального отражения
        std::memcpy(
            model_ubo.specular,
            material_specular,
            sizeof(material_specular)
        );

        // Коэффициент блеска
        model_ubo.alpha = material_alpha;

        // Получаем доступ к памяти буфера сцены
        VmaAllocationInfo scene_allocation{};

        vmaGetAllocationInfo(
            context.allocator,
            vk_scene_uniform_buffer_allocation,
            &scene_allocation
        );

        // Копируем параметры сцены в Uniform Buffer
        std::memcpy(
            scene_allocation.pMappedData,
            &scene_ubo,
            sizeof(scene_ubo)
        );

        // Делаем изменения видимыми для GPU
        vmaFlushAllocation(
            context.allocator,
            vk_scene_uniform_buffer_allocation,
            0,
            sizeof(scene_ubo)
        );

        // Получаем доступ к памяти буфера модели
        VmaAllocationInfo model_allocation{};

        vmaGetAllocationInfo(
            context.allocator,
            vk_model_uniform_buffer_allocation,
            &model_allocation
        );

        // Копируем параметры модели в Uniform Buffer
        std::memcpy(
            model_allocation.pMappedData,
            &model_ubo,
            sizeof(model_ubo)
        );

        // Делаем изменения видимыми для GPU
        vmaFlushAllocation(
            context.allocator,
            vk_model_uniform_buffer_allocation,
            0,
            sizeof(model_ubo)
        );

        // Собираем два набора дескрипторов в массив
        const VkDescriptorSet descriptor_sets[] = {
            vk_scene_descriptor_set,
            vk_model_descriptor_set,
        };

        // Подключаем наборы сцены и модели к графическому конвейеру
        vkCmdBindDescriptorSets(
            fd.command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            vk_pipeline_layout,
            0,
            2,
            descriptor_sets,
            0,
            nullptr
        );

        // Рисуем наши 60 индексов
        vkCmdDrawIndexed(
            fd.command_buffer,
            static_cast<uint32_t>(expanded_indices.size()),
            1,
            0,
            0,
            0
        );

        // Завершаем проход отрисовки
        vkCmdEndRenderPass(fd.command_buffer);

        // Завершаем запись команд
        vkEndCommandBuffer(fd.command_buffer);
    }

} // namespace application