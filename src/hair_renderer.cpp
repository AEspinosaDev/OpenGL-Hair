#include "hair_renderer.h"

void HairRenderer::init()
{
#pragma region ______________ INIT ______________
    Renderer::init();

    chdir("/home/tony/Dev/OpenGL-Hair/");

    m_camera = new Camera(m_window.extent.width, m_window.extent.height, {0.0f, 0.0f, -10.0f});

    m_controller = new Controller(m_camera);

    m_hair = new Mesh();
    m_head = new Mesh();

    m_floor = new Mesh();
    loaders::load_OBJ(m_floor, "resources/models/plane.obj");
    m_floor->set_scale(10.0f);
    m_floor->set_position({0.0f,-3.0f,0.0f});

    m_light.light = new PointLight();
    m_light.dummy = new Mesh();
    loaders::load_OBJ(m_light.dummy, "resources/models/sphere.obj");
    m_light.set_position({3.0f, 2.0f, -3.0f});

#pragma endregion
#pragma region ______________ SHADER PIPELINES______________

    // Uniform buffers
    m_cameraUBO = new UniformBuffer(sizeof(CameraUniforms), UBOLayout::CAMERA_LAYOUT);
    m_cameraUBO->generate();

    m_globalUBO = new UniformBuffer(sizeof(GlobalUniforms), UBOLayout::GLOBAL_LAYOUT);
    m_globalUBO->generate();

    GraphicPipeline litPipeline{};
    litPipeline.shader = new Shader("resources/shaders/cook-torrance.glsl", ShaderType::LIT);
    litPipeline.shader->set_uniform_block("Camera", UBOLayout::CAMERA_LAYOUT);
    litPipeline.shader->set_uniform_block("Scene", UBOLayout::GLOBAL_LAYOUT);
    Material *headMaterial = new Material(litPipeline);
    m_head->set_material(headMaterial);
    Material *floorMaterial = new Material(litPipeline);
    m_floor->set_material(floorMaterial);

    GraphicPipeline hairPipeline{};
    hairPipeline.shader = new Shader("resources/shaders/strand-kajiya.glsl", ShaderType::LIT);
    hairPipeline.shader->set_uniform_block("Camera", UBOLayout::CAMERA_LAYOUT);
    hairPipeline.shader->set_uniform_block("Scene", UBOLayout::GLOBAL_LAYOUT);
    Material *hairMaterial = new Material(hairPipeline);
    m_hair->set_material(hairMaterial);

    GraphicPipeline unlitPipeline{};
    unlitPipeline.shader = new Shader("resources/shaders/unlit.glsl", ShaderType::UNLIT);
    unlitPipeline.shader->set_uniform_block("Camera", UBOLayout::CAMERA_LAYOUT);
    Material *lightMaterial = new Material(unlitPipeline);
    m_light.dummy->set_material(lightMaterial);

#pragma endregion
#pragma region ______________ FRAMEBUFFERS ______________

    // Creating the shadow pass buffer

    TextureConfig depthConfig{};
    depthConfig.format = GL_DEPTH_COMPONENT;
    depthConfig.internalFormat = GL_DEPTH_COMPONENT16;
    depthConfig.dataType = GL_FLOAT;
    depthConfig.anisotropicFilter = false;
    depthConfig.magFilter = GL_NEAREST;
    depthConfig.minFilter = GL_NEAREST;
    // depthConfig.useMipmaps = false;
    depthConfig.wrapS = GL_CLAMP_TO_BORDER;
    depthConfig.wrapT = GL_CLAMP_TO_BORDER;
    depthConfig.borderColor = glm::vec4(1.0f);

    Attachment depthAttachment{};
    depthAttachment.texture = new Texture(m_globalSettings.shadowExtent, depthConfig);
    depthAttachment.attachmentType = GL_DEPTH_ATTACHMENT;

    m_shadowFBO = new Framebuffer(m_globalSettings.shadowExtent, {depthAttachment});
    m_shadowFBO->generate();

#pragma endregion

#pragma region ______________ MESH LOADING ______________
#define YUKSEL
#ifdef YUKSEL
    // CEM YUKSEL MODELS
    {
        std::thread loadThread1(loaders::load_PLY, m_head, "resources/models/woman.ply", true, true, false);
        loadThread1.detach();
        m_head->set_rotation({180.0f, -90.0f, 0.0f});
        std::thread loadThread2(hair_loaders::load_cy_hair, m_hair, "resources/models/natural.hair");
        loadThread2.detach();
        m_hair->set_scale(0.054f);
        m_hair->set_position({0.015f, -0.09f, 0.2f});
        m_hair->set_rotation({-90.0f, 0.0f, 16.7f});
    }
#else
    // NEURAL HAIRCUT MODELS
    {
        loaders::load_PLY(m_head, "resources/models/head_blender.ply", true, true, false);
        std::thread loadThread1(hair_loaders::load_neural_hair, m_hair, "resources/models/2000000.ply", m_head, true, true, false);
        loadThread1.detach();
    }
#endif
#pragma endregion
}

void HairRenderer::update()
{
    if (!user_interface_wants_to_handle_input())
        m_controller->handle_keyboard(m_window.ptr, 0, 0, m_time.delta);
}

