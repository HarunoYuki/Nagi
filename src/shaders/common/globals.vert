#version 330

// -------------------------------------------------------------------------------------------------------------------------------
// uniforms.glsl
// -------------------------------------------------------------------------------------------------------------------------------


// Renderer::InitShaders()
uniform vec2 envMapRes;
uniform float envMapTotalSum;
uniform vec2 resolution;
uniform vec2 invTilesNum;
uniform int lightsNum;
uniform int tlasBVHStartOffset;
uniform sampler2D accumTex;
uniform samplerBuffer BVHTex;
uniform isamplerBuffer vertexIndicesTex;
uniform samplerBuffer verticesTex;
uniform samplerBuffer normalsTex;
uniform sampler2D materialsTex;
uniform sampler2D transformsTex;
uniform sampler2D lightsTex;
uniform sampler2DArray textureMapsArrayTex;
uniform sampler2D envMapTex;
uniform sampler2D envMapCDFTex;

// 
uniform float envMapIntensity;
uniform float envMapRot;
uniform vec3 uniformLightCol;
uniform int maxDepth;
uniform vec2 tileOffset;
uniform int frameNum;
uniform float roughnessMollificationAmt;











/*
	declare globals variables
*/
// -------------------------------------------------------------------------------------------------------------------------------
// globals.glsl
// -------------------------------------------------------------------------------------------------------------------------------


#define PI         3.14159265358979323
#define INV_PI     0.31830988618379067
#define TWO_PI     6.28318530717958648
#define INV_TWO_PI 0.15915494309189533
#define INV_4_PI   0.07957747154594766
#define EPSILON 1e-6
#define INF 1e6

#define QUAD_LIGHT 0
#define SPHERE_LIGHT 1
#define DISTANT_LIGHT 2

#define ALPHA_MODE_OPAQUE 0
#define ALPHA_MODE_BLEND 1
#define ALPHA_MODE_MASK 2

#define MEDIUM_NONE 0
#define MEDIUM_ABSORB 1
#define MEDIUM_SCATTER 2
#define MEDIUM_EMISSIVE 3

struct Ray
{
	vec3 ori;
	vec3 dir;
};

struct Medium
{
	int type;
	vec3 color;
	float density;
	float anisotropy;
};

// material.h
struct Material
{
	vec3 baseColor;
    float anisotropic;

    vec3 emission;

    float metallic;
    float roughness;
    float subsurface;
    float specularTint;

    float sheen;
    float sheenTint;
    float clearcoat;
    float clearcoatRoughness;

    float specTrans;
    float ior;

	Medium medium;

    float ax;
    float ay;
    float opacity;
    int alphaMode;
    float alphaCutoff;
};

struct Camera
{
	vec3 up;
	vec3 right;
	vec3 forward;
	vec3 position;
	float fov;
	float focalDistance;
	float lensRadius;
};

struct Light
{
	vec3 position;
	vec3 emission;
	vec3 u, v;
	float radius;
	float area;
	float type;
};

struct State
{
	int depth;
	float eta;
	float hitT;

	vec3 fhp;       // first hit point along the ray
	vec3 normal;
	vec3 ffnormal;  // face forward normal
	vec3 tangent;
	vec3 bitangent;

	bool isEmitter;

	vec2 texCoord;
	int matID;
	Material mat;
	Medium medium;
};

struct ScatterSample
{
    vec3 L;
    vec3 f;
    float pdf;
};

struct LightSample
{
    vec3 normal;
    vec3 emission;
    vec3 direction;
    float dist;
    float pdf;
};

uniform Camera camera;

// utility function
vec3 FaceForward(vec3 a, vec3 b)
{
    return dot(a, b) < 0.0 ? -b : b;
}

// PBRT-V3 325\328 pages. RGBToXYZ()::xyz[1] Luminance measures brightness of a color. 
float Luminance(vec3 rgb)
{
    return 0.212671 * rgb.r + 0.715160 * rgb.g + 0.072169 * rgb.b;
}

//RNG from code by Moroz Mykhailo (https://www.shadertoy.com/view/wltcRS)
//internal RNG state 
uvec4 seed;
ivec2 pixel;

void InitRNG(vec2 p, int frame)
{
    pixel = ivec2(p);
	// white noise seed
    seed = uvec4(p, uint(frame), uint(p.x) + uint(p.y));
}

// https://www.pcg-random.org/
void pcg4d(inout uvec4 v)
{
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
    v = v ^ (v >> 16u);
    v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
}

float rand()
{
    pcg4d(seed); return float(seed.x) / float(0xffffffffu);
}

vec2 rand2()
{
    pcg4d(seed); return vec2(seed.xy)/float(0xffffffffu);
}

vec3 rand3()
{
    pcg4d(seed); return vec3(seed.xyz)/float(0xffffffffu);
}

vec4 rand4()
{
    pcg4d(seed); return vec4(seed)/float(0xffffffffu);
}







// -------------------------------------------------------------------------------------------------------------------------------
// intersection.glsl
// -------------------------------------------------------------------------------------------------------------------------------

float SphereIntersect(vec3 center, float radius, Ray r)
{
    /*
    // 一般推导
	vec3 localOrigin = r.ori - center;
    float a = dot(r.dir, r.dir);
    float b = dot(r.dir, localOrigin) * 2.0;
    float c = dot(localOrigin, localOrigin) - radius * radius;
    float discriminant = b * b - 4 * a * c;
    if(discriminant < 0.0){
        return INF;
    }
    float t1 = (-b - sqrt(discriminant))/(2.0 * a);
    if(t1 > EPSILON){
        return t1;
    }
    float t2 = (-b + sqrt(discriminant))/(2.0 * a);
    if(t2 > EPSILON){
        return t2;
    }
    */
    
    // 假设ray已经归一化, 那么A=1, 基于此化简公式
    vec3 localOrigin = r.ori - center;
    //float A = dot(r.dir, r.dir) = 1.0;
    float halfB = dot(r.dir, localOrigin); // = 0.5*b;
    float C = dot(localOrigin, localOrigin) - radius * radius;
    float discriminant = halfB*halfB - C;
    if(discriminant < 0.0){
        return INF;
    }

    float t1 = -halfB - sqrt(discriminant);
    if(t1 > EPSILON){
        return t1;
    }

    float t2 = -halfB + sqrt(discriminant);
    if(t2 > EPSILON){
        return t2;
    }

	return INF;
}

float RectangleIntersect(vec3 pos, vec3 u, vec3 v, vec4 plane, Ray r)
{
    vec3 n = vec3(plane);
    float t = (plane.w - dot(n, r.ori)) / dot(n, r.dir);

    if(t > EPSILON)
    {
        vec3 p = r.ori + r.dir * t;
        vec3 vi = p - pos;
        float a1 = dot(u, vi);
        if (a1 >= 0.0 && a1 <= 1.0)
        {
            float a2 = dot(v, vi);
            if (a2 >= 0.0 && a2 <= 1.0)
                return t;
        }
        
    }
    
	return INF;
}

float AABBIntersect(vec3 pMin, vec3 pMax, Ray r)
{
    vec3 invDir = 1.0 / r.dir;

    vec3 tNear = (pMin - r.ori) * invDir;
    vec3 tFar = (pMax - r.ori) * invDir;

    vec3 tMin = min(tNear, tFar);
    vec3 tMax = max(tNear, tFar);

    float tEnter = max(tMin.x, max(tMin.y, tMin.z));
    float tExit = min(tMax.x, min(tMax.y, tMax.z));
    // tExit >= tEnter && tEnter < 0.0
    // 那么直接返回tExit，不需要再判断tExit和0的大小关系了
    // 如果tExit > 0.0，有交点，此时tEnter < 0.0 < tExit，t值就是tExit
    // 如果tExit < 0.0，没交点，此时tEnter < tExit < 0.0，返回的t值无用
	return tExit >= tEnter ? (tEnter > 0.0 ? tEnter : tExit) : -1.0;
}









// -------------------------------------------------------------------------------------------------------------------------------
// sampling.glsl
// -------------------------------------------------------------------------------------------------------------------------------

// ----------------------------------------------------------------------
// Normal Distribution Function And Importance Sampling---[1] 24-26 page、https://zhuanlan.zhihu.com/p/69380665
// ----------------------------------------------------------------------

// isotropic distribution
float GTR1(float NDotH, float a)
{
    if (a >= 1.0)
        return INV_PI;
    float a2 = a * a;
    float t = 1.0 + (a2 - 1.0) * NDotH * NDotH;
    return (a2 - 1.0) / (PI * log(a2) * t);
}

// Importance sampling. microfacet normal, h, according to GTR1
vec3 SampleGTR1(float roughness, float r1, float r2)
{
    float a = max(0.001, roughness);
    float a2 = a * a;

    float phi = r1 * TWO_PI;

    float cosThetaH = sqrt((1.0 - pow(a2, 1.0 - r2)) / (1.0 - a2));
    float sinThetaH = clamp(sqrt(1.0 - (cosThetaH * cosThetaH)), 0.0, 1.0);
    float sinPhiH = sin(phi);
    float cosPhiH = cos(phi);

    return vec3(sinThetaH * cosPhiH, sinThetaH * sinPhiH, cosThetaH);
}

// isotropic distribution, GTR2 equals to GGX
float GTR2(float NDotH, float a)
{
    float a2 = a * a;
    float t = 1.0 + (a2 - 1.0) * NDotH * NDotH;
    return a2 / (PI * t * t);
}

// Importance sample microfacet normal, h, according to GTR2
vec3 SampleGTR2(float roughness, float r1, float r2)
{
    float a = max(0.001, roughness);

    float phi = r1 * TWO_PI;
    
    float cosThetaH = sqrt((1.0 - r2) / (1.0 + (a*a - 1.0) * r2));
    float sinThetaH = clamp(sqrt(1.0 - (cosThetaH * cosThetaH)), 0.0, 1.0);
    float sinPhiH = sin(phi);
    float cosPhiH = cos(phi);
    
    return vec3(sinThetaH * cosPhiH, sinThetaH * sinPhiH, cosThetaH);
}

