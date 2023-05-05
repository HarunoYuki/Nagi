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

	// 预分配
	nodes.resize(2 * bounds.size() - 1);
	orderedPrimsIndices.reserve(bounds.size());

	if (SplitMethod::HLBVH)
		HLBVHBuild(primitivesInfo);
	else
		recursiveBuild(primitivesInfo, 0, bounds.size());

	// 根据实际nodeCounts释放多余空闲空间
	nodes.resize(nodeCounts);
}

bbox3f BVHAccel::WorldBound()
{
	return nodes.empty() ? bbox3f() : nodes[0].bounds;
}

uint32_t BVHAccel::recursiveBuild(std::vector<BVHPrimitiveInfo>& primitivesInfo, int start, int end)
{
	if (start == end) Error("Start cannot equal to End in BVH building.");

	// 记录node在nodes中的idx
	uint32_t curNodeOffset = nodeCounts;
	LinearBVHNode& node = nodes[nodeCounts++];

	// 计算[start, end)的bbox
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
		// 计算图元包围盒质心的bbox
		bbox3f centroidBounds;
		for (int i = start; i < end; i++)
			centroidBounds.grow(primitivesInfo[i].centroid);
		int dim = centroidBounds.MaximumExtent();

		// PBRT-V3 261 page 包围盒体积为空, 只有一个点
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

			// 基于splitMethod划分[start,end)为两个子集
			switch (splitMethod)
			{
			case SplitMethod::Middle: {
				// 按centroidBounds的质心划分
				float pmid = centroidBounds.Center()[dim];
				BVHPrimitiveInfo* midPtr = std::partition(
					&primitivesInfo[start], &primitivesInfo[end-1]+1,
					[dim, pmid](const BVHPrimitiveInfo& pi) {
						return pi.centroid[dim] < pmid;
					});
				mid = midPtr - &primitivesInfo[0];
				// PBRT-V3 262 page 如果prims混叠严重，Middle的划分可能失败（树质量低），用EqualCounts再划分一次
				// For lots of prims with large overlapping bounding boxes, this may fail to partition;
				// in that case don't break and fall through to EqualCounts.
				if (mid != start && mid != end) break;
			}
			case SplitMethod::EuqalCounts: {
				// 把子集划分为相同大小
				mid = (start + end) / 2;
				std::nth_element(&primitivesInfo[start], &primitivesInfo[mid], &primitivesInfo[end-1]+1, 
					[dim](const BVHPrimitiveInfo& a, const BVHPrimitiveInfo& b) {
						return a.centroid[dim] < b.centroid[dim];
					});
				break;
			}
			case SplitMethod::SAH:
			default: {
				// PBRT-V3 265 page. nPrimitive小时，用完整的SAH不划算，所以用EuqalCounts近似SAH
				// Partition primitives using approximate SAH (SplitMethod::EuqalCounts)
				if (nPrimitives <= 2)
				{
					// 把子集划分为相同大小
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
						/*此处，将range划分为等距的数个bucket，然后根据图元bbox的中心点在nodeBbox的归一化局部坐标判断，该图元的中心点所在bucket*/
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
					// PBRT-V3 267 page. 复杂度O(n^2)
					// 遍历所有划分可能，计算按某个bucket划分子集的cost
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

						// 记录最小cost及bucketIdx
						if (cost < minCost) {
							minCost = cost;
							minCostSplitBucket = i;
						}
					}
#else
					// 扫描算法可以实现O(n)的复杂度
					// 倒序扫描，存储各个bucketIdx划分下的rightBound
					std::vector<bbox3f> rightBounds(nBuckets - 1);
					bbox3f rightBbox;
					for (int i = nBuckets - 1; i > 0; i++)
					{
						rightBbox.grow(buckets[i].bound);
						rightBounds[i - 1] = rightBbox;
					}

					// 从rightBound == bound的状态开始初始化
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
						
						// 记录最小cost及bucketIdx
						if (cost < minCost) {
							minCost = cost;
							minCostSplitBucket = i;
						}
					}
#endif // USE_PBRT_SAH_COMPUTE_COST

					// Either create leaf or split primitives at selected SAH bucket
					// 如果直接创建叶子，那么cost就是nPrimitives
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

			// LinearBVH只需要存储右子树的索引
			// 递归创建左子树
			recursiveBuild(primitivesInfo, start, mid);
			// 用递归创建右子树的索引初始化当前node
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
