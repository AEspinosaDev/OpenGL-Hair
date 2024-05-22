#include "hair_renderer.h"

#define YUKSEL
// #define FXAA
#define GLINT_EXTENT 16

void HairRenderer::init()
{
#pragma region INIT
    Renderer::init();

    // chdir("/home/tony/Dev/Hair-Renderer/");

    m_camera = new Camera(m_window.extent.width, m_window.extent.height, {0.0f, 0.0f, -10.0f});

    m_controller = new Controller(m_camera);

    m_vignette = Mesh::create_screen_quad();
    m_skybox = Mesh::create_cube();

    m_hair = new Mesh();
    m_head = new Mesh();

    m_floor = new Mesh();
    loaders::load_OBJ(m_floor,RESOURCES_PATH"models/plane.obj");
    m_floor->set_scale(50.0f);
    m_floor->set_position({0.0f, -4.0f, 0.0f});

    m_light.light = new PointLight();
    m_light.dummy = new Mesh();
    loaders::load_OBJ(m_light.dummy,RESOURCES_PATH"models/sphere.obj");
    m_light.set_position({6.0f, 3.0f, -6.0f});

#pragma endregion
#pragma region FRAMEBUFFERS

    // Noise text generation pass
    TextureConfig noiseConfig{};
    noiseConfig.format = GL_RED;
    noiseConfig.internalFormat = GL_R8;
    noiseConfig.dataType = GL_UNSIGNED_BYTE;
    noiseConfig.anisotropicFilter = false;
    noiseConfig.magFilter = GL_LINEAR;
    noiseConfig.minFilter = GL_LINEAR;
    noiseConfig.wrapS = GL_REPEAT;
    noiseConfig.wrapT = GL_REPEAT;
    noiseConfig.useMipmaps = true;

    Attachment noiseAttachment{};
    noiseAttachment.texture = new Texture({GLINT_EXTENT, GLINT_EXTENT}, noiseConfig);
    noiseAttachment.attachmentType = GL_COLOR_ATTACHMENT0;

    m_noiseFBO = new Framebuffer({GLINT_EXTENT, GLINT_EXTENT}, {noiseAttachment});
    m_noiseFBO->generate();

    // Creating multisampled forward pass buffer

#ifndef FXAA
    TextureConfig masaaColorConfig{};
    masaaColorConfig.type = TextureType::TEXTURE_2D_MULTISAMPLE;
    masaaColorConfig.format = GL_RGBA;
    masaaColorConfig.internalFormat = GL_RGBA16;
    masaaColorConfig.dataType = GL_UNSIGNED_BYTE;

    Attachment msaaColorAttachment{};
    msaaColorAttachment.texture = new Texture(m_window.extent, masaaColorConfig);
    msaaColorAttachment.attachmentType = GL_COLOR_ATTACHMENT0;
    Attachment msaaDepthAttachment{};
    msaaDepthAttachment.isRenderbuffer = true;
    msaaDepthAttachment.renderbuffer = new Renderbuffer(GL_DEPTH24_STENCIL8);
    msaaDepthAttachment.attachmentType = GL_DEPTH_STENCIL_ATTACHMENT;

    m_forwardFBO = new Framebuffer(m_window.extent, {msaaColorAttachment, msaaDepthAttachment}, m_globalSettings.samples);
    m_forwardFBO->generate();
#else
    TextureConfig colorConfig{};
    colorConfig.type = TextureType::TEXTURE_2D;
    colorConfig.format = GL_RGBA;
    colorConfig.internalFormat = GL_RGBA16;
    colorConfig.dataType = GL_UNSIGNED_BYTE;

    Attachment colorAttachment{};
    colorAttachment.texture = new Texture(m_window.extent, colorConfig);
    colorAttachment.attachmentType = GL_COLOR_ATTACHMENT0;
    Attachment depthAttachment{};
    depthAttachment.isRenderbuffer = true;
    depthAttachment.renderbuffer = new Renderbuffer(GL_DEPTH24_STENCIL8);
    depthAttachment.attachmentType = GL_DEPTH_STENCIL_ATTACHMENT;

    m_forwardFBO = new Framebuffer(m_window.extent, {colorAttachment, depthAttachment});
    m_forwardFBO->generate();
#endif

    // Creating the shadow pass buffer

    // TextureConfig colorDepthConfig{};
    // colorDepthConfig.format = GL_RG;
    // colorDepthConfig.internalFormat = GL_RG32F;
    // colorDepthConfig.dataType = GL_FLOAT;
    // colorDepthConfig.anisotropicFilter = false;
    // colorDepthConfig.magFilter = GL_NEAREST;
    // colorDepthConfig.minFilter = GL_NEAREST;
    // colorDepthConfig.wrapS = GL_CLAMP_TO_BORDER;
    // colorDepthConfig.wrapT = GL_CLAMP_TO_BORDER;
    // colorDepthConfig.useMipmaps = false;
    // colorDepthConfig.borderColor = glm::vec4(1.0f);

    // Attachment colorDepthAttachment{};
    // colorDepthAttachment.texture = new Texture(m_globalSettings.shadowExtent, colorDepthConfig);
    // colorDepthAttachment.attachmentType = GL_COLOR_ATTACHMENT0;

    TextureConfig shadowDepthConfig{};
    shadowDepthConfig.format = GL_DEPTH_COMPONENT;
    shadowDepthConfig.internalFormat = GL_DEPTH_COMPONENT32;
    shadowDepthConfig.dataType = GL_FLOAT;
    shadowDepthConfig.anisotropicFilter = false;
    shadowDepthConfig.magFilter = GL_NEAREST;
    shadowDepthConfig.minFilter = GL_NEAREST;
    shadowDepthConfig.wrapS = GL_CLAMP_TO_BORDER;
    shadowDepthConfig.wrapT = GL_CLAMP_TO_BORDER;
    shadowDepthConfig.useMipmaps = false;
    shadowDepthConfig.borderColor = glm::vec4(1.0f);

    Attachment shadowDepthAttachment{};
    shadowDepthAttachment.texture = new Texture(m_globalSettings.shadowExtent, shadowDepthConfig);
    shadowDepthAttachment.attachmentType = GL_DEPTH_ATTACHMENT;

    m_shadowFBO = new Framebuffer(m_globalSettings.shadowExtent, {shadowDepthAttachment});
    m_shadowFBO->generate();

#pragma endregion
#pragma region SHADER PIPELINES

    // Uniform buffers
    m_cameraUBO = new UniformBuffer(sizeof(CameraUniforms), UBOLayout::CAMERA_LAYOUT);
    m_cameraUBO->generate();

    m_globalUBO = new UniformBuffer(sizeof(GlobalUniforms), UBOLayout::GLOBAL_LAYOUT);
    m_globalUBO->generate();

    GraphicPipeline litPipeline{};
    litPipeline.shader = new Shader(RESOURCES_PATH"shaders/cook-torrance.glsl", ShaderType::LIT);
    litPipeline.shader->set_uniform_block("Camera", UBOLayout::CAMERA_LAYOUT);
    litPipeline.shader->set_uniform_block("Scene", UBOLayout::GLOBAL_LAYOUT);
    Material *headMaterial = new Material(litPipeline);
    headMaterial->set_texture("u_shadowMap", m_shadowFBO->get_attachments().front().texture);
    Texture *skin = new Texture();
    loaders::load_image(skin, RESOURCES_PATH"images/head.png");
    skin->generate();
    headMaterial->set_texture("u_albedoMap", skin, 1);
    m_head->set_material(headMaterial);

    Material *floorMaterial = new Material(litPipeline);
    floorMaterial->set_texture("u_shadowMap", m_shadowFBO->get_attachments().front().texture);
    m_floor->set_material(floorMaterial);

    GraphicPipeline hairPipeline{};
#ifdef MARSCHNER
    hairPipeline.shader = new Shader(RESOURCES_PATH"shaders/strand-marschner.glsl", ShaderType::LIT);
#else
    hairPipeline.shader = new Shader("resources/shaders/strand-kajiya.glsl", ShaderType::LIT);
#endif
    hairPipeline.shader->set_uniform_block("Camera", UBOLayout::CAMERA_LAYOUT);
    hairPipeline.shader->set_uniform_block("Scene", UBOLayout::GLOBAL_LAYOUT);
    Material *hairMaterial = new Material(hairPipeline);
    hairMaterial->set_texture("u_shadowMap", m_shadowFBO->get_attachments().front().texture);
    hairMaterial->set_texture("u_noiseMap", m_noiseFBO->get_attachments().front().texture, 1);
    m_hair->set_material(hairMaterial);

    GraphicPipeline unlitPipeline{};
    unlitPipeline.shader = new Shader(RESOURCES_PATH"shaders/unlit.glsl", ShaderType::UNLIT);
    unlitPipeline.shader->set_uniform_block("Camera", UBOLayout::CAMERA_LAYOUT);
    Material *lightMaterial = new Material(unlitPipeline);
    m_light.dummy->set_material(lightMaterial);

    m_depthPipeline.shader = new Shader(RESOURCES_PATH"shaders/depth.glsl", ShaderType::OTHER);

    m_noisePipeline.shader = new Shader(RESOURCES_PATH"shaders/noise-gen.glsl", ShaderType::OTHER);

#ifdef FXAA
    m_fxaaPipeline.shader = new Shader("resources/shaders/fxaa.glsl", ShaderType::OTHER);
    m_fxaaPipeline.shader->bind();
    m_fxaaPipeline.shader->set_int("u_frame", 0);
    m_fxaaPipeline.shader->unbind();
#endif


    GraphicPipeline skyboxPipeline{};
    skyboxPipeline.shader = new Shader(RESOURCES_PATH"shaders/skybox.glsl", ShaderType::OTHER);
    skyboxPipeline.state.depthFunction = DepthFuncType::LEQUAL;

    Material *skyboxMaterial = new Material(skyboxPipeline);

    TextureConfig skymapConfig{};
    skymapConfig.type = TextureType::TEXTURE_CUBEMAP;
    skymapConfig.format = GL_RGB;
    skymapConfig.internalFormat = GL_RGB32F;
    skymapConfig.dataType = GL_FLOAT;
    skymapConfig.anisotropicFilter = false;
    skymapConfig.wrapS = GL_CLAMP_TO_EDGE;
    skymapConfig.wrapT = GL_CLAMP_TO_EDGE;
    skymapConfig.wrapR = GL_CLAMP_TO_EDGE;

    Texture *skymap = new Texture({2048, 2048}, skymapConfig);
    loaders::load_image(skymap, RESOURCES_PATH"images/room.hdr", true);
    skymap->generate();
    skyboxMaterial->set_texture("u_skymap", skymap);

    m_skybox->set_material(skyboxMaterial);

#pragma endregion

#pragma region MESH LOADING

#ifdef YUKSEL
    // CEM YUKSEL MODELS
    {
        std::thread loadThread1(loaders::load_PLY, m_head, RESOURCES_PATH"models/woman.ply", true, true, false);
        loadThread1.detach();
        m_head->set_rotation({180.0f, -90.0f, 0.0f});
        m_head->set_scale(0.98f);
        std::thread loadThread2(hair_loaders::load_cy_hair, m_hair, RESOURCES_PATH"models/curly.hair");
        loadThread2.detach();

        // Low poly
        // m_hair->set_scale(0.054f);
        // m_hair->set_position({0.015f, -0.2f, 0.3f});
        // m_hair->set_rotation({-90.0f, 0.0f, 16.7f});

        m_hair->set_scale(0.048f);
        m_hair->set_position({0.015f, 0.3f, 0.1f});
        m_hair->set_rotation({-90.0f, 0.0f, 90.7f});
    }
#else
    // NEURAL HAIRCUT MODELS
    {
        loaders::load_PLY(m_head, "resources/models/head_blender.ply", true, true, false);
        std::thread loadThread1(hair_loaders::load_neural_hair, m_hair, "resources/models/2000000.ply", m_head, true, true, false);
        loadThread1.detach();
    }
#endif

    // Generate noise texture
    noise_pass();
    m_noiseFBO->get_attachments().front().texture->generate_mipmaps();

#pragma endregion
}

