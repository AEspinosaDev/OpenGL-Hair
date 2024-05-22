#stage vertex
#version 460 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 tangent;
layout(location = 3) in vec3 uv;
layout(location = 4) in vec3 color;


uniform mat4 u_model;

out vec3 v_color;
out vec3 v_tangent;
out int v_id;

void main() {

    gl_Position =  u_model * vec4(position, 1.0);

    v_tangent = normalize(mat3(transpose(inverse(u_model))) * tangent);
    v_color = color;
    v_id = gl_VertexID;

}

#stage geometry
#version 460 core


layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

in vec3 v_color[];
in vec3 v_tangent[];
in int v_id[];

layout (binding = 0) uniform Camera
{
    mat4 viewProj;
    mat4 modelView;
    mat4 view;

}u_camera;

out vec3 g_pos;
out vec3 g_modelPos;
out vec3 g_normal;
out vec2 g_uv;
out vec3 g_dir;
out vec3 g_color;
out vec3 g_origin;
out int g_id;

uniform float u_thickness;
uniform vec3 u_camPos;
uniform mat4 u_model;

void emitQuadPoint(vec4 origin, 
                  vec4 right,
                  float offset,
                  vec3 forward, 
                  vec3 normal, 
                  vec2 uv,
                  int id){
  
        vec4 newPos = origin + right * offset; //Model space
        gl_Position =  u_camera.viewProj * newPos;
        g_dir = normalize(mat3(transpose(inverse(u_camera.view))) * v_tangent[id]);
        g_color = v_color[id];
        g_pos = (u_camera.view *  newPos).xyz;
        g_modelPos = newPos.xyz;
        g_uv = uv;
        g_normal =  normalize(mat3(transpose(inverse(u_camera.view))) * normal);
        g_origin = (u_camera.view * origin).xyz; 
        g_id = v_id[0];

        EmitVertex();
}

void main() {
  
        //Model space --->>>

        vec4 startPoint = gl_in[0].gl_Position;
        vec4 endPoint = gl_in[1].gl_Position;

        vec4 view0 = vec4(u_camPos,1.0)-startPoint;
        vec4 view1 = vec4(u_camPos,1.0)-endPoint;

        vec3 dir0 = v_tangent[0];
        vec3 dir1 = v_tangent[1];

        vec4 right0 = normalize(vec4(cross(dir0.xyz,view0.xyz),0.0));
        vec4 right1 = normalize(vec4(cross(dir1.xyz,view1.xyz),0.0));

        vec3 normal0 = normalize(cross(right0.xyz,dir0.xyz));
        vec3 normal1 = normalize(cross(right1.xyz,dir1.xyz));

        //<<<----

        float halfLength = u_thickness*0.5;

        emitQuadPoint(startPoint,right0,halfLength,dir0,normal0,vec2(1.0,0.0),0);
        emitQuadPoint(endPoint,right1,halfLength,dir1,normal1,vec2(1.0,1.0),1);
        emitQuadPoint(startPoint,-right0,halfLength,dir0,normal0,vec2(0.0,0.0),0);
        emitQuadPoint(endPoint,-right1,halfLength,dir1,normal1,vec2(0.0,1.0),1);

}

#stage fragment
#version 460 core


in vec3 g_color;

in vec3 g_pos;
in vec3 g_modelPos;
in vec3 g_normal;
in vec2 g_uv;
in vec3 g_dir;
in vec3 g_origin;
in flat int g_id;




layout (binding = 0) uniform Camera
{
    mat4 viewProj;
    mat4 modelView;
    mat4 view;

}u_camera;

layout (binding = 1) uniform Scene
{
    vec3 ambientColor;
    float ambientIntensity;
    vec4 lightPos;
    vec3 lightColor;
    float lightIntensity;

    float shadowBias;
    float pcfKernelSize;
    float castShadow;

    float kernelRadius;

    mat4 lightViewProj;
}u_scene;

// ---Hair--
struct HairMaterial{
    vec3 baseColor;
    float specular;
    float roughness;
    float scatter;
    float shift;
    float ior;

    bool glints; 
    bool useScatter;

    bool r;
    bool tt;
    bool trt;
};

uniform float u_thickness;
uniform HairMaterial u_hair;
uniform sampler2D u_shadowMap;
uniform sampler2D u_noiseMap;

float scatterWeight = 0.0;

//Constant
const float PI = 3.14159265359;




out vec4 fragColor;


