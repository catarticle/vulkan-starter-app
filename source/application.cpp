#include "application.hpp"

#include <imgui.h>
#include <cstring>

#include <fstream>
#include <vector>
#include <iostream>

#include <cmath>

namespace application {

    struct Vertex { // создаем собственный составной тип данных с названием
		float position[3]; // массив вещественных чисел со значениями координат x, y, z
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

	VkBuffer vk_vertex_buffer = VK_NULL_HANDLE; // создаём переменную типа VkBuffer с названием vk_vertex_buffer; пока буфер не создан, переменная не содержит действительного дескриптора буфера
	VmaAllocation vk_vertex_buffer_allocation = nullptr; // тип, предоставляемый библиотекой Vulkan Memory Allocator (позволяет управлять выделенной для него памятью); объект, через который VMA управляет выделенной памятью; пока никакая память не выделена
    VkBuffer vk_index_buffer = VK_NULL_HANDLE; // создаём переменную типа VkBuffer с названием vk_index_buffer, в которой будет храниться дескриптор нашего индексного буфера.
    VmaAllocation vk_index_buffer_allocation = nullptr;

    VkBuffer vk_uniform_buffer = VK_NULL_HANDLE;
    VmaAllocation vk_uniform_buffer_allocation = nullptr;

    VkShaderModule vk_vertex_shader = VK_NULL_HANDLE;
    VkShaderModule vk_fragment_shader = VK_NULL_HANDLE;

    VkDescriptorSetLayout vk_descriptor_set_layout = VK_NULL_HANDLE;
    VkDescriptorPool vk_descriptor_pool = VK_NULL_HANDLE;
    VkDescriptorSet vk_descriptor_set = VK_NULL_HANDLE;
    VkPipelineLayout vk_pipeline_layout = VK_NULL_HANDLE; // объект, описывающий интерфейс передачи некоторых ресурсов и данных в шейдеры
    VkPipeline vk_pipeline = VK_NULL_HANDLE; //handle графического конвейера, содержащего настройки обработки и отрисовки

    struct UniformBufferObject {
        float mvp[16];
        float tint[4];
    };

    // Положение фигуры по осям X, Y, Z
    float object_position[3] = { 0.0f, 0.0f, 0.0f };

    // Поворот вокруг осей X, Y, Z
    float object_rotation[3] = { 0.0f, 0.0f, 0.0f };

    // Масштаб
    float object_scale = 1.0f;

    // Тип проекции
    bool use_perspective = true;

    // Цветовой множитель RGB
    float object_color[3] = { 1.0f, 1.0f, 1.0f };


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
        .stride = sizeof(Vertex), // шаг; указываем, что следующая вершина начинается через 12 байт
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX, // данные переходят к следующей вершине при обработке очередной вершины
    };

    // какие именно данные находятся внутри одной вершины и как их интерпретировать
    constexpr VkVertexInputAttributeDescription vertex_attribute{
        .location = 0, // Номер входного атрибута, с которым будет работать вершинный шейдер
        .binding = 0, // Используем описание буфера с номером 0
        .format = VK_FORMAT_R32G32B32_SFLOAT, // Читаем три 32-битных числа с плавающей точкой
        .offset = 0, // Координаты начинаются с самого начала структуры Vertex
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
    Mat4 perspective(float aspect) {
        Mat4 result{};

        const float fov = 60.0f * 3.14159265f / 180.0f;
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
            .vertexAttributeDescriptionCount = 1,
            .pVertexAttributeDescriptions = &vertex_attribute,
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

        // Описываем наборы дескрипторов, используемые графическим конвейером
        const VkPipelineLayoutCreateInfo pipeline_layout_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1, // Pipeline Layout использует одну схему набора дескрипторов
            .pSetLayouts = &vk_descriptor_set_layout,
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

        const VkBufferCreateInfo vertex_buffer_info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = sizeof(vertices), // 3 * 4 = 12 байт -> 12 вершин * 12 байт = 144 байт (занимают все вершины) размер буфера в байтах; sizeof возвращает размер в байтах, занимаемый объектом или типом данных; в данном случае, размер массива vertices
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
            vertices,
            sizeof(vertices)
        );

		vmaFlushAllocation( // Обеспечиваем видимость записанных CPU данных для GPU
            context.allocator,
            vk_vertex_buffer_allocation,
            0,
            VK_WHOLE_SIZE
        );

        const VkBufferCreateInfo index_buffer_info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = sizeof(indices), // 60 индексов * 4 байта = 240 байт (занимают все индексы) размер буфера в байтах; sizeof возвращает размер в байтах, занимаемый объектом или типом данных; в данном случае, размер массива indices
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
            indices,
            sizeof(indices)
        );

        vmaFlushAllocation( // flush отвечает именно за видимость записей в памяти, т.е. мы обеспечили видимость данных для GPU
            context.allocator,
            vk_index_buffer_allocation,
            0,
            VK_WHOLE_SIZE
        );