// VNDF:可见法线分布函数
vec3 SampleGGXVNDF(vec3 V, float ax, float ay, float r1, float r2)
{
    vec3 Vh = normalize(vec3(ax * V.x, ay * V.y, V.z));

    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    vec3 T1 = lensq > 0 ? vec3(-Vh.y, Vh.x, 0) * inversesqrt(lensq) : vec3(1, 0, 0);
    vec3 T2 = cross(Vh, T1);

    float r = sqrt(r1);
    float phi = 2.0 * PI * r2;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;

    vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;

    return normalize(vec3(ax * Nh.x, ay * Nh.y, max(0.0, Nh.z)));
}

// an efficient alternate form
float GTR2Aniso(float NDotH, float HDotX, float HDotY, float ax, float ay)
{
    float a = HDotX / ax;
    float b = HDotY / ay;
    float c = a * a + b * b + NDotH * NDotH;
    return 1.0 / (PI * ax * ay * c * c);
}

// ignoring normalization factor r, and n is the local frame's z axis
vec3 SampleGTR2Aniso(float ax, float ay, float r1, float r2)
{
    float phi = r1 * TWO_PI;

    float sinPhiH = ay * sin(phi);
    float cosPhiH = ax * cos(phi);
    float tanThetaH = sqrt(r2 / (1 - r2));

    return vec3(tanThetaH * cosPhiH, tanThetaH * sinPhiH, 1.0);
    // vec3 projectedH = vec3(tanThetaH*cosPhiH, 0, 0) + 
    //                   vec3(0, tanThetaH*sinPhiH, 0) + 
    //                   vec3(0, 0, 1.0);
}


// ----------------------------------------------------------------------
// Geometry Function---https://zhuanlan.zhihu.com/p/81708753
// ----------------------------------------------------------------------

// Disney Smith GGX
float SmithG(float NDotV, float alphaG)
{
    float a = alphaG * alphaG;
    float b = NDotV * NDotV;
    return (2.0 * NDotV) / (NDotV + sqrt(a + b - a * b));
}

// Anisotropic Smith GGX
float SmithGAniso(float NDotV, float VDotX, float VDotY, float ax, float ay)
{
    float a = VDotX * ax;
    float b = VDotY * ay;
    return (2.0 * NDotV) / (NDotV + sqrt(a * a + b * b + NDotV * NDotV));
}


// ----------------------------------------------------------------------
// Fresnel Equation---https://zhuanlan.zhihu.com/p/31534769
// ----------------------------------------------------------------------

// 计算Schlick-Fresnel近似的指数项
float SchlickWeight(float u)
{
    float m = clamp(1.0 - u, 0.0, 1.0);
    float m2 = m * m;
    return m2 * m2 * m;  // pow(m,5)
}

// 电解质的菲涅尔方程, 无近似
float DielectricFresnel(float cosThetaI, float eta)
{
    float sinThetaTSquare = eta * eta * (1.0 - cosThetaI * cosThetaI);

    // 全内反射
    if(sinThetaTSquare > 1.0)
        return 1.0;
    
    float cosThetaT = sqrt(max(1.0 - sinThetaTSquare, 0.0));

    // 入射光的s偏振(senkrecht polarized)和p偏振(parallel polarized)所造成的反射比
    float rs = (eta * cosThetaT - cosThetaI) / (eta * cosThetaT + cosThetaI);
    float rp = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);

    // 图形学中通常考虑光是无偏振的（unpolarized），也就是两种偏振是等量的，所以可以取其平均值
    return 0.5 * (rs*rs + rp*rp);
}

// ----------------------------------------------------------------------
// Sphere Sampling And Cosine Importance Sampling---https://zhuanlan.zhihu.com/p/360420413
// ----------------------------------------------------------------------

// 半球cosine重要性采样
vec3 CosineSampleHemisphere(float r1, float r2)
{
    vec3 dir;
    float r = sqrt(r1);
    float phi = TWO_PI * r2;
    dir.x = r * cos(phi);
    dir.y = r * sin(phi);
    dir.z = sqrt(max(0.0, 1.0 - dir.x * dir.x - dir.y * dir.y));
    return dir;
}

// 半球均匀采样
vec3 UniformSampleHemisphere(float r1, float r2)
{
    float r = sqrt(max(0.0, 1.0 - r1 * r1));
    float phi = TWO_PI * r2;
    return vec3(r * cos(phi), r * sin(phi), r1);
}

// 球面均匀采样
vec3 UniformSampleSphere(float r1, float r2)
{
    float z = 1.0 - 2.0 * r1;
    float r = sqrt(max(0.0, 1.0 - z * z));
    float phi = TWO_PI * r2;
    return vec3(r * cos(phi), r * sin(phi), z);
}

// 幂启发式，多重重要性采样
float PowerHeuristic(float a, float b)
{
    float t = a * a;
    return t / (b * b + t);
}

// 以法线N为中心构建局部坐标系
void ONB(vec3 N, inout vec3 T, inout vec3 B)
{
    /*
    if(fabs(N.x) > fabs(N.z))
        B = vec3(-N.y, N.x, 0.0);
    else
        B = vec3(0.0, -N.z, N.y);
    B = normalize(B);
    T  = cross(B, N);
    */
    vec3 up = abs(N.z) < 0.9999999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    T = normalize(cross(up, N));
    B = cross(N, T);
}


// ----------------------------------------------------------------------
// Light Sampling
// ----------------------------------------------------------------------

// 采样球形光源
void SampleSphereLight(Light light, vec3 scatterPos, inout LightSample lightSample)
{
    float r1 = rand();
    float r2 = rand();

    // TODO: Fix this. Currently assumes the light will be hit only from the outside
    // 计算光源球心与scatterPos(着色点)的方向、距离
    vec3 sphereCentertoSurface = scatterPos - light.position;
    float distToSphereCenter = length(sphereCentertoSurface);
    sphereCentertoSurface /= distToSphereCenter; // normalization
    
    // 以sphereCentertoSurface为中心建立局部坐标系
    vec3 T, B;
    ONB(sphereCentertoSurface, T, B);
    
    // 均匀采样半球，再转换到局部坐标系
    vec3 sampledDir = UniformSampleHemisphere(r1, r2);
    sampledDir = T * sampledDir.x + B * sampledDir.y + sphereCentertoSurface * sampledDir.z;

    // pos + UnitVector * module length
    vec3 lightSurfacePos = light.position + sampledDir * light.radius;

    // 初始化lightSample
    lightSample.direction = lightSurfacePos - scatterPos;
    lightSample.dist = length(lightSample.direction);
    lightSample.direction /= lightSample.dist;  //normalization
    
    // normalize(lightSurfacePos - light.position) == sampledDir?
    lightSample.normal = normalize(lightSurfacePos - light.position);
    lightSample.emission = light.emission * float(lightsNum); // ???
    
    // 球面只有一半能照射到scatterPos，所以面积A要乘0.5
    // 积分项为dw: pdf = r^2 / (A * 0.5 * cosTheta')
    lightSample.pdf = (lightSample.dist * lightSample.dist) / (light.area * 0.5 * abs(dot(lightSample.normal, lightSample.direction)));
    // 积分项为dA: pdf = 1.0 / (A * 0.5)
}

// 采样矩形光源
void SampleRectLight(Light light, vec3 scatterPos, inout LightSample lightSample)
{
    float r1 = rand();
    float r2 = rand();

    // 随机选择矩形光源上的一点
    vec3 lightSurfacePos = light.position + light.u * r1 + light.v * r2;
    
    // 初始化lightSample
    lightSample.direction = lightSurfacePos - scatterPos;
    lightSample.dist = length(lightSample.direction);
    lightSample.direction /= lightSample.dist;

    lightSample.normal = normalize(cross(light.u, light.v));
    lightSample.emission = light.emission * float(lightsNum); // ???
    
    // 积分项为dw: pdf = r^2 / (A * cosTheta')
    lightSample.pdf = (lightSample.dist * lightSample.dist) / (light.area * abs(dot(lightSample.normal, lightSample.direction)));
    // 积分项为dA: pdf = 1.0 / A
}

// 采样无限远光源
void SampleDistantLight(Light light, vec3 scatterPos, inout LightSample lightSample)
{
    lightSample.direction = normalize(light.position - vec3(0.0));
    lightSample.dist = INF;
    lightSample.normal = normalize(scatterPos - light.position);
    lightSample.emission = light.emission * float(lightsNum);  // ???
    lightSample.pdf = 1.0;
}

// 根据light.type选择光源采样函数
void SampleOneLight(Light light, vec3 scatterPos, inout LightSample lightSample)
{
    int type = int(light.type);

    if (type == QUAD_LIGHT)
        SampleRectLight(light, scatterPos, lightSample);
    else if (type == SPHERE_LIGHT)
        SampleSphereLight(light, scatterPos, lightSample);
    else
        SampleDistantLight(light, scatterPos, lightSample);
}



// ----------------------------------------------------------------------
// ?????
// ----------------------------------------------------------------------


vec3 SampleHG(vec3 V, float g, float r1, float r2)
{
    float cosTheta;

    if (abs(g) < 0.001)
        cosTheta = 1 - 2 * r2;
    else 
    {
        float sqrTerm = (1 - g * g) / (1 + g - 2 * g * r2);
        cosTheta = -(1 + g * g - sqrTerm * sqrTerm) / (2 * g);
    }

    float phi = r1 * TWO_PI;
    float sinTheta = clamp(sqrt(1.0 - (cosTheta * cosTheta)), 0.0, 1.0);
    float sinPhi = sin(phi);
    float cosPhi = cos(phi);

    vec3 v1, v2;
    ONB(V, v1, v2);

    return sinTheta * cosPhi * v1 + sinTheta * sinPhi * v2 + cosTheta * V;
}

