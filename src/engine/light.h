#ifndef __LIGHT__
#define __LIGHT__

#include "shader.h"
#include "object3D.h"
#include "material.h"

GLIB_NAMESPACE_BEGIN

typedef enum LightType
{
    POINT,
    DIRECTIONAL,
    SPORT,
    AREA
} LightType;

// Shadows
struct ShadowConfig
{
    bool cast{true};
    float nearPlane{0.5f};
    float farPlane{100.0f};
    float fov{60.0f};
    glm::vec3 target{0.0f, 0.0f, 0.0f};

    // float bias{0.0001f};
    float bias{0.0002f};
    bool angleDependableBias{false};
    int pcfKernel{7};
    float kernelRadius{2.0};
    // Texture *map;
};

class Light : public Object3D
{
protected:
    glm::vec3 m_color;

    float m_intensity;

    ShadowConfig m_shadowData;

    const LightType m_lighType;

public:
    Light(const char *name, LightType type, glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f), float intensity = 1.0f) : Object3D(name, glm::vec3{0.0f}, LIGHT), m_color(color),
                                                                                                                     m_intensity(intensity), m_lighType(type) {}
    virtual void cache_uniforms(MaterialUniforms &u) const = 0;

    virtual inline glm::vec3 get_color() const { return m_color; }
    virtual inline void set_color(glm::vec3 c) { m_color = c; }

    virtual inline float get_intensity() const { return m_intensity; }
    virtual inline void set_intensity(float i) { m_intensity = i; }

    virtual inline ShadowConfig get_shadow_config() const { return m_shadowData; }
    virtual inline void set_shadow_config(ShadowConfig data) { m_shadowData = data; }

    virtual inline bool
    get_cast_shadows() const
    {
        return m_shadowData.cast;
    }
    virtual inline void set_cast_shadows(bool o) { m_shadowData.cast = o; }

    // Read only
    // virtual const Texture *const get_shadow_map() const { return m_shadow.map; }
    virtual LightType get_light_type() const { return m_lighType; }
};

// POINT LIGHT

class PointLight : public Light
{
    float m_effectArea;
    float m_decaying;

    static int INSTANCED_POINT_LIGHTS;

public:
    PointLight(glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f), float intensity = 1.0f) : Light("Point Light", LightType::POINT, color, intensity), m_effectArea(12.0f), m_decaying(1.0f) { PointLight::INSTANCED_POINT_LIGHTS++; }

    inline float get_area_of_effect() const { return m_effectArea; }
    inline void set_area_of_effect(float a) { m_effectArea = a; }

    inline float get_decaying() const { return m_decaying; }
    inline void set_decaying(float d) { m_decaying = d; }

    void cache_uniforms(MaterialUniforms &u) const;

    inline static int get_number_of_instances() { return INSTANCED_POINT_LIGHTS; }
};

// DIRECTIONAL LIGHT

class DirectionalLight : public Light
{
    glm::vec3 m_direction;

    static int INSTANCED_DIRECTIONAL_LIGHTS;

public:
    DirectionalLight(glm::vec3 direction, glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f), float intensity = 1.0f) : Light("Directional Light", LightType::DIRECTIONAL, color, intensity), m_direction(direction) { DirectionalLight::INSTANCED_DIRECTIONAL_LIGHTS++; }

    inline glm::vec3 get_direction() const { return m_direction; }
    inline void set_direction(glm::vec3 d) { m_direction = d; }

    void cache_uniforms(MaterialUniforms &u) const;

    inline static int get_number_of_instances() { return INSTANCED_DIRECTIONAL_LIGHTS; }
};

GLIB_NAMESPACE_END

#endif