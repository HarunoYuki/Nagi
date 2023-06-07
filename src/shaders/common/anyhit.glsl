/*
	判断是否ray与object是否有交点
*/

/*
AnyHit中的遍历方法与PBRT-V3的比较：
首先这是TLAS和BLAS的结合遍历，pbrt中是单个BVH的遍历，叶子是BVH的话，递归到另一个BVH的Intersect函数。
anyhit中对于中间节点的处理效率低于pbrt。
要测试是否有交点，不需要前向遍历时就计算左右子树的hitT，
pbrt中利用splitAxis决定nextNodeIdx，并将另一个入栈，延迟了某个子树的hitT计算
这对于只测试是否有交点是有性能优势的，因为之前延迟的计算可能不会实际执行。
*/

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

    /* BVH Traversal */

    int nodesToVisit[64];
    int toVisitOffset = 0;
    nodesToVisit[toVisitOffset++] = -1;

    // curNodeIdx，当前处理节点的索引
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

                    // alphaTest, 测试hitPoint是否应视作透明点而被忽略
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