float PhaseHG(float cosTheta, float g)
{
    float denom = 1 + g * g + 2 * g * cosTheta;
    return INV_4_PI * (1 - g * g) / (denom * sqrt(denom));
}












// -------------------------------------------------------------------------------------------------------------------------------
// envmap.glsl
// -------------------------------------------------------------------------------------------------------------------------------

// 坐标系第一象限的二维二分查找，查找value的(x, y)坐标
// value is luminance
vec2 BinarySearch(float value)
{
    ivec2 envMapResInt = ivec2(envMapRes);

    // 先确定在哪一行
    int lower = 0;
    int upper = envMapResInt.y - 1;
    while(lower < upper)
    {
        int mid = (lower + upper) >> 1;
        if (value < texelFetch(envMapCDFTex, ivec2(envMapResInt.x - 1, mid), 0).r)
            upper = mid;
        else
            lower = mid + 1;
    }
    int y = clamp(lower, 0, envMapResInt.y - 1);

    // 再确定在哪一列
    lower = 0;
    upper = envMapResInt.x - 1;
    while(lower < upper)
    {
        int mid = (lower + upper) >> 1;
        if (value < texelFetch(envMapCDFTex, ivec2(mid, y), 0).r)
            upper = mid;
        else
            lower = mid + 1;
    }
    int x = clamp(lower, 0, envMapResInt.x - 1);

    // 实际坐标转换为百分比坐标
    return vec2(x, y) / envMapRes;
}

// 根据已知的ray计算击中envMap的颜色值
vec4 EvalEnvMap(Ray r)
{
    /*
    asin: The range of values returned is [-PI/2, PI/2]
    acos: The range of values returned is [0, PI]
    atan(numerator, denominator): The range of values returned is [-PI, PI]
    */
    // 将r.dir球面坐标 映射到 二维envMap的uv坐标，映射只是一种关系，不一定必须像下面这样计算theta、phi
    float theta = acos(clamp(r.dir.y, -1.0, 1.0));
    vec2 uv = vec2((PI + atan(r.dir.z, r.dir.x)) * INV_TWO_PI, theta * INV_PI) + vec2(envMapRot, 0.0);
    
    vec3 color = texture(envMapTex, uv).rgb;
    float pdf = Luminance(color) / envMapTotalSum;

    // TODO: w项????
    return vec4(color, (pdf * envMapRes.x * envMapRes.y) / (TWO_PI * PI * sin(theta)));
}

// 从envMap中均匀采样一条光线
vec4 SampleEnvMap(inout vec3 color)
{
    // envMapTotalSum是所有像素的LuminanceTotalSum
    vec2 uv = BinarySearch(rand() * envMapTotalSum);

    color = texture(envMapTex, uv).rgb;
    // pdf = singlePixelLuminance / LuminanceTotalSum
    float pdf = Luminance(color) / envMapTotalSum;

    // (u, v)->(phi, theta)
    uv.x -= envMapRot;
    float phi = uv.x * TWO_PI;
    float theta = uv.y * PI;

    if (sin(theta) == 0.0)
        pdf = 0.0;
    
    // TODO: w项????
    return vec4(-sin(theta) * cos(phi), cos(theta), -sin(theta) * sin(phi), (pdf * envMapRes.x * envMapRes.y) / (TWO_PI * PI * sin(theta)));
}



// -------------------------------------------------------------------------------------------------------------------------------
// anyhit.glsl
// -------------------------------------------------------------------------------------------------------------------------------

bool AnyHit(Ray r, float maxDist)
{
#ifdef NAGI_LIGHTS
    // Intersect Emitters
    for(int i = 0; i < lightsNum; i++)
    {
        // 根据light.h中Light的成员变量顺序获取数据，以vec3f为刻度，Light等同5个vec3f
        vec3 position   = texelFetch(lightsTex, ivec2(i * 5 + 0, 0), 0).xyz;
        vec3 emission   = texelFetch(lightsTex, ivec2(i * 5 + 1, 0), 0).xyz;
        vec3 u          = texelFetch(lightsTex, ivec2(i * 5 + 2, 0), 0).xyz;
        vec3 v          = texelFetch(lightsTex, ivec2(i * 5 + 3, 0), 0).xyz;
        vec3 params     = texelFetch(lightsTex, ivec2(i * 5 + 4, 0), 0).xyz;
        float radius    = params.x;
        float area      = params.y;
        float type      = params.z;

        // Intersect area light according to light.type
        if (type == QUAD_LIGHT)
        {
            vec3 normal = normalize(cross(u, v));
            vec4 plane = vec4(normal, dot(normal, position));
            u *= 1.0f / dot(u, u);  // normalization
            v *= 1.0f / dot(v, v);  // normalization

            float d = RectangleIntersect(position, u, v, plane, r);
            if (d > 0.0 && d < maxDist)
                return true;
        }
        else if (type == SPHERE_LIGHT)
        {
            float d = SphereIntersect(position, radius, r);
            if (d > 0.0 && d < maxDist)
                return true;
        }
    }
#endif

    // 遍历BVH
    int nodesToVisit[64];
    int toVisitOffset = 0;
    nodesToVisit[toVisitOffset++] = -1;

    // nodeIdx，当前处理节点的索引
    int curNodeIdx = tlasBVHStartOffset;
#if defined(NAGI_ALPHA_TEST) && !defined(NAGI_MEDIUM)
    int curMatID = 0;
#endif
    bool BLAS = false;

    Ray rTrans;
    rTrans.ori = r.ori;
    rTrans.dir = r.dir;

    // curNodeIdx == -1，退出遍历
    while(curNodeIdx != -1)
    {
        // 根据bvh.h中LinearBVHNode的成员变量顺序获取数据，以vec3f为刻度，LinearBVHNode等同3个vec3f
        ivec3 params    = ivec3(texelFetch(BVHTex, curNodeIdx * 3 + 2).xyz);
        int nPrimitives = params.y;

        // blasBVH叶子节点
        if (nPrimitives > 0 && curNodeIdx < tlasBVHStartOffset)
        {
            // 解析params
            int primitivesOffset = params.x;
            for(int i = 0; i < nPrimitives; i++)
            {
                // 获取vertexIndices数组的数据，以vec3i为刻度
                ivec3 primVertexIdx = ivec3(texelFetch(vertexIndicesTex, primitivesOffset + i).xyz);

                // 获取verticesUVX数组的数据，包括顶点和u，以vec4f为刻度
                vec4 v0_u = texelFetch(verticesTex, primVertexIdx.x);
                vec4 v1_u = texelFetch(verticesTex, primVertexIdx.y);
                vec4 v2_u = texelFetch(verticesTex, primVertexIdx.z);

                // TODO
                vec3 e0 = v1_u.xyz - v0_u.xyz;
                vec3 e1 = v2_u.xyz - v0_u.xyz;
                vec3 pv = cross(rTrans.dir, e1);
                float det = dot(e0, pv);

                vec3 tv = rTrans.ori - v0_u.xyz;
                vec3 qv = cross(tv, e0);

                vec4 uvt;
                uvt.x = dot(tv, pv);
                uvt.y = dot(rTrans.dir, qv);
                uvt.z = dot(e1, qv);
                uvt.xyz = uvt.xyz / det;
                uvt.w = 1.0 - uvt.x - uvt.y;

                if(all(greaterThanEqual(uvt, vec4(0.0))) && uvt.z < maxDist)
                {
#if defined(NAGI_ALPHA_TEST) && !defined(NAGI_MEDIUM)
                    // 从normalsUVY数组中获取uv中的v值，根据scene中的规则，v是第四项w。normalsUVY数组以vec4f为刻度
                    vec2 uv0 = vec2(v0_u.w, texelFetch(normalsTex, primVertexIdx.x).w);
                    vec2 uv1 = vec2(v1_u.w, texelFetch(normalsTex, primVertexIdx.y).w);
                    vec2 uv2 = vec2(v2_u.w, texelFetch(normalsTex, primVertexIdx.z).w);
                    vec2 texCoord = uv0 * uvt.w + uv1 * uvt.x + uv2 * uvt.y;
                    
                    // 根据material.h中Material的成员变量顺序获取数据，以vec4f为刻度，Material等同8个vec4f
                    int baseColorTexID  = texelFetch(materialsTex, ivec2(curMatID * 8 + 6, 0), 0).x;
                    float opacity       = texelFetch(materialsTex, ivec2(curMatID * 8 + 7, 0), 0).y;
	                float alphaMode     = texelFetch(materialsTex, ivec2(curMatID * 8 + 7, 0), 0).z;
	                float alphaCutoff   = texelFetch(materialsTex, ivec2(curMatID * 8 + 7, 0), 0).w;

                    // opacity *= alpha
                    // textureMapsArrayTex是一个三维数组，xy代表一张纹理的坐标，z代表第几张纹理
                    opacity *= texture(textureMapsArrayTex, vec3(texCoord, baseColorTexID)).a;

                    // Ignore intersection and continue ray based on alpha test
                    if (!((alphaMode == ALPHA_MODE_MASK && opacity < alphaCutoff) || 
                          (alphaMode == ALPHA_MODE_BLEND && rand() > opacity)))
                        return true;
#else
                    return true;
#endif
                }
            }
        }
        // tlasBVH叶子节点
        else if (nPrimitives > 0 && curNodeIdx >= tlasBVHStartOffset)
        {
            // 解析params
            int blasBVHStartOffset  = params.x;
            int meshInstanceIdx     = params.y - 1; // 对应scene.cpp中ProcessTLAS()
#if defined(NAGI_ALPHA_TEST) && !defined(NAGI_MEDIUM)
            curMatID = params.z;
#endif
            // tlas的叶子存储blas，blas起始位置是blasBVHStartOffset
            curNodeIdx = blasBVHStartOffset;
            BLAS = true;

            // TODO:需要查阅GLSL中mat4的构建规则与parser.cpp中transform的构建方式
            // 从transforms数组中获取instance的变换矩阵，transforms数组以vec4f为刻度，matrix等同4个vec4f
            vec4 r1 = texelFetch(transformsTex, ivec2(meshInstanceIdx * 4 + 0, 0), 0);
            vec4 r2 = texelFetch(transformsTex, ivec2(meshInstanceIdx * 4 + 1, 0), 0);
            vec4 r3 = texelFetch(transformsTex, ivec2(meshInstanceIdx * 4 + 2, 0), 0);
            vec4 r4 = texelFetch(transformsTex, ivec2(meshInstanceIdx * 4 + 3, 0), 0);
            mat4 transform = mat4(r1, r2, r3, r4);
            // TODO:查阅为什么这里用inverse(transform)
            rTrans.ori = vec3(inverse(transform) * vec4(r.ori, 1.0));
            rTrans.dir = vec3(inverse(transform) * vec4(r.dir, 0.0));

            // Add a marker. We'll return to this spot after we've traversed the entire BLAS
            nodesToVisit[toVisitOffset++] = -1;
            continue;
        }
        // blasBVH、tlasBVH中间节点
        else
        {
            // 解析params
            int firstChildOffset = curNodeIdx + 1; // 前向序列中，左子树为当前子树idx+1
            int secondChildOffset = params.x;

            // TODO: 利用splitAxis优化判断光线优先击中哪一box
            // 根据bvh.h中LinearBVHNode的成员变量顺序获取数据，以vec3f为刻度，LinearBVHNode等同3个vec3f
            float leftHitT  = AABBIntersect(texelFetch(BVHTex, firstChildOffset * 3 + 0).xyz, texelFetch(BVHTex, firstChildOffset * 3 + 1).xyz, rTrans);
            float rightHitT = AABBIntersect(texelFetch(BVHTex, secondChildOffset * 3 + 0).xyz, texelFetch(BVHTex, secondChildOffset * 3 + 1).xyz, rTrans);
            if (leftHitT > 0.0 && rightHitT > 0.0)
            {
                int deferred = -1;
                if (leftHitT > rightHitT) {
                    curNodeIdx = secondChildOffset;
                    deferred = firstChildOffset;
                } else {
                    curNodeIdx = firstChildOffset;
                    deferred = secondChildOffset;
                }
                // 两个子树都有交点，先遍历近的，远的入栈
                nodesToVisit[toVisitOffset++] = deferred;
                continue;
            }
            else if (leftHitT > 0.0)
            {
                curNodeIdx = firstChildOffset;
                continue;
            }
            else if (rightHitT > 0.0)
            {
                curNodeIdx = secondChildOffset;
                continue;
            }
        }

        curNodeIdx = nodesToVisit[--toVisitOffset];

        // If we've traversed the entire BLAS then switch to back to TLAS and resume where we left off
        // 遍历完BLAS后，返回TLAS中，ray要重置回应用transform前
        if (BLAS && curNodeIdx == -1)
        {
            BLAS = false;
            curNodeIdx = nodesToVisit[--toVisitOffset];
            rTrans.ori = r.ori;
            rTrans.dir = r.dir;
        }
    }

    return false;
}









