#include "scene.h"
#include "camera.h"
#include "environmentMap.h"
#include "texture.h"
#include "mesh.h"
#include "material.h"
#include "light.h"
#include "bvh.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

NAMESPACE_BEGIN(nagi)

Scene::Scene() 
	:tlasBVH(nullptr), camera(nullptr), envMap(nullptr), renderOptions(new RenderOptions),
	initialized(false), dirty(true), instancesModified(true), envMapModified(true) {}

Scene::~Scene()
{
	if (tlasBVH)
		delete tlasBVH;

	if (renderOptions)
		delete renderOptions;

	if (camera)
		delete camera;

	if (envMap)
		delete envMap;

	for (size_t i = 0; i < textures.size(); i++)
		delete textures[i];
	textures.clear();

	for (size_t i = 0; i < meshes.size(); i++)
		delete meshes[i];
	meshes.clear();

	for (size_t i = 0; i < meshInstances.size(); i++)
		delete meshInstances[i];
	meshInstances.clear();

	//for (size_t i = 0; i < materials.size(); i++)
	//	delete materials[i];
	//materials.clear();

	//for (size_t i = 0; i < lights.size(); i++)
	//	delete lights[i];
	//lights.clear();
}

void Scene::AddCamera(vec3f pos, vec3f lookat, float fov)
{
	if (camera)
		delete camera;
	camera = new Camera(pos, lookat, fov);
}

void Scene::AddEnvMap(std::string& filename)
{
	if (envMap)
		delete envMap;

	envMap = new EnvironmentMap;
	if (envMap->LoadEnvMap(filename))
		printf("HDR \"%s\" loaded\n", filename.c_str());
	else {
		printf("Unable to load \"%s\" HDR\n", filename.c_str());
		delete envMap;
		envMap = nullptr;
	}
}

int Scene::AddTexture(std::string & filename)
{
	// Check if texture was already loaded
	for (size_t i = 0; i < textures.size(); i++)
		if (textures[i]->name == filename)
			return (int)i;

	int id = (int)textures.size();
	Texture* tex = new Texture;

	printf("Loading texture \"%s\"\n", filename.c_str());
	if (tex->LoadTexture(filename))
		textures.push_back(tex);
	else {
		delete tex;
		id = -1;
		printf("Fail to load \"%s\" texture\n", filename.c_str());
	}

	return id;
}

int Scene::AddMesh(std::string & filename)
{
	// Check if mesh was already loaded
	for (size_t i = 0; i < meshes.size(); i++)
		if (meshes[i]->name == filename)
			return (int)i;

	int id = (int)meshes.size();
	Mesh* mesh = new Mesh;

	printf("Loading mesh \"%s\"\n", filename.c_str());
	if (mesh->LoadMesh(filename))
		meshes.push_back(mesh);
	else {
		delete mesh;
		id = -1;
		printf("Fail to load \"%s\" mesh\n", filename.c_str());
	}

	return id;
}

int Scene::AddMeshInstance(MeshInstance * meshInstance)
{
	int id = (int)meshInstances.size();
	meshInstances.push_back(meshInstance);
	return id;
}

int Scene::AddMaterial(const Material& mat)
{
	int id = (int)materials.size();
	materials.push_back(mat);
	return id;
}

int Scene::AddLight(const Light& light)
{
	int id = (int)lights.size();
	lights.push_back(light);
	return id;
}


// --------------------------------[BVH RELEVANT FUNCTION ]--------------------------------
// --------------------------------[CREATE PROCESS REBUILD]--------------------------------

void Scene::CreateTLAS()
{
	// 遍历所有instance，构建TLAS-BVH
	std::vector<bbox3f> bounds(meshInstances.size());
	// pbrt-v3 exercise 2-1: 快速变化AABB包围盒
	for (size_t i = 0; i < meshInstances.size(); i++)
	{
		mat4 matrix = meshInstances[i]->transform;
		bbox3f bound = meshes[meshInstances[i]->meshID]->blasBVH->WorldBound();
		vec3f pmin = bound.pMin;
		vec3f pmax = bound.pMax;

		vec3f right = vec3f(matrix[0][0], matrix[0][1], matrix[0][2]);
		vec3f up	= vec3f(matrix[1][0], matrix[1][1], matrix[1][2]);
		vec3f forward = vec3f(matrix[2][0], matrix[2][1], matrix[2][2]);
		vec3f translation = vec3f(matrix[3][0], matrix[3][1], matrix[3][2]);

		vec3f xa = right * pmin.x;
		vec3f xb = right * pmax.x;
		vec3f ya = up * pmin.y;
		vec3f yb = up * pmax.y;
		vec3f za = forward * pmin.z;
		vec3f zb = forward * pmax.z;

		pmin = Min(xa, xb) + Min(ya, yb) + Min(za, zb) + translation;
		pmax = Max(xa, xb) + Max(ya, yb) + Max(za, zb) + translation;

		bounds[i] = bbox3f(pmin, pmax);
	}
	printf("Building TLAS-BVH For Scene...\n");
	tlasBVH = new BVHAccel(bounds, 1, BVHAccel::SplitMethod::SAH, 12, 1.0f);
}