void HairRenderer::update()
{
    if (!user_interface_wants_to_handle_input())
        m_controller->handle_keyboard(m_window.ptr, 0, 0, m_time.delta);

    if (m_light.animated)
    {
        float rotationAngle = glm::radians(10.0f * m_time.delta);
        float _x = m_light.light->get_position().x * cos(rotationAngle) - m_light.light->get_position().z * sin(rotationAngle);
        float _z = m_light.light->get_position().x * sin(rotationAngle) + m_light.light->get_position().z * cos(rotationAngle);

        m_light.light->set_position({_x, m_light.light->get_position().y, _z});
        m_light.dummy->set_position(m_light.light->get_position());
    }
}

void HairRenderer::draw()
{

    // Setup UBOs
    CameraUniforms camu;
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
    ShadowConfig shadow = m_light.light->get_shadow_config();
    glm::mat4 lp = glm::perspective(glm::radians(shadow.fov),
                                    1.0f,
                                    shadow.nearPlane,
                                    shadow.farPlane);
    glm::mat4 lv = glm::lookAt(m_light.light->get_position(), shadow.target, glm::vec3(0, 1, 0));
    globu.shadowConfig = {shadow.bias, shadow.pcfKernel, shadow.cast, shadow.kernelRadius};
    globu.lightViewProj = lp * lv;
    m_globalUBO->cache_data(sizeof(GlobalUniforms), &globu);

    if (m_light.light->get_cast_shadows())
        shadow_pass();

    forward_pass();

    postprocess_pass();
}

