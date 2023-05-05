#pragma once
#include <vector>
#include "bounds3.h"
#include "scene.h"

NAMESPACE_BEGIN(nagi)

struct BVHPrimitiveInfo
{
	BVHPrimitiveInfo(int idx, bbox3f box)
		:primitiveIdx(idx), bounds(box), centroid(bounds.Center()) {}
	int primitiveIdx;	// primitive's idx in bounds array, its meaning equal to "bboxIdx"
	bbox3f bounds;	// primitive's bounding box
	vec3f centroid;
};

struct LinearBVHNode
{
	void InitLeaf(bbox3f& box, uint32_t firstPrimOffset, uint16_t nPrims)
	{
		bounds = box;
		primitivesOffset = firstPrimOffset;
		nPrimitives = nPrims;
	}
	void InitInterior(bbox3f& box, uint32_t rightChildOffset, uint16_t axis)
	{
		bounds = box;
		secondChildOffset = rightChildOffset;
		nPrimitives = 0;
		splitAxis = axis;
	}
	bbox3f bounds;
	union {
		uint32_t primitivesOffset;	// used for leaf, belongs to orderedPrimsIndices
		uint32_t secondChildOffset;	// used for interior node, belongs to nodes
		uint32_t blasBVHStartOffset; // used for tlasBVH, indicate the start position of blasBVH stored in leaf
	};
	uint16_t nPrimitives; // if nPrimitives == 0, cur node is interior, otherwise leaf
	union {
		uint16_t splitAxis;	// used for interior, indicate which axis is used to split when bvh bulid
		uint16_t materialID;// used for tlasBVH, indicate which material is used for blasBVH stored in leaf
	};
	// uint8_t splitAxis;	// use for interior, indicate which axis is used to split when bvh bulid
	// uint8_t padding;	// keep struct be n-times 32 byte
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
	friend void Scene::ProcessScene();
	friend void Scene::ProcessBLAS();
	friend void Scene::ProcessTLAS();

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