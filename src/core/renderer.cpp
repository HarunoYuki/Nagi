#include "renderer.h"
#include "quad.h"
#include "program.h"

NAMESPACE_BEGIN(nagi)

Renderer::Renderer(Scene * scene, const std::string & shadersDir) : shadersDir(shadersDir),
	// input
	BVHBuffer(0), BVHTex(0), vertexIndicesBuffer(0), vertexIndicesTex(0), 
	verticesBuffer(0), verticesTex(0), normalsBuffer(0), normalsTex(0), 
	transformTex(0), lightsTex(0), materialsTex(0), texturesMapArrayTex(0),
	envMapTex(0), envMapCDFTex(0),
	// calculate
	pathTraceShader(nullptr), pathTraceShaderLowRes(nullptr),  tonemapShader(nullptr), outputShader(nullptr),
	// output
	pathTraceFBO(0), pathTraceTex(0), pathTraceFBOLowRes(0), pathTraceTexLowRes(0), 
	accumFBO(0), accumTex(0), outputFBO(0), outputTex(), denoisedTex(0)
{
	if (!scene) {
		printf("Scene is empty!\n");
		return;
	}

	quad = new Quad;
	InitGPUDataBuffers();
	InitFBOs();
	InitShaders();

	initialized = true;
}

Renderer::~Renderer()
{
	delete quad;
}

void Renderer::InitGPUDataBuffers()
{
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	glGenBuffers(1, &BVHBuffer);

}

void Renderer::InitFBOs()
{
}

void Renderer::InitShaders()
{
}



NAMESPACE_END(nagi)