#pragma region FORWARD PASS
void HairRenderer::forward_pass()
{
    // glPushDebugGroupKHR(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Begin Section");

    // glPopDebugGroupKHR();

    m_forwardFBO->bind();
    Framebuffer::clear_color_depth_bit();

    resize_viewport(m_window.extent);

    // ----- Draw ----

    MaterialUniforms headu;
    headu.mat4Types["u_model"] = m_head->get_model_matrix();
    headu.vec3Types["u_albedo"] = m_headSettings.skinColor;
    headu.boolTypes["u_hasAlbedoTex"] = m_headSettings.useAlbedoTexture;
    m_head->get_material()->set_uniforms(headu);

    m_head->draw();

    MaterialUniforms hairu;
#ifdef MARSCHNER
    hairu.vec3Types["u_hair.baseColor"] = m_hairSettings.baseColor;
    hairu.floatTypes["u_hair.specular"] = m_hairSettings.specular;
    hairu.floatTypes["u_hair.roughness"] = m_hairSettings.roughness;
    hairu.floatTypes["u_hair.scatter"] = m_hairSettings.scatter;
    hairu.floatTypes["u_hair.shift"] = m_hairSettings.shift;
    hairu.floatTypes["u_hair.ior"] = m_hairSettings.ior;
    hairu.boolTypes["u_hair.r"] = m_hairSettings.r;
    hairu.boolTypes["u_hair.tt"] = m_hairSettings.tt;
    hairu.boolTypes["u_hair.trt"] = m_hairSettings.trt;
    hairu.boolTypes["u_hair.glints"] = m_hairSettings.glints;
#else
    hairu.vec3Types["u_albedo"] = m_hairSettings.color;
    hairu.vec3Types["u_spec1"] = m_hairSettings.specColor1;
    hairu.floatTypes["u_specPwr1"] = m_hairSettings.specPower1;
    hairu.vec3Types["u_spec2"] = m_hairSettings.specColor2;
    hairu.floatTypes["u_specPwr2"] = m_hairSettings.specPower2;
#endif
    hairu.floatTypes["u_thickness"] = m_hairSettings.thickness;
    hairu.mat4Types["u_model"] = m_hair->get_model_matrix();
    hairu.vec3Types["u_camPos"] = m_camera->get_position();
    m_hair->get_material()->set_uniforms(hairu);

    m_hair->draw(true, GL_LINES);

    MaterialUniforms dummyu;
    m_light.dummy->set_position(m_light.light->get_position());
    dummyu.mat4Types["u_model"] = m_light.dummy->get_model_matrix();
    dummyu.boolTypes["u_useVertexColor"] = false;
    dummyu.vec3Types["u_baseColor"] = glm::vec3(1.0f);
    m_light.dummy->get_material()->set_uniforms(dummyu);

    m_light.dummy->draw();

    // MaterialUniforms flooru;
    // flooru.mat4Types["u_model"] = m_floor->get_model_matrix();
    // flooru.vec3Types["u_albedo"] = glm::vec3(1.0);
    // m_floor->get_material()->set_uniforms(flooru);

    // m_floor->draw();

    m_skybox->set_rotation({0.0, m_globalSettings.enviromentRotation, 0.0});
    MaterialUniforms skyu;
    skyu.mat4Types["u_viewProj"] = m_camera->get_projection() * glm::mat4(glm::mat3(m_camera->get_view())); // Take out the transform
    skyu.mat4Types["u_model"] = m_skybox->get_model_matrix();
    m_skybox->get_material()->set_uniforms(skyu);

    m_skybox->draw();
}
#pragma endregion

