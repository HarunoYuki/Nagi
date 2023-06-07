/*
	简单材质朗伯体
*/

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

// 计算朗伯体的pdf与brdf
vec3 LambertEval(State state, vec3 V, vec3 N, vec3 L, inout float pdf)
{
    pdf = dot(N, L) * INV_PI;
    return INV_PI * state.mat.baseColor * dot(N, L);
}