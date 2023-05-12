#pragma once

#include <vector>
#include "matrix.h"

NAMESPACE_BEGIN(nagi)

class Camera;
class EnvironmentMap;
class Texture;
class Mesh;
class MeshInstance;
class Material;
class Light;
class BVHAccel;
struct LinearBVHNode;

struct RenderOptions
{
	RenderOptions() {
		renderResolution = vec2i(600, 600);
		windowResolution = vec2i(1000, 600);
		tileResolution = vec2i(100, 100);
		maxSpp = 512;
		maxDepth = 3;
		RRDepth = 2;
		texArrayWidth = 2048;
		texArrayHeight = 2048;
		denoiserFrameCnt = 20;
		enableRR = true;
		enableDenoiser = false;
		enableTonemap = true;
		enableAces = false;
		enableSimpleAcesFit = false;
		enableOpenglNormalMap = true;
		enableUniformLight = false;
		enableHideEmitters = false;
		enableBackground = false;
		enableTransparentBackground = false;
		enableIndependentRenderSize = false;
		enableRoughnessMollification = false;
		enableVolumeMIS = false;
		enableEnvMap = false;
		envMapIntensity = 1.0f;
		envMapRot = 0.0f;
	}
	vec2i renderResolution;
	vec2i windowResolution;
	vec2i tileResolution;
	int maxSpp;
	int maxDepth;
	int RRDepth;
	int texArrayWidth;
	int texArrayHeight;
	int denoiserFrameCnt;
	bool enableRR;
	bool enableDenoiser;
	bool enableTonemap;
	bool enableAces;
	bool enableSimpleAcesFit;
	bool enableOpenglNormalMap;
	bool enableUniformLight;
	bool enableHideEmitters;
	bool enableBackground;
	bool enableTransparentBackground;
	bool enableIndependentRenderSize;
	bool enableRoughnessMollification;
	bool enableVolumeMIS;
	bool enableEnvMap;
	float envMapIntensity;
	float envMapRot;
};

class Scene
{
public:
	Scene();
	~Scene();

	void AddCamera(vec3f pos, vec3f lookat, float fov);
	void AddEnvMap(std::string& filename);
	int AddTexture(std::string& filename);
	int AddMesh(std::string& filename);
	int AddMeshInstance(MeshInstance* meshInstance);
	int AddMaterial(const Material& mat);
	int AddLight(const Light& light);

	void RebuildTLAS();
	void ProcessScene();

private:
	void CreateTLAS();
	void CreateBLAS();
	void ProcessBLAS();
	void ProcessTLAS();

public:
	// TLAS, leaf is BLAS
	BVHAccel* tlasBVH;
	// 将TLAS与BLAS整合后的全部nodes
	std::vector<LinearBVHNode> sceneNodes;
	// TLAS开始的索引
	uint32_t tlasBVHStartOffset;
	// 各个mesh的blasBVH在nodes中的开始索引
	std::vector<uint32_t> blasBVHStartOffsets;

	// RenderOptions is responsible for indicate render options.
	RenderOptions* renderOptions;

	// only one camera for generate rays.
	Camera* camera;

	// an EnvironmentMap for Image-based Lighting.
	EnvironmentMap* envMap;

	// vector存储指针的好处是避免在执行push等操作时复制元素，尤其是对象数据量很大时。
	// 但与此相对就要在原始数据的基础上多存储一个指针，以空间换时间。
	// meshes in the scene
	std::vector<Mesh*> meshes;
	// a MeshInstance holds a meshId and relevant transform, also includes a materialId.
	// this seperation which reduces the actual number of triangle mesh in the scene can really save memory usage.
	std::vector<MeshInstance*> meshInstances;

	// materials和lights可以存储指针，但是这些复数数据要利用glTexImage2D发送到gpu上
	// 如果存储指针，没办法将复数对象的数据直接赋给glTexImage2D的data形参
	// store all material
	std::vector<Material> materials;
	// store all light
	std::vector<Light> lights;

	// Texture Data
	std::vector<Texture*> textures;
	std::vector<unsigned char> textureMapsArray;

	// scene data for all obj
	std::vector<vec3i> scenePrimsVertexIndices;
	std::vector<vec4f> verticesUVX;// Vertex + texture Coord (u/s)
	std::vector<vec4f> normalsUVY;  // Normal + texture Coord (v/t)
	std::vector<mat4> transforms;

	// there are four varible control render state.
	// Is scene has been initialized?
	bool initialized;
	// Is scene has to rendered from scratch?
	bool dirty;
	// Is meshInstances have been modified?
	bool instancesModified;
	// Is envMap has been modified?
	bool envMapModified;
};

NAMESPACE_END(nagi)