#pragma region SHADOW PASS
void HairRenderer::shadow_pass()
{

    m_shadowFBO->bind();
    Framebuffer::clear_depth_bit();
    Framebuffer::enable_depth_writes(true);
    Framebuffer::enable_depth_test(true);

    resize_viewport(m_globalSettings.shadowExtent);

    m_depthPipeline.shader->bind();

    m_depthPipeline.shader->set_mat4("u_model", m_head->get_model_matrix());
    m_depthPipeline.shader->set_bool("u_isHair", false);
    m_head->draw(false);

    // m_depthPipeline.shader->set_mat4("u_model", m_floor->get_model_matrix());
    // m_floor->draw(false);

    m_depthPipeline.shader->set_mat4("u_model", m_hair->get_model_matrix());
    m_depthPipeline.shader->set_bool("u_isHair", true);

    m_hair->draw(false, GL_LINES);

    m_depthPipeline.shader->unbind();
}
#pragma endregion
#pragma region NOISE PASS
void HairRenderer::noise_pass()
{
    m_noiseFBO->bind();

    resize_viewport({GLINT_EXTENT, GLINT_EXTENT});

    m_noisePipeline.shader->bind();

    m_vignette->draw(false);

    m_noisePipeline.shader->unbind();
}

#pragma endregion
#pragma region POST PROCESS PASS
void HairRenderer::postprocess_pass()
{
#ifndef FXAA
    Framebuffer::blit(m_forwardFBO, nullptr, GL_COLOR_BUFFER_BIT, GL_NEAREST, m_window.extent, m_window.extent);
#else
    Framebuffer::bind_default();

    m_fxaaPipeline.shader->bind();

    m_forwardFBO->get_attachments().front().texture->bind(); 

    m_vignette->draw(false);

    m_fxaaPipeline.shader->unbind();
#endif
}

