#include "scene.h"
#include "camera.h"
#include "environmentMap.h"
#include "texture.h"
#include "mesh.h"
#include "material.h"
#include "light.h"
#include "bvh.h"

NAMESPACE_BEGIN(nagi)


Scene::Scene() 
	:camera(nullptr), envMap(nullptr), renderOptions(new RenderOptions),
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

void Scene::CreateTLAS()
{
	// ��������instance������TLAS-BVH
	std::vector<bbox3f> bounds(meshInstances.size());
	// pbrt-v3 exercise 2-1: ���ٱ仯AABB��Χ��
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
	tlasBVH = new BVHAccel(bounds);
}

void Scene::CreateBLAS()
{
	// ����ÿ��mesh��Ϊÿ��mesh����BLAS-BVH
#pragma omp parallel for
	for (size_t i = 0; i < meshes.size(); i++)
	{
		printf("Building BLAS-BVH For Mesh \"%s\"...\n", meshes[i]->name.c_str());
		meshes[i]->BuildBVH();
	}
}

void Scene::Process()
{
	printf("--------[BUILDING BLAS-BVH FOR EVERY MESH]--------\n");
	CreateBLAS();

	printf("--------[BUILDING TLAS-BVH FOR WHOLE SCENE]--------\n");
	CreateTLAS();

	printf("--------[INTEGRATE BLAS-BVH AND TLAS-BVH]--------\n");
	// Add offset
	printf("Adding offset to the blasBVH...\n");
	// mesh��blasBVH�Ľڵ���sceneNodes�еĶ���ƫ��
	uint32_t blasBVHRootOffset = 0;
	// mesh��blasBVH��Ҷ�Ӵ洢��ͼԪ������scenePrimsVertexIndices�еĶ���ƫ��
	uint32_t blasBVHPrimsOffset = 0;
#pragma omp parallel for
	for (size_t i = 0; i < meshes.size(); i++)
	{
		std::vector<LinearBVHNode>& blasBVHNodes = meshes[i]->blasBVH->nodes;

		// ����mesh��BVHNodes��scene��
		sceneNodes.insert(sceneNodes.end(), blasBVHNodes.begin(), blasBVHNodes.end());

		// ����node�еĲ��ֲ���������ӦsceneNodes�Ĵ洢��ʽ
#pragma omp parallel for
		for (size_t j = 0; j < meshes[i]->blasBVH->nodeCounts; j++)
		{
			// ����nPrimitives�ж���leaf������interior
			if (blasBVHNodes[j].nPrimitives)
				// ����Ҷ�ӽڵ�洢�ĵ�һ��ͼԪ��ƫ��
				sceneNodes[blasBVHRootOffset + j].primitivesOffset += blasBVHPrimsOffset;
			else
				// �����м�ڵ�洢����������ƫ��
				sceneNodes[blasBVHRootOffset + j].secondChildOffset += blasBVHRootOffset;
		}

		// ���²���
		blasBVHStartOffsets.push_back(blasBVHRootOffset);
		blasBVHRootOffset += (uint32_t)meshes[i]->blasBVH->nodes.size();	// nodeCounts == nodes.size()
		blasBVHPrimsOffset += (uint32_t)meshes[i]->blasBVH->orderedPrimsIndices.size();
	}

	// After adding offset: sceneNodes.size() == blasBVHRootOffset
	tlasStartOffset = blasBVHRootOffset;

	// Add offset
	printf("Updating tlasBVH's information...\n");
	// ����tlasBVH��BVHNodes��scene��
	sceneNodes.insert(sceneNodes.end(), tlasBVH->nodes.begin(), tlasBVH->nodes.end());
#pragma omp parallel for
	for (size_t i = 0; i < tlasBVH->nodeCounts; i++)
	{
		// ����nPrimitives�ж���leaf������interior
		if (tlasBVH->nodes[i].nPrimitives)
		{
			uint32_t instanceIdx = tlasBVH->orderedPrimsIndices[tlasBVH->nodes[i].primitivesOffset];
			uint32_t meshID = meshInstances[instanceIdx]->meshID;

			// ��¼tlasBVH��Ҷ�ӽڵ�洢��blasBVH��sceneNodes�е�ƫ�ƣ�����ʼλ��
			sceneNodes[tlasStartOffset + i].blasBVHStartOffset = blasBVHStartOffsets[meshID];
			// ��¼tlasBVH��Ҷ�ӽڵ�洢��blasBVHʹ�õ�materialID
			sceneNodes[tlasStartOffset + i].materialID = meshInstances[instanceIdx]->materialID;
		}
		else
			// �����м�ڵ�洢����������ƫ��
			sceneNodes[tlasStartOffset + i].secondChildOffset += tlasStartOffset;
	}


	// Copy mesh data
	printf("Copying mesh data to scene..\n");
	// mesh��blasBVH��Ҷ�Ӵ洢��ͼԪ������Ӧ����������������verticesUVX��normalsUVY�еĶ���ƫ��
	uint32_t blasBVHVerticesOffset = 0;
	for (size_t i = 0; i < meshes.size(); i++)
	{
		std::vector<uint32_t>& blasBVHPrimsIndices = meshes[i]->blasBVH->orderedPrimsIndices;
		size_t blasBVHPrimsIndicesNum = blasBVHPrimsIndices.size();

		// ������������������ΪsceneBVHҶ�ӽڵ��д洢��ͼԪ��verticesUVX��normalsUVY֮�������
		for (size_t j = 0; j < blasBVHPrimsIndicesNum; j++)
		{
			// scenePrimsVertexIndices.size() = sum{meshes[x].blasBVH.orderedPrimsIndices.size() | x:[0,n)}
			// ��mesh��orderedPrimsIndicesչ��Ϊ������������Ϊprim�������Σ�����һ��orderedIdx��Ӧ������������
			scenePrimsVertexIndices.push_back(vec3i{ (blasBVHPrimsIndices[j] * 3 + 0) + blasBVHVerticesOffset,
													 (blasBVHPrimsIndices[j] * 3 + 1) + blasBVHVerticesOffset,
													 (blasBVHPrimsIndices[j] * 3 + 2) + blasBVHVerticesOffset });
		}

		// ���²���
		blasBVHVerticesOffset += meshes[i]->verticesUVX.size(); // meshes[i]->verticesUVX.size() == 3*blasBVHPrimsIndicesNum
		
		// ����mesh�Ķ��㡢���ߡ�uv�����ݵ�scene��
		verticesUVX.insert(verticesUVX.end(), meshes[i]->verticesUVX.begin(), meshes[i]->verticesUVX.end());
		normalsUVY.insert(normalsUVY.end(), meshes[i]->normalsUVY.begin(), meshes[i]->normalsUVY.end());
	}

	// Copy meshInstance transform
	printf("Copying meshInstance transform to scene..\n");
	for (size_t i = 0; i < meshInstances.size(); i++)
		transforms.push_back(meshInstances[i]->transform);
}

void Scene::ProcessBLAS()
{
}


NAMESPACE_END(nagi)