// -------------------------------------------------------------------------------------------------------------------------------
// closest_hit.glsl
// -------------------------------------------------------------------------------------------------------------------------------


bool ClosestHit(Ray r, inout State state, inout LightSample lightSample)
{
    float t = INF;
    float d;

    // Intersect Emitters
#ifdef NAGI_LIGHTS
#ifdef NAGI_HIDE_EMITTERS
// depth为0时，ray从相机出发，此时忽视光源
if(state.depth > 0)
#endif
    for(int i = 0; i < lightsNum; i++)
    {
        // 根据light.h中Light的成员变量顺序获取数据，以vec3f为刻度，Light等同5个vec3f
        vec3 position   = texelFetch(lightsTex, ivec2(i * 5 + 0, 0), 0).xyz;
        vec3 emission   = texelFetch(lightsTex, ivec2(i * 5 + 1, 0), 0).xyz;
        vec3 u          = texelFetch(lightsTex, ivec2(i * 5 + 2, 0), 0).xyz;
        vec3 v          = texelFetch(lightsTex, ivec2(i * 5 + 3, 0), 0).xyz;
        vec3 params     = texelFetch(lightsTex, ivec2(i * 5 + 4, 0), 0).xyz;
        float radius    = params.x;
        float area      = params.y;
        float type      = params.z;

        // Intersect area light according to light.type
        if (type == QUAD_LIGHT)
        {
            vec3 normal = normalize(cross(u, v));
            if (dot(normal, r.dir) > 0.0)
                continue;
            vec4 plane = vec4(normal, dot(normal, position));
            u *= 1.0f / dot(u, u);  // normalization
            v *= 1.0f / dot(v, v);  // normalization

            d = RectangleIntersect(position, u, v, plane, r);
            if (d < 0.0)
                d = INF;
            
            if (d < t)
            {
                t = d;
                float cosTheta  = dot(-r.dir, normal);
                lightSample.pdf      = (t * t) / (area * cosTheta);
                lightSample.emission = emission;
                state.isEmitter = true;
            }
        }
        else if (type == SPHERE_LIGHT)
        {
            float d = SphereIntersect(position, radius, r);
            if (d < 0.0)
                d = INF;
            
            if (d < t)
            {
                t = d;
                vec3 hitPoint = r.ori + t * r.dir;
                float cosTheta  = dot(-r.dir, normalize(hitPoint - position));
                // TODO: Fix this. Currently assumes the light will be hit only from the outside
                lightSample.pdf      = (t * t) / (area * 0.5 * cosTheta);
                lightSample.emission = emission;
                state.isEmitter = true;
            }
        }
    }
#endif

    /* BVH Traversal */

    int nodesToVisit[64];
    int toVisitOffset = 0;
    nodesToVisit[toVisitOffset++] = -1;

    int curNodeIdx = tlasBVHStartOffset;
    int curMatID = 0;
    bool BLAS = false;

    ivec3 triangleIdx = ivec3(-1);
    mat4 triangleTransMat, instanceTransMat;
    vec3 barycentric;
    vec4 vert0, vert1, vert2;

    Ray rTrans;
    rTrans.ori = r.ori;
    rTrans.dir = r.dir;

    // curNodeIdx == -1，退出遍历
    while(curNodeIdx != -1)
    {
        // 根据bvh.h中LinearBVHNode的成员变量顺序获取数据，以vec3f为刻度，LinearBVHNode等同3个vec3f
        ivec3 params    = ivec3(texelFetch(BVHTex, curNodeIdx * 3 + 2).xyz);
        int nPrimitives = params.y;

        // blasBVH叶子节点
        if (nPrimitives > 0 && curNodeIdx < tlasBVHStartOffset)
        {
            // 解析params
            int primitivesOffset = params.x;
            for(int i = 0; i < nPrimitives; i++)
            {
                // 获取vertexIndices数组的数据，以vec3i为刻度
                ivec3 primVertexIdx = ivec3(texelFetch(vertexIndicesTex, primitivesOffset + i).xyz);

                // 获取verticesUVX数组的数据，包括顶点和u，以vec4f为刻度
                vec4 v0_u = texelFetch(verticesTex, primVertexIdx.x);
                vec4 v1_u = texelFetch(verticesTex, primVertexIdx.y);
                vec4 v2_u = texelFetch(verticesTex, primVertexIdx.z);

                // TODO:
                vec3 e0 = v1_u.xyz - v0_u.xyz;
                vec3 e1 = v2_u.xyz - v0_u.xyz;
                vec3 pv = cross(rTrans.dir, e1);
                float det = dot(e0, pv);

                vec3 tv = rTrans.ori - v0_u.xyz;
                vec3 qv = cross(tv, e0);

                vec4 uvt;
                uvt.x = dot(tv, pv);
                uvt.y = dot(rTrans.dir, qv);
                uvt.z = dot(e1, qv);
                uvt.xyz = uvt.xyz / det;
                uvt.w = 1.0 - uvt.x - uvt.y;

                if(all(greaterThanEqual(uvt, vec4(0.0))) && uvt.z < t)
                {
                    // 记录最近交点的相关信息
                    t = uvt.z;
                    triangleIdx = primVertexIdx;
                    state.matID = curMatID;
                    barycentric = uvt.wxy;
                    vert0 = v0_u, vert1 = v1_u, vert2 = v2_u;
                    triangleTransMat = instanceTransMat;
                }
            }
        }
        // tlasBVH叶子节点
        else if (nPrimitives > 0 && curNodeIdx >= tlasBVHStartOffset)
        {
            // 解析params
            int blasBVHStartOffset  = params.x;
            int meshInstanceIdx     = params.y - 1; // 对应scene.cpp中ProcessTLAS()
            curMatID                = params.z;

            // tlas的叶子存储blas，blas起始位置是blasBVHStartOffset
            curNodeIdx = blasBVHStartOffset;
            BLAS = true;

            // TODO:需要查阅GLSL中mat4的构建规则与parser.cpp中transform的构建方式
            // 从transforms数组中获取instance的变换矩阵，transforms数组以vec4f为刻度，matrix等同4个vec4f
            vec4 r1 = texelFetch(transformsTex, ivec2(meshInstanceIdx * 4 + 0, 0), 0);
            vec4 r2 = texelFetch(transformsTex, ivec2(meshInstanceIdx * 4 + 1, 0), 0);
            vec4 r3 = texelFetch(transformsTex, ivec2(meshInstanceIdx * 4 + 2, 0), 0);
            vec4 r4 = texelFetch(transformsTex, ivec2(meshInstanceIdx * 4 + 3, 0), 0);
            instanceTransMat = mat4(r1, r2, r3, r4);
            // TODO:查阅为什么这里用inverse(instanceTransMat)
            rTrans.ori = vec3(inverse(instanceTransMat) * vec4(r.ori, 1.0));
            rTrans.dir = vec3(inverse(instanceTransMat) * vec4(r.dir, 0.0));

            // Add a marker. We'll return to this spot after we've traversed the entire BLAS
            nodesToVisit[toVisitOffset++] = -1;
            continue;
        }
        // blasBVH、tlasBVH中间节点
        else
        {
            // 解析params
            int firstChildOffset = curNodeIdx + 1; // 前向序列中，左子树为当前子树idx+1
            int secondChildOffset = params.x;

            // TODO: 利用splitAxis优化判断光线优先击中哪一box
            // 根据bvh.h中LinearBVHNode的成员变量顺序获取数据，以vec3f为刻度，LinearBVHNode等同3个vec3f
            float leftHitT  = AABBIntersect(texelFetch(BVHTex, firstChildOffset * 3 + 0).xyz, texelFetch(BVHTex, firstChildOffset * 3 + 1).xyz, rTrans);
            float rightHitT = AABBIntersect(texelFetch(BVHTex, secondChildOffset * 3 + 0).xyz, texelFetch(BVHTex, secondChildOffset * 3 + 1).xyz, rTrans);
            if (leftHitT > 0.0 && rightHitT > 0.0)
            {
                int deferred = -1;
                if (leftHitT > rightHitT) {
                    curNodeIdx = secondChildOffset;
                    deferred = firstChildOffset;
                } else {
                    curNodeIdx = firstChildOffset;
                    deferred = secondChildOffset;
                }
                // 两个子树都有交点，先遍历近的，远的入栈
                nodesToVisit[toVisitOffset++] = deferred;
                continue;
            }
            else if (leftHitT > 0.0)
            {
                curNodeIdx = firstChildOffset;
                continue;
            }
            else if (rightHitT > 0.0)
            {
                curNodeIdx = secondChildOffset;
                continue;
            }
        }

        // 只在遍历完叶子节点后、中间节点的左右子树都无hit时，才出栈回退
        curNodeIdx = nodesToVisit[--toVisitOffset];

        // If we've traversed the entire BLAS then switch to back to TLAS and resume where we left off
        // 遍历完BLAS后，返回TLAS中，ray要重置回应用transform前
        if (BLAS && curNodeIdx == -1)
        {
            BLAS = false;
            curNodeIdx = nodesToVisit[--toVisitOffset];
            rTrans.ori = r.ori;
            rTrans.dir = r.dir;
        }
    }

    /* Processing State after BVH Traversal */

    // 无交点
    if (t == INF)
        return false;
    
    state.hitT = t;
    state.fhp = r.ori + t * r.dir;  // first hit point along the ray
    
    // ray击中triangle，而不是光源
    if (triangleIdx.x != -1)
    {
        state.isEmitter = false;

        // 从normalsUVY数组中获取法线，normalsUVY数组以vec4f为刻度
        vec4 n0 = texelFetch(normalsTex, triangleIdx.x);
        vec4 n1 = texelFetch(normalsTex, triangleIdx.y);
        vec4 n2 = texelFetch(normalsTex, triangleIdx.z);

        // 从verticesUVX和normalsUVY的w项获取uv
        vec2 uv0 = vec2(vert0.w, n0.w);
        vec2 uv1 = vec2(vert1.w, n1.w);
        vec2 uv2 = vec2(vert2.w, n2.w);

        state.texCoord = uv0 * barycentric.x + uv1 * barycentric.y + uv2 * barycentric.z;
        vec3 normal = normalize(n0.xyz * barycentric.x + n1.xyz * barycentric.y + n2.xyz * barycentric.z);

        // 将局部法线变换回世界法线
        state.normal = normalize(transpose(inverse(mat3(triangleTransMat))) * normal);
        state.ffnormal = dot(state.normal, r.dir) <= 0.0 ? state.normal : -state.normal;    // face forward normal

        // 计算切线和副法线
        vec3 deltaPos1 = vert1.xyz - vert0.xyz;
        vec3 deltaPos2 = vert2.xyz - vert0.xyz;

        vec2 deltaUV1 = uv1 - uv0;
        vec2 deltaUV2 = uv2 - uv0;
        
        float invdet = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);

        state.tangent   = (deltaUV2.y * deltaPos1 - deltaUV1.y * deltaPos2) * invdet;
        state.bitangent = (deltaUV1.x * deltaPos2 - deltaUV2.x * deltaPos1) * invdet;

        state.tangent = normalize(mat3(triangleTransMat) * state.tangent);
        state.bitangent = normalize(mat3(triangleTransMat) * state.bitangent);
    }

    return true;
}