void HairRenderer::draw()
{

#pragma region ______________ SHADOW PASS ______________

    m_shadowFBO->bind();
    
    resize_viewport(m_globalSettings.shadowExtent);

    CameraUniforms camu;
    camu.vp = m_camera->get_projection() * m_camera->get_view();
    camu.mv = m_camera->get_view() * m_head->get_model_matrix();
    camu.v = m_camera->get_view();
    m_cameraUBO->cache_data(sizeof(CameraUniforms), &camu);

    




#pragma endregion
#pragma region ______________ FORWARD PASS ______________

    Framebuffer::bind_default();

    resize_viewport(m_window.extent);

    camu.vp = m_camera->get_projection() * m_camera->get_view();
    camu.mv = m_camera->get_view() * m_head->get_model_matrix();
    camu.v = m_camera->get_view();
    m_cameraUBO->cache_data(sizeof(CameraUniforms), &camu);
    GlobalUniforms globu;
    globu.ambient = {m_globalSettings.ambientColor,
                     m_globalSettings.ambientStrength};
    glm::vec3 lightViewSpace = camu.v * glm::vec4(m_light.light->get_position(), 1.0f); // Transform to view space
    globu.lightPos = {lightViewSpace, 1.0f};
    globu.lightColor = {m_light.light->get_color(), m_light.light->get_intensity()};
    m_globalUBO->cache_data(sizeof(GlobalUniforms), &globu);

    // ----- Draw ----
    Framebuffer::clear_color_depth_bit();

    MaterialUniforms headu;
    headu.mat4Types["u_model"] = m_head->get_model_matrix();
    headu.vec3Types["u_albedo"] = m_headSettings.skinColor;
    m_head->get_material()->set_uniforms(headu);

    m_head->draw();

    MaterialUniforms hairu;
    hairu.mat4Types["u_model"] = m_hair->get_model_matrix();
    hairu.vec3Types["u_albedo"] = m_hairSettings.color;
    hairu.vec3Types["u_spec1"] = m_hairSettings.specColor1;
    hairu.floatTypes["u_specPwr1"] = m_hairSettings.specPower1;
    hairu.vec3Types["u_spec2"] = m_hairSettings.specColor2;
    hairu.floatTypes["u_specPwr2"] = m_hairSettings.specPower2;
    hairu.floatTypes["u_thickness"] = m_hairSettings.thickness;
    hairu.vec3Types["u_camPos"] = m_camera->get_position();

    m_hair->get_material()->set_uniforms(hairu);

    m_hair->draw(true, GL_LINES);

    MaterialUniforms dummyu;
    m_light.dummy->set_position(m_light.light->get_position());
    dummyu.mat4Types["u_model"] = m_light.dummy->get_model_matrix();
    dummyu.boolTypes["u_useVertexColor"] = false;
    m_light.dummy->get_material()->set_uniforms(dummyu);

    m_light.dummy->draw();

    MaterialUniforms flooru;
    flooru.mat4Types["u_model"] = m_floor->get_model_matrix();
    flooru.vec3Types["u_albedo"] = glm::vec3(1.0);
    m_floor->get_material()->set_uniforms(flooru);

    m_floor->draw();

#pragma endregion
}

void HairRenderer::setup_user_interface_frame()
{
    Renderer::setup_user_interface_frame();

    ImGui::NewFrame();

    // ImGui::ShowDemoWindow(&m_globalSettings.showUI);

    ImGui::Begin("Settings", &m_globalSettings.showUI); // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
    ImGui::SeparatorText("Profiler");
    ImGui::Text(" %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::Separator();
    ImGui::SeparatorText("Global Settings");
    if (ImGui::Checkbox("V-Sync", &m_settings.vSync))
    {
        set_v_sync(m_settings.vSync);
    }
    ImGui::Separator();
    ImGui::SeparatorText("Hair Settings");
    gui::draw_transform_widget(m_hair);
    ImGui::DragFloat("Strand thickness", &m_hairSettings.thickness, 0.001f, 0.001f, 0.05f);
    ImGui::ColorEdit3("Strand color", (float *)&m_hairSettings.color);
    ImGui::Separator();
    ImGui::SeparatorText("Head Settings");
    gui::draw_transform_widget(m_head);
    ImGui::ColorEdit3("Skin color", (float *)&m_headSettings.skinColor);
    ImGui::Separator();
    ImGui::SeparatorText("Lighting Settings");
    ImGui::ColorEdit3("Ambient color", (float *)&m_globalSettings.ambientColor);
    ImGui::DragFloat("Ambient intensity", &m_globalSettings.ambientStrength, 0.1f, 0.0f, 10.0f);
    float pointIntensity = m_light.light->get_intensity();
    if (ImGui::DragFloat("Point intensity", &pointIntensity, 0.1f, 0.0f, 10.0f))
    {
        m_light.light->set_intensity(pointIntensity);
    };
    glm::vec3 pointColor = m_light.light->get_color();
    if (ImGui::ColorEdit3("Point color", (float *)&pointColor))
    {
        m_light.light->set_color(pointColor);
    };
    gui::draw_transform_widget(m_light.light);

    ImGui::Separator();

    ImGui::End();

    ImGui::Render();
}

void HairRenderer::setup_window_callbacks()
{
    glfwSetWindowUserPointer(m_window.ptr, this);

    glfwSetKeyCallback(m_window.ptr, [](GLFWwindow *w, int key, int scancode, int action, int mods)
                       { static_cast<HairRenderer *>(glfwGetWindowUserPointer(w))->key_callback(w, key, scancode, action, mods); });
    glfwSetCursorPosCallback(m_window.ptr, [](GLFWwindow *w, double x, double y)
                             { static_cast<HairRenderer *>(glfwGetWindowUserPointer(w))->mouse_callback(w, x, y); });
    glfwSetFramebufferSizeCallback(m_window.ptr, [](GLFWwindow *w, int width, int height)
                                   { static_cast<HairRenderer *>(glfwGetWindowUserPointer(w))->resize_callback(w, width, height); });
}
