#ifndef __SETTINGS__
#define __SETTINGS__

#include "engine/core.h"
#include "engine/renderer.h"

#define MARSCHNER

struct UserInterfaceSettings
{
    ImVec2 padding;
    float windowBorder;
    float windowRounding;
};
struct HairSettings
{
    float thickness = 0.002f;
#ifdef MARSCHNER
    glm::vec3 baseColor = glm::vec3(
        68.0f / 255.0f,
        37.0f / 255.0f,
        10.0f / 255.0f);
    float specular = 5.0f;
    float roughness = 0.4f;
    float scatter = 250.0f;
    float shift = 0.12f; // In radians (-5º to -10º) => 0.088 to 0.17 //Not with epic 0.02 does fine
    float ior = 1.55f;

    bool r = true;
    bool tt = true;
    bool trt = true;

    bool glints = false;
    bool useScatter = true;
#else
    // glm::vec3 color = glm::vec3(0.95f, 0.65f, 0.16f);
    glm::vec3 color = glm::vec3(0.6f, 0.078f, 0.078f);
    glm::vec3 specColor1 = glm::vec3(1.0f, 1.0f, 1.0f);
    glm::vec3 specColor2 = color;
    float specPower1 = 210.0f;
    float specPower2 = 8.0f;
#endif
};
struct HeadSettings
{
    glm::vec3 skinColor = glm::vec3(
        225.0f / 255.0f,
        94.0f / 255.0f,
        66.0f / 255.0f);
    bool useAlbedoTexture = false;
};
struct GlobalSettings
{
    bool showUI{true};
    glm::vec3 ambientColor = glm::vec3(1.0f);
    float ambientStrength = 0.3f;
    float enviromentRotation = 0.0f;
    Extent2D shadowExtent = {2048, 2048};
    unsigned int samples = 16;
};

#endif