void Scene::CreateBLAS()
{
	// 遍历每个mesh，为每个mesh构建BLAS-BVH
#pragma omp parallel for
	for (size_t i = 0; i < meshes.size(); i++)
	{
		printf("Building BLAS-BVH For Mesh \"%s\"...\n", meshes[i]->name.c_str());
		meshes[i]->BuildBVH();
	}
}


void Scene::ProcessBLAS()
{
	printf("Copying blasBVHNodes to the scene, Adding offset for these blasBVHNodes in sceneNodes...\n");
	// mesh的blasBVH的节点在sceneNodes中的额外偏移
	uint32_t blasBVHRootOffset = 0;
	// mesh的blasBVH的叶子存储的图元索引在scenePrimsVertexIndices中的额外偏移
	uint32_t blasBVHPrimsOffset = 0;
#pragma omp parallel for
	for (size_t i = 0; i < meshes.size(); i++)
	{
		std::vector<LinearBVHNode>& blasBVHNodes = meshes[i]->blasBVH->nodes;

		// 拷贝mesh的BVHNodes到scene中
		sceneNodes.insert(sceneNodes.end(), blasBVHNodes.begin(), blasBVHNodes.end());

		// 修正node中的部分参数，以适应sceneNodes的存储方式
#pragma omp parallel for
		for (size_t j = 0; j < meshes[i]->blasBVH->nodeCounts; j++)
		{
			// 根据nPrimitives判断是leaf，还是interior
			if (blasBVHNodes[j].nPrimitives)
				// 修正叶子节点存储的第一个图元的偏移
				sceneNodes[blasBVHRootOffset + j].primitivesOffset += blasBVHPrimsOffset;
			else
				// 修正中间节点存储的右子树的偏移
				sceneNodes[blasBVHRootOffset + j].secondChildOffset += blasBVHRootOffset;
		}

		// 更新步长
		blasBVHStartOffsets.push_back(blasBVHRootOffset);
		blasBVHRootOffset += (uint32_t)meshes[i]->blasBVH->nodes.size();	// nodeCounts == nodes.size()
		blasBVHPrimsOffset += (uint32_t)meshes[i]->blasBVH->orderedPrimsIndices.size();
	}

	// 修正完node后有: sceneNodes.size() == blasBVHRootOffset，也是tlasBVH开始的偏移
	tlasBVHStartOffset = blasBVHRootOffset;
	// 提前分配将各个mesh的primIdx展开为vertexIdx，并存储在统一数组中所需的空间
	scenePrimsVertexIndices.resize(blasBVHPrimsOffset);
}

void Scene::ProcessTLAS()
{
	printf("Copying tlasBVHNodes to the scene, Updating information for these tlasBVHNodes in sceneNodes...\n");
	// 拷贝tlasBVH的BVHNodes到scene中
	sceneNodes.insert(sceneNodes.end(), tlasBVH->nodes.begin(), tlasBVH->nodes.end());
#pragma omp parallel for
	for (size_t i = 0; i < tlasBVH->nodeCounts; i++)
	{
		// 根据nPrimitives判断是leaf，还是interior
		if (tlasBVH->nodes[i].nPrimitives)
		{
			uint32_t instanceIdx = tlasBVH->orderedPrimsIndices[tlasBVH->nodes[i].primitivesOffset];
			uint32_t meshID = meshInstances[instanceIdx]->meshID;

			// 记录tlasBVH的叶子节点存储的blasBVH在sceneNodes中的偏移，即起始位置
			sceneNodes[tlasBVHStartOffset + i].blasBVHStartOffset = blasBVHStartOffsets[meshID];
			// 记录tlasBVH的叶子节点存储的blasBVH使用的materialID
			sceneNodes[tlasBVHStartOffset + i].materialID = meshInstances[instanceIdx]->materialID;
		}
		else
			// 修正中间节点存储的右子树的偏移
			sceneNodes[tlasBVHStartOffset + i].secondChildOffset += tlasBVHStartOffset;
	}
}

