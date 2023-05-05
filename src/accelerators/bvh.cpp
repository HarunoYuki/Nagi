#include "bvh.h"


NAMESPACE_BEGIN(nagi)

BVHAccel::BVHAccel(std::vector<bbox3f>& bounds, int maxPrimsInNode,
					SplitMethod splitMethod, int nBuckets, float traversalCost)
	:maxPrimsInNode(std::min(255, maxPrimsInNode)),
	splitMethod(splitMethod),
	nBuckets(std::min(64, nBuckets)),
	traversalCost(traversalCost),
	nodeCounts(0)
{
	// one bbox corresponds to one primitive(not actual geometry shape)
	if (bounds.empty())	return;
	
	// For each primitive to be stored in the BVH, we store 
	// the centroid of its bounding box, its complete bounding box, and its index in the primitives array 
	std::vector<BVHPrimitiveInfo> primitivesInfo(bounds.size());
	for (size_t i = 0; i < bounds.size(); i++) {
		primitivesInfo[i] = BVHPrimitiveInfo{ i, bounds[i] };
	}

	// Ԥ����
	nodes.resize(2 * bounds.size() - 1);
	orderedPrimsIndices.reserve(bounds.size());

	if (SplitMethod::HLBVH)
		HLBVHBuild(primitivesInfo);
	else
		recursiveBuild(primitivesInfo, 0, bounds.size());

	// ����ʵ��nodeCounts�ͷŶ�����пռ�
	nodes.resize(nodeCounts);
}

bbox3f BVHAccel::WorldBound()
{
	return nodes.empty() ? bbox3f() : nodes[0].bounds;
}