/*
struct LinearBVHNode
{
	bbox3f bounds;
	union {
		uint32_t primitivesOffset;		// orderedPrimsIndices的索引，用于blasBVH的叶子节点
		uint32_t secondChildOffset;		// nodes的索引，用于blasBVH和tlasBVH的中间节点，只需要存储右子树的idx，左子树idx为curIdx+1
		uint32_t blasBVHStartOffset;	// sceneNodes的索引，用于tlasBVH的叶子节点，表示blasBVH的存储起点
	};
	union {
		// (curNodeIdx < tlasBVHStartOffset) ? blasBVh : tlasBVH
		// blasBVH，nPrimitives == 0 是中间节点，nPrimitives != 0 是叶子存储的prim数
		// tlasBVH，nPrimitives == 0 是中间节点，meshInstanceIdx != 0 是叶子引用的blasBVH对应的instanceIdx
		uint32_t nPrimitives;
		uint32_t meshInstanceIdx;		// 记录instanceIndex，为了在GLSL中遍历时访问transformTex
	};
	union {
		uint32_t splitAxis;				// used for interior, indicate which axis is used to split when bvh bulid
		uint32_t materialID;			// used for tlasBVH, indicate which material is used for blasBVH stored in leaf
	};
};
*/












// -------------------------------------------------------------------------------------------------------------------------------
// disney.glsl
// -------------------------------------------------------------------------------------------------------------------------------


 vec3 DisneyEval(State state, vec3 V, vec3 N, vec3 L, out float pdf);

// V定义在TBN定义的局部空间的标准坐标系XYZ上，TBN本身在世界空间下
vec3 ToWorld(vec3 T, vec3 B, vec3 N, vec3 V)
{
    return V.x * T + V.y * B + V.z * N;
}

// T B N是变换矩阵的左上角三维子阵的行向量
vec3 ToLocal(vec3 T, vec3 B, vec3 N, vec3 V)
{
    return vec3(dot(T, V), dot(B, V), dot(N, V));
}

// [1]-cloth sheen
void TintColors(Material mat, float eta, out float F0, out vec3 Csheen, out vec3 Cspec0)
{
    float Cdlum = Luminance(mat.baseColor);
    vec3 Ctint = Cdlum > 0.0 ? mat.baseColor / Cdlum : vec3(1.0);  // normalize lum. to isolate hue+sat
    
    // F0: thetaD == 0°
    F0 = (1.0 - eta) / (1.0 + eta);
    F0 *= F0;

    // mix(x, y, a) = x * (1−a) + y * a
    Cspec0 = F0 * mix(vec3(1.0), Ctint, mat.specularTint);
    Csheen = mix(vec3(1.0), Ctint, mat.sheenTint);
}


// [1]-5.3 漫反射公式
vec3 EvalDisneyDiffuse(Material mat, vec3 Csheen, vec3 V, vec3 L, vec3 H, out float pdf)
{
    pdf = 0.0;
    if (L.z <= 0.0)
        return vec3(0.0);
    
    float LDotH = dot(L, H);    // thetaD
    
    // Diffuse, [1]-5.3 Fd
    float FL = SchlickWeight(L.z);  // TBN局部空间下N就是Z轴, dot(L,N)=L.z
    float FV = SchlickWeight(V.z);  // TBN局部空间下N就是Z轴, dot(V,N)=V.z
    float Fd90 = 0.5 + 2.0 * mat.roughness * LDotH * LDotH;   //Fd90: thetaD(angle between L and H) == 90°
    float Fd = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);

    // 化简上述漫反射公式得到如下：
    // float Rr = 2.0 * mat.roughness * LDotH * LDotH;
    // float Fretro = Rr * (FL + FV + FL * FV * (Rr - 1.0));
    // float Fd = (1.0 - 0.5 * FL) * (1.0 - 0.5 * FV);

    // Fake subsurface, [1]-5.3
    // Based on Hanrahan-Krueger brdf approximation of isotropic bssrdf
    // 1.25 scale is used to (roughly) preserve albedo
    // Fss90 used to "flatten" retroreflection based on roughness
    float Fss90 = (Fd90 - 0.5) * 0.5;   // mat.roughness * LDotH * LDotH
    float Fss = mix(1.0, Fss90, FL) * mix(1.0, Fss90, FV);
    float ss = 1.25 * (Fss * (1.0 / (L.z + V.z) - 0.5) + 0.5);

    // Sheen
    float FH = SchlickWeight(LDotH);
    vec3 Fsheen = FH * mat.sheen * Csheen;

    pdf = L.z * INV_PI;
    return INV_PI * mat.baseColor * mix(Fd, ss, mat.subsurface) + Fsheen;
}


// 
vec3 EvalMicrofacetReflection(Material mat, vec3 V, vec3 L, vec3 H, vec3 F, out float pdf)
{
    pdf = 0.0;
    if (L.z <= 0.0) // Reflection
        return vec3(0.0);
    
    float D = GTR2Aniso(H.z, H.x, H.y, mat.ax, mat.ay);
    float G1 = SmithGAniso(abs(V.z), V.x, V.y, mat.ax, mat.ay);
    float G2 = G1 * SmithGAniso(abs(L.z), L.x, L.y, mat.ax, mat.ay);

    pdf = G1 * D / (4.0 * V.z);
    return F * D * G2 / (4.0 * L.z * V.z);
}


