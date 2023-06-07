/*
	微表面模型功能函数，包括
    各种法线分布函数及重要性采样
    电解质和导体的菲涅尔方程
    SmithGGX几何函数
    cosine半球采样、均匀球面/半球采样
    矩形、球形、平行光源采样
*/

/* References:
 * [1] [Physically Based Shading at Disney] https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf
 * [2] [Extending the Disney BRDF to a BSDF with Integrated Subsurface Scattering] https://blog.selfshadow.com/publications/s2015-shading-course/burley/s2015_pbs_disney_bsdf_notes.pdf
 * [3] [The Disney BRDF Explorer] https://github.com/wdas/brdf/blob/main/src/brdfs/disney.brdf
 */

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
    lightSample.emission = light.emission * float(lightsNum);   // ?
    
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
    lightSample.emission = light.emission * float(lightsNum);
    
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
    lightSample.emission = light.emission * float(lightsNum);
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