        const VkBufferCreateInfo uniform_buffer_info{ // описываем параметры создаваемого буфера
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sizeof(UniformBufferObject),
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        const VmaAllocationCreateInfo uniform_allocation_info{
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };

        if (vmaCreateBuffer(
            context.allocator,
            &uniform_buffer_info,
            &uniform_allocation_info,
            &vk_uniform_buffer,
            &vk_uniform_buffer_allocation,
            nullptr
        ) != VK_SUCCESS) {
            return false;
        }

        const VkDescriptorSetLayoutBinding ubo_binding{ // описание отдельной привязки
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1, // количество дескрипторов внутри этой привязки (у нас один Uniform Buffer)
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, // набор стадий шейдеров, которым доступен ресурс
            .pImmutableSamplers = nullptr,
        };

        const VkDescriptorSetLayoutCreateInfo descriptor_layout_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &ubo_binding,
        };

        if (vkCreateDescriptorSetLayout(
            context.device,
            &descriptor_layout_info,
            nullptr,
            &vk_descriptor_set_layout
        ) != VK_SUCCESS) {
            return false;
        }

        const VkDescriptorPoolSize pool_size{ // количество дескрипторов определённого типа
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
        };

        const VkDescriptorPoolCreateInfo pool_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
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

        const VkDescriptorSetAllocateInfo allocate_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = vk_descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &vk_descriptor_set_layout,
        };

        if (vkAllocateDescriptorSets(
            context.device,
            &allocate_info,
            &vk_descriptor_set
        ) != VK_SUCCESS) {
            return false;
        }

        const VkDescriptorBufferInfo descriptor_buffer_info{ // буфер и какую область внутри него должен использовать дескриптор
            .buffer = vk_uniform_buffer,
            .offset = 0,
            .range = sizeof(UniformBufferObject),
        };

        vk_vertex_shader = loadShader("shaders/basic.vert.spv");
        vk_fragment_shader = loadShader("shaders/basic.frag.spv");

        if (vk_vertex_shader == VK_NULL_HANDLE ||
            vk_fragment_shader == VK_NULL_HANDLE) {
            std::cerr << "Failed to load shaders\n";
            return false;
        }

		const VkWriteDescriptorSet descriptor_write{ // структура, описывающая операцию записи информации о ресурсах в Descriptor Set
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = vk_descriptor_set,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &descriptor_buffer_info,
        };

        vkUpdateDescriptorSets(
            context.device,
            1,
            &descriptor_write,
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

        vkDestroyDescriptorSetLayout(
            context.device,
            vk_descriptor_set_layout,
            nullptr
        );

        vmaDestroyBuffer(
            context.allocator,
            vk_uniform_buffer,
            vk_uniform_buffer_allocation
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

        // Выбор цветового множителя
        ImGui::ColorEdit3("Color", object_color);

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

        // Матрица вида: положение и ориентация камеры
        Mat4 view = multiply(
            translation(0.0f, 0.0f, -6.0f),
            multiply(rotationX(-0.7f), rotationY(0.4f))
        );

        // Соотношение сторон окна
        float aspect =
            float(context.swapchain_extent.width) /
            float(context.swapchain_extent.height);

        // Выбираем проекцию
        Mat4 projection = use_perspective
            ? perspective(aspect)
            : orthographic(aspect);

        // Итоговая матрица Model-View-Projection
        Mat4 mvp = multiply(
            projection,
            multiply(view, model)
        );

		// Подготавливаем матрицу MVP и цвет для записи в Uniform Buffer
        UniformBufferObject ubo{};

        std::memcpy(ubo.mvp, mvp.data, sizeof(mvp.data)); // куда, откуда, сколько байт

        ubo.tint[0] = object_color[0];
        ubo.tint[1] = object_color[1];
        ubo.tint[2] = object_color[2];
        ubo.tint[3] = 1.0f;

        VmaAllocationInfo uniform_allocation{}; // структура с информацией о выделении памяти для Uniform Buffer

        vmaGetAllocationInfo(
            context.allocator,
            vk_uniform_buffer_allocation,
            &uniform_allocation
        );

		// Копируем данные в выделенную память
        std::memcpy(
			uniform_allocation.pMappedData, // указатель на выделенную память
            &ubo,
            sizeof(ubo)
        );

		// Обеспечиваем видимость данных для GPU
        vmaFlushAllocation(
            context.allocator,
            vk_uniform_buffer_allocation,
            0,
            sizeof(ubo)
        );

        vkCmdBindDescriptorSets( // привязывает Descriptor Set к графическому конвейеру
            fd.command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            vk_pipeline_layout,
            0,
            1,
            &vk_descriptor_set,
            0,
            nullptr
        );

        // Рисуем наши 60 индексов
        vkCmdDrawIndexed(
            fd.command_buffer,
            uint32_t(sizeof(indices) / sizeof(indices[0])),
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