vec3 shiftTangent(vec3 T, vec3 N, float shift){
  vec3 shiftedT = T+shift*N;
  return normalize(shiftedT);
}

float linearizeDepth(float depth, float near, float far) {
    float z = depth * 2.0 - 1.0; 
    float linearZ = (2.0 * near * far) / (far + near - z * (far - near));
    return (linearZ - near) / (far - near);
}
///Fresnel
float fresnelSchlick(float ior, float cosTheta) {
    float F0 = ((1.0-ior)*(1.0-ior))/((1.0+ior)*(1.0+ior));
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Azimuthal REFLECTION
float NR(vec3 wi, vec3 wo, float cosPhi){
  float cosHalfPhi = sqrt(0.5+0.5*cosPhi);

  // return (0.25*cosHalfPhi)*fresnelSchlick(u_hair.ior,sqrt(0.5*(1+dot(wi,wo)))); //Frostbite
  return (0.25*cosHalfPhi)*fresnelSchlick(u_hair.ior,sqrt(0.5+0.5*dot(wi,wo))); //Epic
}
//Attenuattion
vec3 A(float f, int p, vec3 t){ //fresnel, Stage, Absorbtion
  return (1-f)*(1-f)*pow(f,p-1)*pow(t,vec3(p));
}

// Azimuthal TRANSMITION
vec3 NTT(float sinThetaD, float cosThetaD, float cosPhi){

  // float _ior = sqrt(u_hair.ior*u_hair.ior - sinThetaD* sinThetaD)/ cosThetaD; //Original
  float _ior =  1.19 / cosThetaD + 0.36 * cosThetaD; //Fit EPIC
  float a = 1/_ior;

  float cosHalfPhi = sqrt(0.5+0.5*cosPhi); 
  float h = 1 + a *(0.6-0.8*cosPhi) *  cosHalfPhi; //Fit EPIC

  float power = sqrt(1-h*h*a*a)/(2*cosThetaD);

  float D = exp(-3.65*cosPhi-3.98); //Intensity distribution
  vec3 T = pow(u_hair.baseColor,vec3(power)); //Absortion
  float F = fresnelSchlick(u_hair.ior,cosThetaD*sqrt(1-h*h)); //Fresnel

  return A(F,1,T)*D;
}
// Azimuthal DOUBLE REFLECTION
vec3 NTRT(float sinThetaD, float cosThetaD, float cosPhi){
  // float _ior = sqrt(u_hair.ior*u_hair.ior - sinThetaD* sinThetaD)/ cosThetaD; //Original
// float gamma = asin(h/_ior);
  //  vec3 T = exp(-2*u_hair.baseColor*(1+cos(2*gamma)));
  // float scale = clamp(1.5*(1-2.0*u_hair.roughness),0.0,1.0); //Frostbite scale term

  float h = sqrt(3)/2; //Por que si
  float D = exp(17*cosPhi-16.78); //Intensity distribution
   vec3 T = pow(u_hair.baseColor,vec3(0.8/cosThetaD));
  float F = fresnelSchlick(u_hair.ior,cosThetaD*sqrt(1-h*h)); //Fresnel CONSTANT ?

  return A(F,2,T)*D;
}

//Longitudinal TERM
  float M(float sinTheta, float roughness){
  // return exp(-(sinTheta*sinTheta)/(2*roughness*roughness))/sqrt(2*PI*roughness); //Frostbite 
  return 1.0/(roughness*sqrt(2*PI))*exp((-sinTheta*sinTheta)/(2.0*roughness*roughness)); //Epic. sintheta = sinThetaWi+sinThetaV-alpha
}

//Real-time Marschnerr
vec3 computeLighting(){

  //--->>>View space
  vec3 wi = normalize(u_scene.lightPos.xyz- g_pos);   //Light vector
  vec3 n = g_normal;                                 //Strand shading normal
  vec3 v = normalize(-g_pos);                         //Camera vector
  vec3 u = normalize(g_dir);                          //Strand tangent/direction

  vec3 radiance = u_scene.lightColor*u_scene.lightIntensity;
  
  //Betas
  float betaR = u_hair.roughness*u_hair.roughness;
  float betaTT = 0.5*betaR;
  float betaTRT = 2.0*betaR;

  if(u_hair.glints){
    float glint = texture(u_noiseMap, vec2(g_id/50000.0,g_uv.y)).r; //Make them move tangent
    u = shiftTangent(u,n,(glint)*0.1);
  }

  //Theta & Phi
  float sinThetaWi = dot(wi,u);
  float sinThetaV = dot(v,u);
  vec3 wiPerp = wi - sinThetaWi * u;
  vec3 vPerp = v - sinThetaV * u;
  float cosPhiD = dot(wiPerp,vPerp)/sqrt(dot(wiPerp,wiPerp)*dot(vPerp,vPerp));

  // Diff
  float thetaD = (asin(sinThetaWi)-asin(sinThetaV))*0.5;
  float cosThetaD = cos(thetaD);
  float sinThetaD = sin(thetaD);

  float R = u_hair.r ? M(sinThetaWi+sinThetaV-u_hair.shift*2.0, betaR )*NR(wi,v,cosPhiD): 0.0; 
  vec3 TT = u_hair.tt ? M(sinThetaWi+sinThetaV+u_hair.shift,betaTT)*NTT(sinThetaD,cosThetaD,cosPhiD): vec3(0.0); 
  vec3 TRT = u_hair.trt ? M(sinThetaWi+sinThetaV+u_hair.shift*4.0,betaTRT)*NTRT(sinThetaD,cosThetaD,cosPhiD): vec3(0.0); 


  vec3 albedo = u_hair.baseColor;
  vec3 specular = (R*u_hair.specular+TT+TRT*u_hair.specular);

  return (specular+albedo) * radiance;
  
  // return specular/(cosThetaD*cosThetaD)  *radiance;

}


float filterPCF(int kernelSize, vec3 coords, float bias) {

    int edge = kernelSize / 2;
    vec2 texelSize = 1.0 / textureSize(u_shadowMap, 0);

    float currentDepth = coords.z;

    float shadow = 0.0;

    for(int x = -edge; x <= edge; ++x) {
        for(int y = -edge; y <= edge; ++y) {
            float pcfDepth = texture(u_shadowMap, vec2(coords.xy + vec2(x, y) * texelSize* u_scene.kernelRadius)).r;

            //Scatter weight
            float currentZ = linearizeDepth(currentDepth,0.5,100.0);
            float shadowZ = linearizeDepth(pcfDepth,0.5,100.0);
            float weight = 1.0-clamp(exp(-u_hair.scatter*abs(currentZ-shadowZ)),0.0,1.0);
            scatterWeight += weight;

            shadow += currentDepth - bias > pcfDepth ? 1.0* (u_hair.useScatter ? weight: 1.0) : 0.0;
        }
    }
    scatterWeight /= (kernelSize * kernelSize);
    return shadow /= (kernelSize * kernelSize);

}

float computeShadow(){

    vec4 posLightSpace = u_scene.lightViewProj * vec4(g_modelPos, 1.0);

    vec3 projCoords = posLightSpace.xyz / posLightSpace.w; //For x,y and Z

    projCoords  = projCoords * 0.5 + 0.5;

    if(projCoords.z > 1.0 || projCoords.z < 0.0)
        return 0.0;

    vec3 lightDir = normalize(u_scene.lightPos.xyz - g_pos);
    float bias = max(u_scene.shadowBias *  5.0 * (1.0 - dot(g_dir, lightDir)),u_scene.shadowBias);  //Modulate by angle of incidence
   
    return filterPCF(int(u_scene.pcfKernelSize), projCoords,bias);

}

float getLuminance(vec3 li){
  return 0.2126*li.r + 0.7152*li.g+0.0722*li.b;
}

vec3 multipleScattering(){
  
  vec3 l = normalize(u_scene.lightPos.xyz- g_pos);  //Light vector
  vec3 r = normalize(-g_pos);       
  vec3 u = normalize(g_dir); 
  
  vec3 n = normalize(r - u * dot(u,r));
   
  // float wrapLight = (dot(n,l)+1.0)/(4.0*PI);
  vec3 scattering = pow(u_hair.baseColor/getLuminance(u_scene.lightColor),vec3(scatterWeight));
  return scattering;




}


void main() {

    vec3 color  = computeLighting();
    if(u_scene.castShadow==1.0){
        color*= 1.0 - computeShadow();
        if(u_hair.useScatter)
            color*= multipleScattering();
    }

    vec3 ambient = (u_scene.ambientIntensity * u_scene.ambientColor) * u_hair.baseColor ;
    color+=ambient;

    //Tone Up
    // color = color / (color + vec3(1.0));
    //Gamma Correction
    // const float GAMMA = 2.2;
    //color = pow(color, vec3(1.0 / GAMMA));

    fragColor = vec4(color,1.0f);

}