//
vec3 EvalMicrofacetRefraction(Material mat, float eta, vec3 V, vec3 L, vec3 H, vec3 F, out float pdf)
{
    pdf = 0.0;
    if (L.z >= 0.0) // Refraction
        return vec3(0.0);

    float LDotH = dot(L, H);
    float VDotH = dot(V, H);

    float D = GTR2Aniso(H.z, H.x, H.y, mat.ax, mat.ay);
    float G1 = SmithGAniso(abs(V.z), V.x, V.y, mat.ax, mat.ay);
    float G2 = G1 * SmithGAniso(abs(L.z), L.x, L.y, mat.ax, mat.ay);

    float denom = LDotH + VDotH * eta;
    denom *= denom;
    float eta2 = eta * eta;
    float jacobian = abs(LDotH) / denom;

    pdf = G1 * max(0.0, VDotH) * D * jacobian / V.z;
    return pow(mat.baseColor, vec3(0.5)) * (1.0 - F) * D * G2 * abs(VDotH) * jacobian * eta2 / abs(L.z * V.z);
}


// 计算清漆层的brdf与pdf
vec3 EvalClearcoat(Material mat, vec3 V, vec3 L, vec3 H, out float pdf)
{
    pdf = 0.0;
    if (L.z <= 0.0)
        return vec3(0.0);
    
    float VDotH = dot(V, H);

    float F = mix(0.04, 1.0, SchlickWeight(VDotH));
    float D = GTR1(H.z, mat.clearcoatRoughness);
    float G = SmithG(L.z, 0.25) * SmithG(V.z, 0.25);
    float jacobian = 1.0 / (4.0 * VDotH);

    pdf = D * H.z * jacobian;
    return vec3(F * D * G);
}


// 重要性采样一个bsdf方向
vec3 DisneySample(State state, vec3 V, vec3 N, vec3 L, out float pdf)
{
    pdf = 0.0;

    float r1 = rand();
    float r2 = rand();

    // TODO: Tangent and bitangent should be calculated from mesh (provided, the mesh has proper uvs)
    vec3 T, B;
    ONB(N, T, B);

    // 变换到局部空间(着色空间)以简化部分计算 (NDotL = L.z; NDotV = V.z; NDotH = H.z)
    V = ToLocal(T, B, N, V);

    // Tint colors
    vec3 Csheen, Cspec0;
    float F0;
    TintColors(state.mat, state.eta, F0, Csheen, Cspec0);

    // 根据材质构建不同光学属性的权重// 各种光学属性的采样概率// 归一化采样概率
    float dielectricWt = (1.0 - state.mat.metallic) * (1.0 - state.mat.specTrans);
    float metalWt = state.mat.metallic; // 反射权重
    float transWt = (1.0 - state.mat.metallic) * state.mat.specTrans;   // 折射权重

    // 各种光学属性的采样概率
    float schlickWt = SchlickWeight(V.z);
    
    float diffusePr = dielectricWt * Luminance(state.mat.baseColor);
    float dielectricPr = dielectricWt * Luminance(mix(Cspec0, vec3(1.0), schlickWt));
    float metalPr = metalWt * Luminance(mix(state.mat.baseColor, vec3(1.0), schlickWt));
    float transPr = transWt;
    float clearcoatPr = 0.25 * state.mat.clearcoat;

    // 归一化采样概率
    float invTotalWt = 1.0 / (diffusePr + dielectricPr + metalPr + transPr + clearcoatPr);
    diffusePr *= invTotalWt;
    dielectricPr *= invTotalWt;
    metalPr *= invTotalWt;
    transPr *= invTotalWt;
    clearcoatPr *= invTotalWt;

    // 采样概率的累积分布函数cdf
    float cdf[5];
    cdf[0] = diffusePr;
    cdf[1] = cdf[0] + dielectricPr;
    cdf[2] = cdf[1] + metalPr;
    cdf[3] = cdf[2] + transPr;
    cdf[4] = cdf[3] + clearcoatPr;

    // 基于重要性选择一个lobe采样
    float r3 = rand();

    // 漫反射
    if (r3 < cdf[0])
    {
        L = CosineSampleHemisphere(r1, r2);
    }
    // 电解质+金属反射
    else if (r3 < cdf[2])
    {
        vec3 H = SampleGGXVNDF(V, state.mat.ax, state.mat.ay, r1, r2);
        if (H.z < 0.0)
            H = -H;
        L = normalize((reflect(-V, H)));    // reflect(I, N) = I + (-2*dot(I, N)*N);
    }
    // 透射
    else if (r3 < cdf[3])
    {
        vec3 H = SampleGGXVNDF(V, state.mat.ax, state.mat.ay, r1, r2);
        float F = DielectricFresnel(abs(dot(V, H)), state.eta);

        if (H.z < 0.0)
            H = -H;
        
        // Rescale random number for reuse
        r3 = (r3 - cdf[2]) / (cdf[3] - cdf[2]);

        if (r3 < F)
            L = normalize(reflect(-V, H));              // Reflection
        else
            L = normalize(refract(-V, H, state.eta));   // Transmission
    }
    // 清漆层
    else
    {
        vec3 H = SampleGTR1(state.mat.clearcoatRoughness, r1, r2);
        if(H.z < 0.0)
            H = -H;
        L = normalize(reflect(-V, H));
    }

    L = ToWorld(T, B, N, L);
    V = ToWorld(T, B, N, V);

    return DisneyEval(state, V, N, L, pdf);
}


vec3 DisneyEval(State state, vec3 V, vec3 N, vec3 L, out float pdf)
{
    pdf = 0.0;
    vec3 f = vec3(0.0);

    // TODO: Tangent and bitangent should be calculated from mesh (provided, the mesh has proper uvs)
    vec3 T, B;
    ONB(N, T, B);

    // 变换到局部空间(着色空间)以简化部分计算 (NDotL = L.z; NDotV = V.z; NDotH = H.z)
    L = ToLocal(T, B, N, L);
    V = ToLocal(T, B, N, V);

    vec3 H;
    if (L.z > 0.0)
        H = normalize(L + V);
    else
        H = normalize(L + V * state.eta);

    if(H.z < 0.0)
        H = -H;
    
    // Tint colors
    vec3 Csheen, Cspec0;
    float F0;
    TintColors(state.mat, state.eta, F0, Csheen, Cspec0);

    // 根据材质构建不同光学属性的权重
    float dielectricWt = (1.0 - state.mat.metallic) * (1.0 - state.mat.specTrans);
    float metalWt = state.mat.metallic; // 反射权重
    float transWt = (1.0 - state.mat.metallic) * state.mat.specTrans;   // 折射权重

    // 各种光学属性的采样概率
    float schlickWt = SchlickWeight(V.z);
    
    float diffusePr = dielectricWt * Luminance(state.mat.baseColor);
    float dielectricPr = dielectricWt * Luminance(mix(Cspec0, vec3(1.0), schlickWt));
    float metalPr = metalWt * Luminance(mix(state.mat.baseColor, vec3(1.0), schlickWt));
    float transPr = transWt;
    float clearcoatPr = 0.25 * state.mat.clearcoat;

    // 归一化采样概率
    float invTotalWt = 1.0 / (diffusePr + dielectricPr + metalPr + transPr + clearcoatPr);
    diffusePr *= invTotalWt;
    dielectricPr *= invTotalWt;
    metalPr *= invTotalWt;
    transPr *= invTotalWt;
    clearcoatPr *= invTotalWt;

    // L、V与法线同侧，证明是反射
    bool isReflection = (L.z * V.z) > 0.0;

    float tmpPdf = 0.0;
    float VDotH = abs(dot(V, H));

    // 漫反射
    if (diffusePr > 0.0 && isReflection)
    {
        f += EvalDisneyDiffuse(state.mat, Csheen, V, L, H, tmpPdf) * dielectricWt;
        pdf += tmpPdf * diffusePr;
    }

    // 电解质反射
    if (dielectricPr > 0.0 && isReflection)
    {
        // Normalize for interpolating based on Cspec0
        float F = (DielectricFresnel(VDotH, 1.0 / state.mat.ior) - F0) / (1.0 - F0);

        f += EvalMicrofacetReflection(state.mat, V, L, H, mix(Cspec0, vec3(1.0), F), tmpPdf) * dielectricWt;
        pdf += tmpPdf * dielectricPr;
    }

    // 金属反射
    if (metalPr > 0.0 && isReflection)
    {
        // 金属的菲涅尔基础反射率具有色彩, F的计算使用Schlick-Fresnel近似
        vec3 F = mix(state.mat.baseColor, vec3(1.0), SchlickWeight(VDotH));

        f += EvalMicrofacetReflection(state.mat, V, L, H, F, tmpPdf) * metalWt;
        pdf += tmpPdf * metalPr;
    }

    // 透射/镜面反射 BSDF
    if (transPr > 0.0)
    {
        // 电解质的菲涅尔基础反射率单色调
        float F = DielectricFresnel(VDotH, state.eta);

        if (isReflection)
        {
            f += EvalMicrofacetReflection(state.mat, V, L, H, vec3(F), tmpPdf) * transWt;
            pdf += tmpPdf * transPr * F;
        }
        else
        {
            f += EvalMicrofacetRefraction(state.mat, state.eta, V, L, H, vec3(F), tmpPdf) * transWt;
            pdf += tmpPdf * transPr * (1.0 - F);
        }
    }

    // 清漆层
    if (clearcoatPr > 0.0 && isReflection)
    {
        f += EvalClearcoat(state.mat, V, L, H, tmpPdf) * 0.25 * state.mat.clearcoat;
        pdf += tmpPdf * clearcoatPr;
    }

    return f * abs(L.z);
}










