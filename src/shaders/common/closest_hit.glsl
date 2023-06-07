/*
	计算ray与scene的最近hitPoint，用hitPoint的几何、材质等信息更新state
*/

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