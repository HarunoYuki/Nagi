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

// TODO: uint32_t��uint16_t��Χ���ޣ�������scene�ı�ʾ���������Ҫ��Ⱦ���ӳ�����������Ҫ������������
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
		uint32_t primitivesOffset;		// orderedPrimsIndices������������blasBVH��Ҷ�ӽڵ�
		uint32_t secondChildOffset;		// nodes������������blasBVH��tlasBVH���м�ڵ㣬ֻ��Ҫ�洢��������idx��������idxΪcurIdx+1
		uint32_t blasBVHStartOffset;	// sceneNodes������������tlasBVH��Ҷ�ӽڵ㣬��ʾblasBVH�Ĵ洢���
	};
	union {
		// (nodeIdx < tlasBVHStartOffset) ? blasBVh : tlasBVH
		// blasBVH��nPrimitives == 0 ���м�ڵ㣬nPrimitives != 0 ��Ҷ�Ӵ洢��prim��
		// tlasBVH��nPrimitives == 0 ���м�ڵ㣬meshInstanceIdx != 0 ��Ҷ�����õ�blasBVH��Ӧ��instanceIdx
		uint32_t nPrimitives;
		uint32_t meshInstanceIdx;		// ��¼instanceIndex��Ϊ����GLSL�б���ʱ����transformTex
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

	// ������scene�д���blas��tlas
	// �ⲿScene�����Ԫ����������private����Ϊprivate�����¶��ⲿ���غ�����������ôBVHAccel����޷������ú���
	//friend void Scene::ProcessScene();
	//friend void Scene::ProcessBLAS();
	//friend void Scene::ProcessTLAS();
	friend class Scene;

protected:
	// չƽ���BVHNodes
	std::vector<LinearBVHNode> nodes;
	// BVH�нڵ�����
	uint32_t nodeCounts;
	// ������Ҷ��˳���������е�ͼԪ����
	std::vector<uint32_t> orderedPrimsIndices;
	// Ҷ���п��Դ洢�����ͼԪ��
	const int maxPrimsInNode;
	// �м�ڵ�Ļ��ֲ���
	const SplitMethod splitMethod;
	// SAHͰ������
	const int nBuckets;
	// SAH�б����ڵ�ĳɱ�
	const float traversalCost;

private:
	// ��ָ�봴��BVH
	uint32_t recursiveBuild(std::vector<BVHPrimitiveInfo>& primitivesInfo, int start, int end);
	uint32_t HLBVHBuild(std::vector<BVHPrimitiveInfo>& primitivesInfo);
};

NAMESPACE_END(nagi)