// -------------------------------------------------------------------------------------------------------------------------------
// lambert.glsl
// -------------------------------------------------------------------------------------------------------------------------------

// cosine重要性采样
vec3 LambertSample(State state, vec3 V, vec3 N, inout vec3 L, inout float pdf)
{
    float r1 = rand();
    float r2 = rand();

    vec3 T, B;
    ONB(N, T, B);

    L = CosineSampleHemisphere(r1, r2);
    L = T * L.x + B * L.y + N * L.z;

    pdf = dot(N, L) * INV_PI;
    return INV_PI * state.mat.baseColor * dot(N, L);
}

vec3 LambertEval(State state, vec3 V, vec3 N, vec3 L, inout float pdf)
{
    pdf = dot(N, L) * INV_PI;
    return INV_PI * state.mat.baseColor * dot(N, L);
}












// -------------------------------------------------------------------------------------------------------------------------------
// pathtrace.glsl
// -------------------------------------------------------------------------------------------------------------------------------

// 从materialsTex中按 C++：class Material 解析参数到 GLSL：struct Material
void GetMaterial(inout State state, Ray r)
{
    // 每个Material等同8个vec4f, 共有8组参数
    int startOffset = state.matID * 8;
    Material mat;

    vec4 param1 = texelFetch(materialsTex, ivec2(startOffset + 0, 0), 0);
    vec4 param2 = texelFetch(materialsTex, ivec2(startOffset + 1, 0), 0);
    vec4 param3 = texelFetch(materialsTex, ivec2(startOffset + 2, 0), 0);
    vec4 param4 = texelFetch(materialsTex, ivec2(startOffset + 3, 0), 0);
    vec4 param5 = texelFetch(materialsTex, ivec2(startOffset + 4, 0), 0);
    vec4 param6 = texelFetch(materialsTex, ivec2(startOffset + 5, 0), 0);
    vec4 param7 = texelFetch(materialsTex, ivec2(startOffset + 6, 0), 0);
    vec4 param8 = texelFetch(materialsTex, ivec2(startOffset + 7, 0), 0);

    mat.baseColor           = param1.rgb;
    mat.anisotropic         = param1.w;

    mat.emission            = param2.rgb;
    
    mat.metallic            = param3.x;
    mat.roughness           = max(param3.y, 0.001);
    mat.subsurface          = param3.z;
    mat.specularTint        = param3.w;

    mat.sheen               = param4.x;
    mat.sheenTint           = param4.y;
    mat.clearcoat           = param4.z;
    mat.clearcoatRoughness  = mix(0.1, 0.001, param4.w); // Remapping from gloss to roughness

    mat.specTrans           = param5.x;
    mat.ior                 = param5.y;
    mat.medium.type         = int(param5.z);
    mat.medium.density      = param5.w;

    mat.medium.color        = param6.rgb;
    mat.medium.anisotropy   = param6.w;

	int baseColorTexID      = int(param7.x);
	int roughnessTexID      = int(param7.y);
	int metallicTexID       = int(param7.z);
	int normalMapTexID      = int(param7.w);

	int emissionMapTexID    = int(param8.x);
    mat.opacity             = param8.y;
    mat.alphaMode           = int(param8.z);
    mat.alphaCutoff         = param8.w;

    // BaseColor Map
    if (baseColorTexID >= 0)
    {
        vec4 color = texture(textureMapsArrayTex, vec3(state.texCoord, baseColorTexID));
        mat.baseColor = pow(color.rgb, vec3(2.2));  // srgb to linear
        mat.opacity *= color.a;
    }

    // Roughness Map
    if (roughnessTexID >= 0)
    {
        // TODO: fix roughness?
        // float rgh = texture(textureMapsArrayTex, vec3(state.texCoord, roughnessTexID)).r;
        // mat.roughness = max(rgh * rgh, 0.001);
        mat.roughness = max(texture(textureMapsArrayTex, vec3(state.texCoord, roughnessTexID)).r, 0.001);
    }

    // Metallic Map
    if (metallicTexID >= 0)
    {
        mat.metallic = texture(textureMapsArrayTex, vec3(state.texCoord, metallicTexID)).r;
    }

    // Normal Map
    if (normalMapTexID >= 0)
    {
        vec3 texNormal = texture(textureMapsArrayTex, vec3(state.texCoord, normalMapTexID)).rgb;

#ifdef NAGI_OPENGL_NORMALMAP
        texNormal.y = 1.0 - texNormal.y;
#endif
        texNormal = normalize(texNormal * 2.0 - 1.0);   // remap

        vec3 originNormal = state.normal;   // state.normal = geometry normal
        // convert texNormal to world space
        state.normal = normalize(state.tangent * texNormal.x + state.bitangent * texNormal.y + state.normal * texNormal.z);
        state.ffnormal = dot(originNormal, r.dir) <= 0.0 ? state.normal : -state.normal;    // face forward normal
    }

    // Emission Map
    if (emissionMapTexID >= 0)
    {
        mat.emission = pow(texture(textureMapsArrayTex, vec3(state.texCoord, emissionMapTexID)).rgb, vec3(2.2));  // srgb to linear
    }

#ifdef NAGI_ROUGHNESS_MOLLIFICATION
    if(state.depth > 0)
        mat.roughness = max(mix(0.0, state.mat.roughness, roughnessMollificationAmt), mat.roughness);
#endif

    // TODO: fix roughness?
    float aspect = sqrt(1.0 - 0.9 * mat.anisotropic);
    float rgh2 = mat.roughness * mat.roughness;
    mat.ax = max(rgh2 / aspect, 0.001);
    mat.ay = max(rgh2 * aspect, 0.001);

    state.mat = mat;
    state.eta = dot(r.dir, state.normal) < 0.0 ? (1.0 / mat.ior) : mat.ior;     // eta = i/t
}


#if defined(NAGI_MEDIUM) && defined(NAGI_VOL_MIS)
vec3 EvalTransmittance(Ray r)
{
    LightSample lightSample;
    State state;
    vec3 transmittance = vec3(1.0);

    for(int depth = 0; depth < maxDepth; depth++)
    {
        bool hit = ClosestHit(r, state, lightSample);

        // 没有命中物体，或者命中的是光源, break后返回transmittance
        if (!hit || state.isEmitter)
            break;
        
        // 获取用于计算的材质参数
        int startOffset = state.matID * 8;      // 每个Material等同8个vec4f, 共有8组参数
        vec4 param5 = texelFetch(materialsTex, ivec2(startOffset + 4, 0), 0);
        vec4 param6 = texelFetch(materialsTex, ivec2(startOffset + 5, 0), 0);
        vec4 param8 = texelFetch(materialsTex, ivec2(startOffset + 7, 0), 0);

        state.mat.metallic            = texelFetch(materialsTex, ivec2(startOffset + 2, 0), 0).x;
        state.mat.specTrans           = param5.x;
        state.mat.medium.type         = int(param5.z);
        state.mat.medium.density      = param5.w;
        state.mat.medium.color        = param6.rgb;
        state.mat.medium.anisotropy   = param6.w;
        state.mat.opacity             = param8.y;
        state.mat.alphaMode           = int(param8.z);
        state.mat.alphaCutoff         = param8.w;

        // alphaTest, 测试hitPoint是否应视作透明点而被忽略
        bool alphaTest = ((state.mat.alphaMode == ALPHA_MODE_MASK  && state.mat.opacity < state.mat.alphaCutoff) ||
                          (state.mat.alphaMode == ALPHA_MODE_BLEND && rand() > state.mat.opacity));
        bool refractive = (1.0 - state.mat.metallic) * state.mat.specTrans > 0.0;

        // Refraction is ignored (Not physically correct but helps with sampling lights from inside refractive objects)
        if(!alphaTest && !refractive)
            return vec3(0.0);      // 命中实体点
        
        // TODO: Evaluate transmittance
        if (dot(state.normal, r.dir) > 0.0 && state.mat.medium.type != MEDIUM_NONE)
        {
            vec3 color = (state.mat.medium.type == MEDIUM_ABSORB) ? vec3(1.0) - state.mat.medium.color : vec3(1.0);
            transmittance *= exp(-color * state.mat.medium.density * state.hitT);
        }

        // Move ray origin to hit point
        r.ori = state.fhp + r.dir * EPSILON;
    }
    return transmittance;
}
#endif