void Scene::RebuildTLAS()
{
	// delete original tlas
	delete tlasBVH;
	// rebuild tlas
	CreateTLAS();

	// cleanup original tlasNodes in sceneNodes
	sceneNodes.resize(tlasBVHStartOffset);
	// reprocess tlas
	ProcessTLAS();

	// Copy transforms
	transforms.resize(0);
	transforms.resize(meshInstances.size());
	for (size_t i = 0; i < meshInstances.size(); i++)
		transforms[i] = meshInstances[i]->transform;

	instancesModified = true;
	dirty = true;
}


void Scene::ProcessScene()
{
	printf("----------[BUILDING BLAS-BVH FOR EVERY MESH]---------\n");
	CreateBLAS();

	printf("----------[BUILDING TLAS-BVH FOR WHOLE SCENE]--------\n");
	CreateTLAS();

	printf("----------[INTEGRATE BLAS-BVH AND TLAS-BVH]----------\n");
	ProcessBLAS();
	ProcessTLAS();

	printf("----------[COPYING MESH DATA TO THE SCENE]-----------\n");
	printf("Copying mesh data to the scene, Expand the primIndex to vertexIndex...\n");
	// mesh的blasBVH的叶子存储的图元索引对应的三个顶点索引在scene.verticesUVX、scene.normalsUVY中的额外偏移
	uint32_t blasBVHVerticesOffset = 0;
	size_t counter = 0;	// 计数用
	for (size_t i = 0; i < meshes.size(); i++)
	{
		std::vector<uint32_t>& blasBVHPrimsIndices = meshes[i]->blasBVH->orderedPrimsIndices;
		size_t blasBVHPrimsIndicesNum = blasBVHPrimsIndices.size();

		// 修正顶点索引，以作为sceneBVH叶子节点中存储的图元和verticesUVX、normalsUVY之间的桥梁
		for (size_t j = 0; j < blasBVHPrimsIndicesNum; j++)
		{
			// scenePrimsVertexIndices.size() = sum{meshes[x].blasBVH.orderedPrimsIndices.size() | x:[0,n)}
			// 将mesh的orderedPrimsIndices展开为顶点索引。因为prim是三角形，所以一个orderedIdx对应三个顶点索引
			scenePrimsVertexIndices[counter++] = vec3i{  (blasBVHPrimsIndices[j] * 3 + 0) + blasBVHVerticesOffset,
														 (blasBVHPrimsIndices[j] * 3 + 1) + blasBVHVerticesOffset,
														 (blasBVHPrimsIndices[j] * 3 + 2) + blasBVHVerticesOffset };
		}

		// 更新步长
		// meshes[i]->verticesUVX.size() == 3 * meshes[i]->blasBVH->orderedPrimsIndices.size()
		blasBVHVerticesOffset += meshes[i]->verticesUVX.size();

		// 拷贝mesh的顶点、法线、uv等数据到scene中
		verticesUVX.insert(verticesUVX.end(), meshes[i]->verticesUVX.begin(), meshes[i]->verticesUVX.end());
		normalsUVY.insert(normalsUVY.end(), meshes[i]->normalsUVY.begin(), meshes[i]->normalsUVY.end());
	}

	printf("----------[COPYING TRANSFORM TO THE SCENE]-----------\n");
	printf("Copying meshInstance transform to the scene...\n");
	transforms.resize(meshInstances.size());
	for (size_t i = 0; i < meshInstances.size(); i++)
		transforms[i] = meshInstances[i]->transform;

	if (!textures.empty())
	{
		printf("----------[COPYING TEXTURES TO THE SCENE]------------\n");
		printf("Copying and resizing textures...\n");
		int reqWidth = renderOptions->texArrayWidth;
		int reqHeight = renderOptions->texArrayHeight;
		size_t texBytes = reqWidth * reqHeight * 4;
		textureMapsArray.resize(texBytes * textures.size());

#pragma omp parallel for
		for (size_t i = 0; i < textures.size(); i++)
		{
			if (textures[i]->width != reqWidth || textures[i]->height != reqHeight)
			{
				unsigned char * resizedTex = new unsigned char[texBytes];
				stbir_resize_uint8(&textures[i]->texData[0], textures[i]->width, textures[i]->height, 0,
									resizedTex, reqWidth, reqHeight, 0, 4);
				std::copy(resizedTex, resizedTex + texBytes, &textureMapsArray[i * texBytes]);
				delete[] resizedTex;
			}
			else
				std::copy(textures[i]->texData.begin(), textures[i]->texData.end(), &textureMapsArray[i * texBytes]);
		}
	}

	if (!camera)
	{
		bbox3f sceneBound = tlasBVH->WorldBound();
		vec3f diagonal = sceneBound.Diagonal();
		vec3f center = sceneBound.Center();
		AddCamera(vec3f(center.x, center.y, center.z + diagonal.Length()), center, 45.0f);
	}

	initialized = true;
}

NAMESPACE_END(nagi)