#pragma endregion

void HairRenderer::setup_user_interface_frame()
{
    Renderer::setup_user_interface_frame();

    ImGui::NewFrame();

    // ImGui::ShowDemoWindow(&m_globalSettings.showUI);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(420, 920), ImGuiCond_Once);
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
#ifdef MARSCHNER
    ImGui::ColorEdit3("Base color", (float *)&m_hairSettings.baseColor);
    ImGui::DragFloat("Specular", &m_hairSettings.specular, .05f, 0.0f, 30.0f);
    ImGui::DragFloat("Roughness", &m_hairSettings.roughness, .05f, 0.0f, 1.0f);
    ImGui::DragFloat("Scatter", &m_hairSettings.scatter, 1.f, 0.0f, 1000.0f);
    ImGui::DragFloat("Shift", &m_hairSettings.shift, -0.05f, 0.0F ,2.0F );
    ImGui::DragFloat("IOR", &m_hairSettings.ior, 0.01f, 0.0f, 10.0f);
    ImGui::Checkbox("Glints", &m_hairSettings.glints);
    ImGui::Spacing();
    ImGui::Checkbox("R Lobe", &m_hairSettings.r);
    ImGui::Checkbox("TT Lobe", &m_hairSettings.tt);
    ImGui::Checkbox("TRT Lobe", &m_hairSettings.trt);
#else
    ImGui::ColorEdit3("Base color", (float *)&m_hairSettings.color);
    ImGui::DragFloat("Specular 1 power", &m_hairSettings.specPower1, 1.0f, 0.0f, 240.0f);
    ImGui::DragFloat("Specular 2 power", &m_hairSettings.specPower2, 1.0f, 0.0f, 240.0f);
#endif
    ImGui::Separator();
    ImGui::SeparatorText("Head Settings");
    gui::draw_transform_widget(m_head);
    ImGui::ColorEdit3("Skin color", (float *)&m_headSettings.skinColor);
    ImGui::Checkbox("Use texture", &m_headSettings.useAlbedoTexture);
    ImGui::Separator();
    ImGui::SeparatorText("Lighting Settings");
    ImGui::ColorEdit3("Clear color", (float *)&m_globalSettings.ambientColor);
    ImGui::DragFloat("Enviroment rotation", &m_globalSettings.enviromentRotation, 1.0f, -180.0f, 180.0f);
    ImGui::DragFloat("Enviroment intensity", &m_globalSettings.ambientStrength, 0.1f, 0.0f, 10.0f);
    gui::draw_light_widget(m_light.light);
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