uint32_t BVHAccel::recursiveBuild(std::vector<BVHPrimitiveInfo>& primitivesInfo, int start, int end)
{
	if (start == end) Error("Start cannot equal to End in BVH building.");

	// ��¼node��nodes�е�idx
	uint32_t curNodeOffset = nodeCounts;
	LinearBVHNode& node = nodes[nodeCounts++];

	// ����[start, end)��bbox
	bbox3f bound;
	for (int i = start; i < end; i++)
		bound.grow(primitivesInfo[i].bounds);

	uint16_t nPrimitives = end - start;
	if (nPrimitives == 1) {
		// Create leaf _LinearBVHNode_
		uint32_t firstPrimOffset = (uint32_t)orderedPrimsIndices.size();
		for (int i = start; i < end; i++)
			orderedPrimsIndices.push_back(primitivesInfo[i].primitiveIdx);
		node.InitLeaf(bound, firstPrimOffset, nPrimitives);
		return curNodeOffset;
	}
	else {
		// ����ͼԪ��Χ�����ĵ�bbox
		bbox3f centroidBounds;
		for (int i = start; i < end; i++)
			centroidBounds.grow(primitivesInfo[i].centroid);
		int dim = centroidBounds.MaximumExtent();

		// PBRT-V3 261 page ��Χ�����Ϊ��, ֻ��һ����
		// If all of the centroid points are at the same position (i.e., the centroid bounds have zero volume),
		// then recursion stops and a leaf node is created with the primitives; 
		// none of the splitting methods here is effective in that (unusual) case.
		if (centroidBounds.pMin[dim] == centroidBounds.pMax[dim])
		{
			// Create leaf _LinearBVHNode_
			uint32_t firstPrimOffset = (uint32_t)orderedPrimsIndices.size();
			for (int i = start; i < end; i++)
				orderedPrimsIndices.push_back(primitivesInfo[i].primitiveIdx);
			node.InitLeaf(bound, firstPrimOffset, nPrimitives);
			return curNodeOffset;
		}
		else {
			int mid = (start + end) / 2;

			// ����splitMethod����[start,end)Ϊ�����Ӽ�
			switch (splitMethod)
			{
			case SplitMethod::Middle: {
				// ��centroidBounds�����Ļ���
				float pmid = centroidBounds.Center()[dim];
				BVHPrimitiveInfo* midPtr = std::partition(
					&primitivesInfo[start], &primitivesInfo[end-1]+1,
					[dim, pmid](const BVHPrimitiveInfo& pi) {
						return pi.centroid[dim] < pmid;
					});
				mid = midPtr - &primitivesInfo[0];
				// PBRT-V3 262 page ���prims������أ�Middle�Ļ��ֿ���ʧ�ܣ��������ͣ�����EqualCounts�ٻ���һ��
				// For lots of prims with large overlapping bounding boxes, this may fail to partition;
				// in that case don't break and fall through to EqualCounts.
				if (mid != start && mid != end) break;
			}
			case SplitMethod::EuqalCounts: {
				// ���Ӽ�����Ϊ��ͬ��С
				mid = (start + end) / 2;
				std::nth_element(&primitivesInfo[start], &primitivesInfo[mid], &primitivesInfo[end-1]+1, 
					[dim](const BVHPrimitiveInfo& a, const BVHPrimitiveInfo& b) {
						return a.centroid[dim] < b.centroid[dim];
					});
				break;
			}
			case SplitMethod::SAH:
			default: {
				// PBRT-V3 265 page. nPrimitiveСʱ����������SAH�����㣬������EuqalCounts����SAH
				// Partition primitives using approximate SAH (SplitMethod::EuqalCounts)
				if (nPrimitives <= 2)
				{
					// ���Ӽ�����Ϊ��ͬ��С
					mid = (start + end) / 2;
					std::nth_element(&primitivesInfo[start], &primitivesInfo[mid], &primitivesInfo[end-1]+1,
						[dim](const BVHPrimitiveInfo& a, const BVHPrimitiveInfo& b) {
							return a.centroid[dim] < b.centroid[dim];
						});
				}
				else {
					// Allocate _BucketInfo_ for SAH partition buckets
					std::vector<BucketInfo> buckets(nBuckets);

					// Initialize _BucketInfo_ for SAH partition buckets
					for (int i = start; i < end; i++)
					{
						/*�˴�����range����Ϊ�Ⱦ������bucket��Ȼ�����ͼԪbbox�����ĵ���nodeBbox�Ĺ�һ���ֲ������жϣ���ͼԪ�����ĵ�����bucket*/
						int b = nBuckets * centroidBounds.LocalNormalizedCoord(primitivesInfo[i].centroid)[dim];
						if (b < 0 || b > nBuckets) Error("Bucket Idx is out of range");
						if (b == nBuckets) b -= 1;
						buckets[b].count++;
						buckets[b].bound.grow(primitivesInfo[i].bounds);
					}

					float minCost = std::numeric_limits<float>::max();
					int minCostSplitBucket = -1;
					float invNodeBoundArea = 1.0f / bound.SurfaceArea();

#ifdef USE_PBRT_SAH_COMPUTE_COST
					// PBRT-V3 267 page. ���Ӷ�O(n^2)
					// �������л��ֿ��ܣ����㰴ĳ��bucket�����Ӽ���cost
					for (int i = 0; i < nBuckets - 1; i++)
					{
						bbox3f leftBounds, rightBounds;
						int leftCount, rightCount;
						for (int j = 0; j <= i; j++)
						{
							leftCount += buckets[j].count;
							leftBounds.grow(buckets[j].bound);
						}
						for (int j = i + 1; j < nBuckets; j++)
						{
							rightCount += buckets[j].count;
							rightBounds.grow(buckets[j].bound);
						}

						float cost = traversalCost +
									(leftCount * leftBounds.SurfaceArea() +
									 rightCount * rightBounds.SurfaceArea()) * invNodeBoundArea;

						// ��¼��Сcost��bucketIdx
						if (cost < minCost) {
							minCost = cost;
							minCostSplitBucket = i;
						}
					}
#else
					// ɨ���㷨����ʵ��O(n)�ĸ��Ӷ�
					// ����ɨ�裬�洢����bucketIdx�����µ�rightBound
					std::vector<bbox3f> rightBounds(nBuckets - 1);
					bbox3f rightBbox;
					for (int i = nBuckets - 1; i > 0; i++)
					{
						rightBbox.grow(buckets[i].bound);
						rightBounds[i - 1] = rightBbox;
					}

					// ��rightBound == bound��״̬��ʼ��ʼ��
					bbox3f leftBounds;
					int leftCount = 0;
					int rightCount = nPrimitives;

					for (int i = 0; i < nBuckets - 1; i++)
					{
						leftBounds.grow(buckets[i].bound);
						leftCount += buckets[i].count;
						rightCount -= buckets[i].count;

						float cost = traversalCost +
									(leftCount * leftBounds.SurfaceArea() +
									 rightCount * rightBounds[i].SurfaceArea()) * invNodeBoundArea;
						
						// ��¼��Сcost��bucketIdx
						if (cost < minCost) {
							minCost = cost;
							minCostSplitBucket = i;
						}
					}
#endif // USE_PBRT_SAH_COMPUTE_COST

					// Either create leaf or split primitives at selected SAH bucket
					// ���ֱ�Ӵ���Ҷ�ӣ���ôcost����nPrimitives
					float leafCost = nPrimitives;
					if (nPrimitives > maxPrimsInNode || minCost < leafCost) {
						BVHPrimitiveInfo* pmid = std::partition(
							&primitivesInfo[start], &primitivesInfo[end - 1] + 1,
							[=](const BVHPrimitiveInfo& pi) {
								int b = nBuckets * centroidBounds.LocalNormalizedCoord(pi.centroid)[dim];
								if (b < 0 || b > nBuckets) Error("Bucket Idx is out of range");
								if (b == nBuckets) b -= 1;
								return b <= minCostSplitBucket;
							});
						mid = pmid - &primitivesInfo[0];
					}
					else {
						// Create leaf _LinearBVHNode_
						uint32_t firstPrimOffset = (uint32_t)orderedPrimsIndices.size();
						for (int i = start; i < end; i++)
							orderedPrimsIndices.push_back(primitivesInfo[i].primitiveIdx);
						node.InitLeaf(bound, firstPrimOffset, nPrimitives);
						return curNodeOffset;
					}
				}
				break;
			}	// case SAH brace
			}	// switch brace

			// LinearBVHֻ��Ҫ�洢������������
			// �ݹ鴴��������
			recursiveBuild(primitivesInfo, start, mid);
			// �õݹ鴴����������������ʼ����ǰnode
			node.InitInterior(bound, recursiveBuild(primitivesInfo, mid, end), dim);
		}
	}

	return curNodeOffset;
}

uint32_t BVHAccel::HLBVHBuild(std::vector<BVHPrimitiveInfo>& primitivesInfo)
{
	// TODO: using SplitMethod::HLBVH to build BVH
	return int();
}



NAMESPACE_END(nagi)
