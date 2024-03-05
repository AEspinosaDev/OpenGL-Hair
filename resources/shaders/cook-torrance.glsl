#shader vertex
#version 460 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 tangent;
layout(location = 3) in vec3 uv;
layout(location = 4) in vec3 color;

uniform mat4 u_viewProj;
uniform mat4 u_modelView;
uniform mat4 u_view;
uniform mat4 u_model;
uniform vec3 u_lightPos;

out vec3 _pos;
out vec3 _normal;
out vec3 _lightPos;

void main() {
    _pos = (u_modelView * vec4(position, 1.0)).xyz;
    _normal = normalize(mat3(transpose(inverse(u_modelView))) * normal);
    _lightPos = (u_view * vec4(u_lightPos, 1.0)).xyz;

    gl_Position = u_viewProj  * u_model * vec4(position, 1.0);

}

#shader fragment
#version 460 core

in vec3 _pos;
in vec3 _normal;
in vec3 _lightPos;

uniform vec3 u_skinColor;

out vec4 FragColor;

//Surface global properties
vec3 g_normal = _normal;
vec3 g_albedo = u_skinColor;
float g_opacity = 1.0;
float g_roughness = 0.8;
float g_metalness = 0.0;
float g_ao = 1.0;

//Constant
const float PI = 3.14159265359;

///Fresnel
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

//Normal Distribution 
//Trowbridge - Reitz GGX
float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}
//Geometry
//Schlick - GGX
float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 computeLighting() {

    //Vector setup
    vec3 lightDir = normalize(_lightPos - _pos);
    vec3 viewDir = normalize(-_pos);
    vec3 halfVector = normalize(lightDir + viewDir); //normalize(viewDir + lightDir);

	//Heuristic fresnel factor
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, g_albedo, g_metalness);

	//Radiance
    // vec3 radiance = light.color * computeAttenuation(light) * light.intensity;
    vec3 radiance = vec3(1.0) ;


	// Cook-Torrance BRDF
    float NDF = distributionGGX(g_normal, halfVector, g_roughness);
    float G = geometrySmith(g_normal, viewDir, lightDir, g_roughness);
    vec3 F = fresnelSchlick(max(dot(halfVector, viewDir), 0.0), F0);

    vec3 kD = vec3(1.0) - F;
    kD *= 1.0 - g_metalness;

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(g_normal, viewDir), 0.0) * max(dot(g_normal, lightDir), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

	// Add to outgoing radiance result
    float lambertian = max(dot(g_normal, lightDir), 0.0);
    return (kD * g_albedo / PI + specular) * radiance * lambertian;

}


void main() {
    
    vec3 color = computeLighting();

    //Ambient component
    const float ambientIntensity = 0.2;
    const vec3 ambientColor = vec3(1.0);
    vec3 ambient = (ambientIntensity * 0.1 * ambientColor) * g_albedo * g_ao;
    color += ambient;

	//Tone Up
    color = color / (color + vec3(1.0));

    //Gamma Correction 
    const float GAMMA = 2.2;
    color = pow(color, vec3(1.0 / GAMMA));

    FragColor = vec4(color, 1.0);

}