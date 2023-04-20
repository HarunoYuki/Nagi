#pragma once
#include <string>
#include "vector.h"
#include "glad.h"

NAMESPACE_BEGIN(nagi)

class Scene;
class Quad;
class Program;

class Renderer
{
public:
	Renderer(Scene* scene, const std::string& shadersDir);
	~Renderer();

	void ResizeRenderer();
	void ReloadShaders();

	bool initialized = false;
private:
	void InitGPUDataBuffers();
	void InitFBOs();
	void InitShaders();


protected:
	Scene* scene;
	Quad* quad;

	// Input Data: Opengl buffer objects and textures for storing scene data on the GPU
	GLuint BVHBuffer;
	GLuint BVHTex;
	GLuint vertexIndicesBuffer;
	GLuint vertexIndicesTex;
	GLuint verticesBuffer;
	GLuint verticesTex;
	GLuint normalsBuffer;
	GLuint normalsTex;
	GLuint transformsTex;
	GLuint lightsTex;
	GLuint materialsTex;
	GLuint textureMapsArrayTex;
	GLuint envMapTex;
	GLuint envMapCDFTex;

	// Calculate: Shader Program
	std::string shadersDir;
	Program* pathTraceShader;
	Program* pathTraceShaderLowRes;
	Program* tonemapShader;
	Program* outputShader;

	// Output: FBOs and Color Attachment
	GLuint pathTraceFBO;
	GLuint pathTraceTex;
	GLuint pathTraceFBOLowRes;
	GLuint pathTraceTexLowRes;
	GLuint accumFBO;
	GLuint accumTex;
	GLuint outputFBO;
	GLuint outputTex[2];
	GLuint denoisedTex; // used for oidn

	// render state
	vec2i renderRes;
	vec2i windowRes;
	vec2i tileRes;
	vec2i tileIdx;
	vec2i tilesNum;
	vec2f invTilesNum;
	int curFrameBuffer;
	int frameCounter;
	int sampleCounter;
	float pixelRatio;

	// Denoiser output
	vec3f* denoiserInputFramePtr;
	vec3f* frameOutputPtr;
	bool denoised;
};

NAMESPACE_END(nagi)