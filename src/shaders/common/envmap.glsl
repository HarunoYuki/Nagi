/*
	采样环境贴图，实现IBL
*/

#ifdef NAGI_ENVMAP
#ifndef NAGI_UNIFORM_LIGHT

// 坐标系第一象限的二维二分查找，查找value的(x, y)坐标
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
    // 核心在于球面坐标的(theta, phi)究竟是哪两个角，与哪个坐标轴相关
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
    // 哪个像素的luminance大，哪个像素就更可能被采样
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

#endif
#endif