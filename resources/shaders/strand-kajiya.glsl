#shader vertex
#version 460 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 tangent;
layout(location = 3) in vec3 uv;
layout(location = 4) in vec3 color;

layout (std140) uniform Camera
{
    mat4 viewProj;
    mat4 modelView;
    mat4 view;
}u_camera;

uniform mat4 u_model;
uniform vec3 u_lightPos;

out vec3 v_lightPos;
out vec3 v_color;
out vec3 v_tangent;

void main() {

    gl_Position = u_camera.viewProj * u_model * vec4(position, 1.0);

    v_tangent = normalize(mat3(transpose(inverse(u_camera.view*u_model))) * tangent);
    v_color = color;
    v_lightPos = (u_camera.view * vec4(u_lightPos,1.0)).xyz;

}

#shader geometry
#version 460 core

// #define NORMAL_MAPPING

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

in vec3 v_color[];
in vec3 v_lightPos[];
in vec3 v_tangent[];

out vec3 g_pos;
out vec3 g_normal;
out vec2 g_uv;
out vec3 g_dir;
out vec3 g_color;
out vec3 g_lightPos;
out vec3 g_origin;
#ifdef NORMAL_MAPPING
out mat3 g_TBN;
#endif

uniform float u_thickness;

void emitQuadPoint(vec4 origin, 
                  vec4 right,
                  float offset,
                  vec3 forward, 
                  vec3 normal, 
                  vec2 uv,
                  int id){
  
        gl_Position  = origin + right * offset;
        g_pos = gl_Position.xyz;
        g_uv = uv;
        g_color = v_color[id];
        g_lightPos = v_lightPos[id];
        g_normal = normal;
        g_dir = v_tangent[id];
        g_origin = origin.xyz;

        //In case of normal mapping
#ifdef NORMAL_MAPPING
        g_TBN = mat3(right.xyz, forward, g_normal);
#endif

        EmitVertex();
}

void main() {
  
        vec4 startPoint = gl_in[0].gl_Position;
        vec4 endPoint = gl_in[1].gl_Position;

        vec4 view0 = -startPoint;
        vec4 view1 = -endPoint;

        vec3 dir0 = v_tangent[0];
        vec3 dir1 = v_tangent[1];

        vec4 right0 = normalize(vec4(cross(dir0.xyz,view0.xyz),0.0));
        vec4 right1 = normalize(vec4(cross(dir1.xyz,view1.xyz),0.0));

        vec3 normal0 = normalize(cross(dir0.xyz,right0.xyz));
        vec3 normal1 = normalize(cross(dir1.xyz,right1.xyz));

        float halfLength = u_thickness*0.5;

        emitQuadPoint(startPoint,right0,halfLength,dir0,normal0,vec2(1.0,0.0),0);
        emitQuadPoint(endPoint,right1,halfLength,dir1,normal1,vec2(1.0,1.0),1);
        emitQuadPoint(startPoint,-right0,halfLength,dir0,normal0,vec2(-1.0,0.0),0);
        emitQuadPoint(endPoint,-right1,halfLength,dir1,normal1,vec2(-1.0,1.0),1);

}

#shader fragment
#version 460 core

// #define NORMAL_MAPPING

in vec3 g_color;
in flat vec3 g_lightPos;

in vec3 g_pos;
in vec3 g_normal;
in vec2 g_uv;
in vec3 g_dir;
in vec3 g_origin;
#ifdef NORMAL_MAPPING
in mat3 g_TBN;
#endif

// ---Hair--
uniform vec3 u_albedo;
uniform vec3 u_spec1;
uniform vec3 u_spec2;
uniform float u_specPwr1;
uniform float u_specPwr2;
uniform float u_thickness;
uniform sampler2D u_normalText;

out vec4 FragColor;

vec3 sh_normal;
float halfLength = u_thickness*0.5f;



float computePointInCircleSurface(float u,float radius) {
    return sqrt(1.0 - u * u);
}

void computeNormal(){
    float offsetMag = computePointInCircleSurface(g_uv.x,halfLength);
    vec3 offsetPos = g_pos + g_normal * (halfLength/offsetMag);
    
    sh_normal = normalize(offsetPos-g_origin);
}
#ifdef NORMAL_MAPPING
void computeNormalFromTexture(){
    vec3 textNormal = texture(u_normalText, g_uv).rgb;
    textNormal = textNormal * 2.0 - 1.0;   
    sh_normal = normalize(g_TBN * textNormal); 
}
#endif

vec3 shiftTangent(vec3 T, vec3 N, float shift){

  vec3 shiftedT = T+shift*N;
  return normalize(shiftedT);
}
float strandSpecular(vec3 T, vec3 V, vec3 L, float exponent){
  vec3 H = normalize(L + V);
  float u =dot(T,L); //Lambertian
  float t =dot(T,V);
  float dotTH =dot(T,H); //Spec
  float sinTH = sin(acos(dotTH));

  float dirAtten = smoothstep(-1.0, 0.0,
    dotTH);
   
  //return pow(u*t+sin(acos(u))*sin(acos(t)),exponent);
  return dirAtten * pow(sinTH, exponent);
}


//Scheuermann / Kajiya. Kay
vec3 computeLighting(){
  vec3 L = normalize(g_lightPos- g_pos);
  vec3 V = normalize(-g_pos);
  vec3 D = normalize(g_dir);
//   vec3 halfVector = normalize(lightDir + viewDir);

  
//   float shift = uHasTiltText ? texture(uTiltText,_uv).r : 0.0;
  float shift = 0.0;


  // vec3 t1 = shiftTangent(T, N, 0.0 + shift);
  // vec3 t2 = shiftTangent(T,  N, 0.0 + shift);

  vec3 ambient = 0.2*u_albedo;
  vec3 diffuse = u_albedo*clamp(dot(sh_normal,L),0.0,1.0);
  // vec3 diffuse = u_albedo;

  
  vec3 specular = clamp(u_spec1 * strandSpecular(D, V,L, u_specPwr1),0.0,0.3);
  //vec3 specular = vec3(0.0);
    
//   float highlight = uHasHighlightText ? texture(uHighlightText,_uv).r:1.0;
//   specular += clamp(u_spec2*highlight* strandSpecular(t2,V,L,uSpecularPower2),0.0,1.0);
    float lightIntensity = 1.0;
  return ambient+(diffuse)*lightIntensity;//Include lambertian with different 

}


void main() {

    computeNormal();
    FragColor = vec4(computeLighting(), 1.0);

    //  FragColor = vec4(sh_normal, 1.0);

}