// 计算直接光照的辐射度贡献Ld
vec3 DirectLight(Ray r, State state, bool isSurface)
{
    ScatterSample scatterSample;
    vec3 Ld = vec3(0.0);
    vec3 Li = vec3(0.0);
    vec3 scatterPos = state.fhp + state.normal * EPSILON;   // slightly offset

    /* 环境贴图采样 */
#if defined(NAGI_ENVMAP) && !defined(NAGI_UNIFORM_LIGHT)
    {
        vec3 color;
        vec4 dirPdf = SampleEnvMap(Li);
        vec3 lightDir = dirPdf.xyz;
        float lightPdf = dirPdf.w;

        Ray shadowRay = Ray(scatterPos, lightDir);

    #if defined(NAGI_MEDIUM) && defined(NAGI_VOL_MIS)
        // scene中有参与介质
        Li *= EvalTransmittance(shadowRay);
        
        if (isSurface)
            scatterSample.f = DisneyEval(state, -r.dir, state.ffnormal, lightDir, scatterSample.pdf);
        else
        {
            float p = PhaseHG(dot(-r.dir, lightDir), state.medium.anisotropy);
            scatterSample.f = vec3(p);
            scatterSample.pdf = p;
        }
        
        if (scatterSample.pdf > 0.0)
        {
            float misWeight = PowerHeuristic(lightPdf, scatterSample.pdf);
            if (misWeight > 0.0)
                Ld += misWeight * Li * scatterSample.f * envMapIntensity / lightPdf; 
        }
    #else
        // scene中无参与介质，全部为实体
        bool inShadow = AnyHit(shadowRay, INF - EPSILON);

        if (!inShadow)
        {
            scatterSample.f = DisneyEval(state, -r.dir, state.ffnormal, lightDir, scatterSample.pdf);

            if (scatterSample.pdf > 0.0)
            {
                float misWeight = PowerHeuristic(lightPdf, scatterSample.pdf);
                if (misWeight > 0.0)
                    Ld += misWeight * Li * scatterSample.f * envMapIntensity / lightPdf; 
            }
        }
    #endif
    }   /* 环境贴图采样 */
#endif


    /* 采样解析光源 */
#ifdef NAGI_LIGHTS
    {
        // 选择一个要采样的光源
        int index = int(rand() * float(lightsNum)) * 5;
        // 根据light.h中Light的成员变量顺序获取数据，以vec3f为刻度，Light等同5个vec3f
        vec3 position   = texelFetch(lightsTex, ivec2(index * 5 + 0, 0), 0).xyz;
        vec3 emission   = texelFetch(lightsTex, ivec2(index * 5 + 1, 0), 0).xyz;
        vec3 u          = texelFetch(lightsTex, ivec2(index * 5 + 2, 0), 0).xyz;     // u vector for rect
        vec3 v          = texelFetch(lightsTex, ivec2(index * 5 + 3, 0), 0).xyz;     // v vector for rect
        vec3 params     = texelFetch(lightsTex, ivec2(index * 5 + 4, 0), 0).xyz;
        float radius    = params.x;
        float area      = params.y;
        float type      = params.z;     // 0->Rect, 1->Sphere, 2->Distant

        LightSample lightSample;
        Light light = Light(position, emission, u, v, radius, area, type);
        SampleOneLight(light, scatterPos, lightSample);
        Li = lightSample.emission;

        // quad光源具有单边性，所以需要判断是否在背面，只处理正面的
        if (dot(lightSample.direction, lightSample.normal) < 0.0)
        {
            Ray shadowRay = Ray(scatterPos, lightSample.direction);

    #if defined(NAGI_MEDIUM) && defined(NAGI_VOL_MIS)
            // scene中有参与介质
            Li *= EvalTransmittance(shadowRay);
            
            if (isSurface)
                scatterSample.f = DisneyEval(state, -r.dir, state.ffnormal, lightSample.direction, scatterSample.pdf);
            else
            {
                float p = PhaseHG(dot(-r.dir, lightSample.direction), state.medium.anisotropy);
                scatterSample.f = vec3(p);
                scatterSample.pdf = p;
            }
            
            float misWeight = 1.0;
            if (light.area > 0.0)   // 非无限远平行光源才计算misWe、ight
                misWeight = PowerHeuristic(lightSample.pdf, scatterSample.pdf);
            
            if (scatterSample.pdf > 0.0)
                Ld += misWeight * Li * scatterSample.f / lightSample.pdf; 
    #else
            // scene中无参与介质，全部为实体
            bool inShadow = AnyHit(shadowRay, lightSample.dist - EPSILON);

            if (!inShadow)
            {
                scatterSample.f = DisneyEval(state, -r.dir, state.ffnormal, lightSample.direction, scatterSample.pdf);

                float misWeight = 1.0;
                if (light.area > 0.0)   // 非无限远平行光源才计算misWe、ight
                    misWeight = PowerHeuristic(lightSample.pdf, scatterSample.pdf);
                
                if (scatterSample.pdf > 0.0)
                    Ld += misWeight * Li * scatterSample.f / lightSample.pdf; 
            }
    #endif
        }
    }   /* 采样解析光源 */
#endif
    
    
    return Ld;
}


vec4 PathTrace(Ray r)
{
    vec3 radiance = vec3(0.0);
    vec3 throughput = vec3(1.0);
    State state;
    LightSample lightSample;
    ScatterSample scatterSample;

    // FIXME: alpha from material opacity/medium density
    float alpha = 1.0;

    // For medium tracking
    bool inMedium = false;
    bool mediumSampled = false;
    bool surfaceScatter = false;
    
    for(state.depth = 0;; state.depth++)
    {
        bool hit = ClosestHit(r, state, lightSample);

        /* 未命中物体或光源 */
        if (!hit)
        {
    #if defined(NAGI_BACKGROUND) || defined(NAGI_TRANSPARENT_BACKGROUND)
            if (state.depth == 0)
                alpha = 0.0;
    #endif

    #ifdef NAGI_HIDE_EMITTERS
            if (state.depth > 0)
    #endif
            {
        #ifdef NAGI_UNIFORM_LIGHT
                radiance += uniformLightCol * throughput;
        #else
            #ifdef NAGI_ENVMAP
                // 计算envMap贡献的radiance
                vec4 envMapColPdf = EvalEnvMap(r);

                float misWeight = 1.0;
                // 使用采样envMap得到的pdf与上一条递归光线的pdf计算misWeight
                if (state.depth > 0)
                    misWeight = PowerHeuristic(scatterSample.pdf, envMapColPdf.w);

            #if defined(NAGI_MEDIUM) && defined(NAGI_VOL_MIS)
                if (!surfaceScatter)
                    misWeight = 1.0;
            #endif

                /* 累计envMap的radiance贡献 */
                if (misWeight > 0.0)
                    radiance += misWeight * envMapColPdf.rgb * throughput * envMapIntensity;
            #endif
        #endif
            }
            break;      // 未命中则退出pathtrace
        }   /* 未命中物体或光源 */

        /* 获取scatterPos的材质信息 */
        GetMaterial(state, r);

        /* 累计发光物体的radiance贡献。此处不进行重要性采样 */
        radiance += state.mat.emission * throughput;

        /* 累计光源的radiance贡献 */
    #ifdef NAGI_LIGHTS
        if (state.isEmitter)    // ray击中光源
        {
            float misWeight = 1.0;
            if (state.depth > 0)
                misWeight = PowerHeuristic(scatterSample.pdf, lightSample.pdf);
            
        #if defined(NAGI_MEDIUM) && defined(NAGI_VOL_MIS)
            if (!surfaceScatter)
                misWeight = 1.0;
        #endif

            radiance += misWeight * lightSample.emission * throughput;
            break;      // 命中的是光源则退出pathtrace
        }
    #endif

        // 达到最大递归深度则退出pathtrace
        if (state.depth == maxDepth)
            break;
        
    #ifdef NAGI_MEDIUM
        mediumSampled = false;
        surfaceScatter = false;

        // 处理参与介质中的吸收、发光、散射
        // TODO: Handle light sources placed inside medium
        if (inMedium)
        {
            if (state.medium.type == MEDIUM_ABSORB)
            {
                throughput *= exp(-(1.0 - state.medium.color) * state.medium.density * state.hitT);
            }
            else if (state.medium.type == MEDIUM_EMISSIVE)
            {
                radiance += state.medium.color * state.medium.density * state.hitT * throughput;
            }
            else
            {
                // 在参与介质中采样一个距离
                float scatterDist = min(-log(rand()) / state.medium.density, state.hitT);
                mediumSampled = scatterDist < state.hitT;       // prob equal

                if (mediumSampled)
                {
                    throughput *= state.medium.color;

                    // Move ray origin to scattering position
                    r.ori = r.ori + r.dir * scatterDist;
                    state.fhp = r.ori;

                    // Transmittance Evaluation
                    radiance += DirectLight(r, state, false) * throughput;

                    // Pick a new direction based on the phase function
                    vec3 scatterDir = SampleHG(-r.dir, state.medium.anisotropy, rand(), rand());
                    scatterSample.pdf = PhaseHG(dot(-r.dir, scatterDir), state.medium.anisotropy);
                    r.dir = scatterDir;
                }
            }
        }
    #endif

    #ifdef NAGI_MEDIUM
        if (!mediumSampled)
    #endif
        {
        #ifdef NAGI_ALPHA_TEST
            // Ignore intersection and continue ray based on alpha test
            if (((state.mat.alphaMode == ALPHA_MODE_MASK  && state.mat.opacity < state.mat.alphaCutoff) || 
                 (state.mat.alphaMode == ALPHA_MODE_BLEND && rand() > state.mat.opacity)))
            {
                scatterSample.L = r.dir;
                state.depth--;
            }
            else
        #endif
            {
                surfaceScatter = true;

                // Next event estimation
                radiance += DirectLight(r, state, true) * throughput;

                // Sample BSDF for color and outgoing direction
                scatterSample.f = DisneySample(state, -r.dir, state.ffnormal, scatterSample.L, scatterSample.pdf);
                if (scatterSample.pdf > 0.0)
                    throughput *= scatterSample.f / scatterSample.pdf;
                else
                    break;
            }

            // Move ray origin to hit point and set direction for next bounce
            r.dir = scatterSample.L;
            r.ori = state.fhp + r.dir * EPSILON;

        #ifdef NAGI_MEDIUM
            // Note: Nesting of volumes isn't supported due to lack of a volume stack for performance reasons
            // Ray is in medium only if it is entering a surface containing a medium
            if (dot(r.dir, state.normal) < 0.0 && state.mat.medium.type != MEDIUM_NONE)
            {
                inMedium = true;
                // Get medium params from the intersected object
                state.medium = state.mat.medium;
            }
            // FIXME: Objects clipping or inside a medium were shaded incorrectly as inMedium would be set to false.
            // This hack works for now but needs some rethinking
            else if (state.mat.medium.type != MEDIUM_NONE)
                inMedium = false;
        #endif
        }

        // 俄罗斯轮盘赌
    #ifdef NAGI_RR
        if (state.depth >= NAGI_RR_DEPTH)
        {
            float q = min(max(throughput.x, max(throughput.y, throughput.z)) + 0.001, 0.95);
            if (rand() > q)
                break;
            throughput /= q;
        }
    #endif
    
    }

    return vec4(radiance, alpha);
}
