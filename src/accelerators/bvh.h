#pragma once
#include <vector>
#include "bounds3.h"
#include "scene.h"

NAMESPACE_BEGIN(nagi)

struct BVHPrimitiveInfo
{
	BVHPrimitiveInfo() {}
	BVHPrimitiveInfo(int idx, bbox3f box)
		:primitiveIdx(idx), bounds(box), centroid(bounds.Center()) {}
	int primitiveIdx;	// primitive's idx in bounds array, its meaning equal to "bboxIdx"
	bbox3f bounds;	// primitive's bounding box
	vec3f centroid;
};

// TODO: uint32_t、uint16_t范围有限，限制了scene的表示能力，如果要渲染复杂场景，可能需要更改数据类型
struct LinearBVHNode
{
	void InitLeaf(bbox3f& box, uint32_t firstPrimOffset, uint32_t nPrims)
	{
		bounds = box;
		primitivesOffset = firstPrimOffset;
		nPrimitives = nPrims;
	}
	void InitInterior(bbox3f& box, uint32_t rightChildOffset, uint32_t axis)
	{
		bounds = box;
		secondChildOffset = rightChildOffset;
		nPrimitives = 0;
		splitAxis = axis;
	}
	bbox3f bounds;
	union {
		uint32_t primitivesOffset;		// orderedPrimsIndices的索引，用于blasBVH的叶子节点
		uint32_t secondChildOffset;		// nodes的索引，用于blasBVH和tlasBVH的中间节点，只需要存储右子树的idx，左子树idx为curIdx+1
		uint32_t blasBVHStartOffset;	// sceneNodes的索引，用于tlasBVH的叶子节点，表示blasBVH的存储起点
	};
	union {
		// (nodeIdx < tlasBVHStartOffset) ? blasBVh : tlasBVH
		// blasBVH，nPrimitives == 0 是中间节点，nPrimitives != 0 是叶子存储的prim数
		// tlasBVH，nPrimitives == 0 是中间节点，meshInstanceIdx != 0 是叶子引用的blasBVH对应的instanceIdx
		uint32_t nPrimitives;
		uint32_t meshInstanceIdx;		// 记录instanceIndex，为了在GLSL中遍历时访问transformTex
	};
	union {
		uint32_t splitAxis;				// used for interior, indicate which axis is used to split when bvh bulid
		uint32_t materialID;			// used for tlasBVH, indicate which material is used for blasBVH stored in leaf
	};
	// uint8_t padding;					// keep struct be n-times 32 byte
};

struct BucketInfo
{
	int count = 0;
	bbox3f bound;
};

class BVHAccel
{
public:
	enum SplitMethod {
		Middle, EuqalCounts, SAH, HLBVH
	};

	BVHAccel(std::vector<bbox3f>& bounds,
			int maxPrimsInNode = 1,
			SplitMethod splitMethod = SplitMethod::SAH,
			int nBuckets = 12,
			float traversalCost = 1.0f);
	~BVHAccel() {}

	bbox3f WorldBound();

	// 方便在scene中处理blas、tlas
	// 外部Scene类的友元函数不能是private，因为private将导致对外部隐藏函数声明，那么BVHAccel类就无法看到该函数
	//friend void Scene::ProcessScene();
	//friend void Scene::ProcessBLAS();
	//friend void Scene::ProcessTLAS();
	friend class Scene;

protected:
	// 展平后的BVHNodes
	std::vector<LinearBVHNode> nodes;
	// BVH中节点总数
	uint32_t nodeCounts;
	// 按创建叶子顺序重新排列的图元索引
	std::vector<uint32_t> orderedPrimsIndices;
	// 叶子中可以存储的最大图元数
	const int maxPrimsInNode;
	// 中间节点的划分策略
	const SplitMethod splitMethod;
	// SAH桶的数量
	const int nBuckets;
	// SAH中遍历节点的成本
	const float traversalCost;

private:
	// 无指针创建BVH
	uint32_t recursiveBuild(std::vector<BVHPrimitiveInfo>& primitivesInfo, int start, int end);
	uint32_t HLBVHBuild(std::vector<BVHPrimitiveInfo>& primitivesInfo);
};

NAMESPACE_END(nagi)