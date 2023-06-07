/*
	Disney BSDF
*/

/* References:
 * [1] [Physically Based Shading at Disney] https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf
 * [2] [Extending the Disney BRDF to a BSDF with Integrated Subsurface Scattering] https://blog.selfshadow.com/publications/s2015-shading-course/burley/s2015_pbs_disney_bsdf_notes.pdf
 * [3] [The Disney BRDF Explorer] https://github.com/wdas/brdf/blob/main/src/brdfs/disney.brdf
 * [4] [Miles Macklin's implementation] https://github.com/mmacklin/tinsel/blob/master/src/disney.h
 * [5] [Simon Kallweit's project report] http://simon-kallweit.me/rendercompo2015/report/
 * [6] [Microfacet Models for Refraction through Rough Surfaces] https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
 * [7] [Sampling the GGX Distribution of Visible Normals] https://jcgt.org/published/0007/04/01/paper.pdf
 * [8] [Pixar's Foundation for Materials] https://graphics.pixar.com/library/PxrMaterialsCourse2017/paper.pdf
 * [9] [Mitsuba 3] https://github.com/mitsuba-renderer/mitsuba3
 */

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