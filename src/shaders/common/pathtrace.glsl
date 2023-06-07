/*
	路径追踪核心
*/


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
