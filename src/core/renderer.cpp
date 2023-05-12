#include "renderer.h"
#include "quad.h"
#include "program.h"
#include "scene.h"
#include "light.h"
#include "material.h"
#include "environmentMap.h"
#include "shader.h"
#include "bvh.h"

NAMESPACE_BEGIN(nagi)

Program* LoadShaders(ShaderSource vert, ShaderSource frag)
{
	std::vector<Shader> shaders;
	shaders.push_back(Shader(vert, GL_VERTEX_SHADER));
	shaders.push_back(Shader(frag, GL_FRAGMENT_SHADER));
	return new Program(shaders);
}

Renderer::Renderer(Scene * scene, const std::string & shadersDir) 
	: scene(scene), shadersDir(shadersDir), quad(new Quad),
	// input
	BVHBuffer(0), BVHTex(0), vertexIndicesBuffer(0), vertexIndicesTex(0), 
	verticesBuffer(0), verticesTex(0), normalsBuffer(0), normalsTex(0), 
	transformsTex(0), lightsTex(0), materialsTex(0), textureMapsArrayTex(0),
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

	if (!scene->initialized)
		scene->ProcessScene();

	InitGPUDataBuffers();
	InitFBOs();
	InitShaders();

	initialized = true;
}

Renderer::~Renderer()
{
	delete scene;
	delete quad;

	// delete inupt data
	glDeleteBuffers(1,&BVHBuffer); glDeleteTextures(1, &BVHTex);
	glDeleteBuffers(1,&vertexIndicesBuffer); glDeleteTextures(1, &vertexIndicesTex);
	glDeleteBuffers(1,&verticesBuffer); glDeleteTextures(1, &verticesTex);
	glDeleteBuffers(1,&normalsBuffer); glDeleteTextures(1, &normalsTex);
	glDeleteTextures(1, &transformsTex); glDeleteTextures(1, &lightsTex);
	glDeleteTextures(1, &materialsTex); glDeleteTextures(1, &textureMapsArrayTex);
	glDeleteTextures(1, &envMapTex); glDeleteTextures(1, &envMapCDFTex);

	// delete calculate shader
	delete pathTraceShader; delete pathTraceShaderLowRes;
	delete tonemapShader; delete outputShader;

	// delete output fbo and color attachment
	glDeleteFramebuffers(1,&pathTraceFBO); glDeleteTextures(1, &pathTraceTex);
	glDeleteFramebuffers(1,&pathTraceFBOLowRes); glDeleteTextures(1, &pathTraceTexLowRes);
	glDeleteFramebuffers(1,&accumFBO); glDeleteTextures(1, &accumTex);
	glDeleteFramebuffers(1, &outputFBO); glDeleteTextures(1, &outputTex[0]);
	glDeleteTextures(1, &outputTex[1]); glDeleteTextures(1, &denoisedTex);

	// Delete denoiser data
	delete[] denoiserInputFramePtr; delete[] frameOutputPtr;
}

void Renderer::InitGPUDataBuffers()
{
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	// Create buffer and texture for BVH
	glGenBuffers(1, &BVHBuffer);
	glBindBuffer(GL_TEXTURE_BUFFER, BVHBuffer);
	glBufferData(GL_TEXTURE_BUFFER, sizeof(LinearBVHNode)*scene->sceneNodes.size(), scene->sceneNodes.data(), GL_STATIC_DRAW);
	glGenTextures(1, &BVHTex);
	glBindTexture(GL_TEXTURE_BUFFER, BVHTex);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, BVHBuffer);

	// Create buffer and texture for vertex indices
	glGenBuffers(1, &vertexIndicesBuffer);
	glBindBuffer(GL_TEXTURE_BUFFER, vertexIndicesBuffer);
	glBufferData(GL_TEXTURE_BUFFER, sizeof(vec3i)*scene->scenePrimsVertexIndices.size(), scene->scenePrimsVertexIndices.data(), GL_STATIC_DRAW);
	glGenTextures(GL_TEXTURE_BUFFER, &vertexIndicesTex);
	glBindTexture(GL_TEXTURE_BUFFER, vertexIndicesTex);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32I, vertexIndicesBuffer);

	// Create buffer and texture for vertices
	glGenBuffers(1, &verticesBuffer);
	glBindBuffer(GL_TEXTURE_BUFFER, verticesBuffer);
	glBufferData(GL_TEXTURE_BUFFER, sizeof(vec4f)*scene->verticesUVX.size(), scene->verticesUVX.data(), GL_STATIC_DRAW);
	glGenTextures(GL_TEXTURE_BUFFER, &verticesTex);
	glBindTexture(GL_TEXTURE_BUFFER, verticesTex);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, verticesBuffer);

	// Create buffer and texture for normals
	glGenBuffers(1, &normalsBuffer);
	glBindBuffer(GL_TEXTURE_BUFFER, normalsBuffer);
	glBufferData(GL_TEXTURE_BUFFER, sizeof(vec4f)*scene->normalsUVY.size(), scene->normalsUVY.data(), GL_STATIC_DRAW);
	glGenTextures(GL_TEXTURE_BUFFER, &normalsTex);
	glBindTexture(GL_TEXTURE_BUFFER, normalsTex);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, normalsBuffer);

	// Create texture for transforms
	glGenTextures(1, &transformsTex);
	glBindTexture(GL_TEXTURE_2D, transformsTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, (sizeof(mat4) / sizeof(vec4f))*scene->transforms.size(), 1, 0, GL_RGBA, GL_FLOAT, scene->transforms.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	if (!scene->lights.empty())
	{
		// Create texture for lights
		glGenTextures(1, &lightsTex);
		glBindTexture(GL_TEXTURE_2D, lightsTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, (sizeof(Light) / sizeof(vec3f))*scene->lights.size(), 1, 0, GL_RGB, GL_FLOAT, scene->lights.data());
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	// Create texture for lights
	glGenTextures(1, &materialsTex);
	glBindTexture(GL_TEXTURE_2D, materialsTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, (sizeof(Material) / sizeof(vec4f))*scene->materials.size(), 1, 0, GL_RGBA, GL_FLOAT, scene->materials.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	// Create texture for scene textures
	if (!scene->textures.empty())
	{
		glGenTextures(1, &textureMapsArrayTex);
		glBindTexture(GL_TEXTURE_2D_ARRAY, textureMapsArrayTex);
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, scene->renderOptions->texArrayWidth, scene->renderOptions->texArrayHeight, scene->textures.size(), 0, GL_RGBA, GL_UNSIGNED_BYTE, &scene->textureMapsArray[0]);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	}

	if (scene->envMap)
	{
		glGenTextures(1, &envMapTex);
		glBindTexture(GL_TEXTURE_2D, envMapTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, scene->envMap->width, scene->envMap->height, 0, GL_RGB, GL_FLOAT, scene->envMap->img);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);

		glGenTextures(1, &envMapCDFTex);
		glBindTexture(GL_TEXTURE_2D, envMapCDFTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, scene->envMap->width, scene->envMap->height, 0, GL_RED, GL_FLOAT, scene->envMap->cdf);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	// Bind textures to texture slots as they will not change slots during the lifespan of the renderer
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_BUFFER, BVHTex);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_BUFFER, vertexIndicesTex);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_BUFFER, verticesTex);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_BUFFER, normalsTex);
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D, materialsTex);
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, transformsTex);
	glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_2D, lightsTex);
	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_2D_ARRAY, textureMapsArrayTex);
	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D, envMapTex);
	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_2D, envMapCDFTex);
}

void Renderer::ResizeRenderer()
{
	// 删除着色器，重新载入
	delete pathTraceShader; delete pathTraceShaderLowRes;
	delete tonemapShader; delete outputShader;

	// 输出改变，所以删除原先的输出缓冲区及颜色附件
	glDeleteFramebuffers(1, &pathTraceFBO); glDeleteTextures(1, &pathTraceTex);
	glDeleteFramebuffers(1, &pathTraceFBOLowRes); glDeleteTextures(1, &pathTraceTexLowRes);
	glDeleteFramebuffers(1, &accumFBO); glDeleteTextures(1, &accumTex);
	glDeleteFramebuffers(1, &outputFBO); glDeleteTextures(1, &outputTex[0]);
	glDeleteTextures(1, &outputTex[1]); glDeleteTextures(1, &denoisedTex);

	// Delete denoiser data
	delete[] denoiserInputFramePtr; delete[] frameOutputPtr;

	// 重新载入
	InitFBOs();
	InitShaders();
}

void Renderer::InitFBOs()
{
	frameCounter = 1;
	sampleCounter = 1;
	curFrameBuffer = 0;
	pixelRatio = 0.25f;

	renderRes = scene->renderOptions->renderResolution;
	windowRes = scene->renderOptions->windowResolution;
	tileRes = scene->renderOptions->tileResolution;

	tilesNum.x = ceilf((float)renderRes.x / tileRes.x);
	tilesNum.y = ceilf((float)renderRes.y / tileRes.y);

	invTilesNum.x = (float)tileRes.x / renderRes.x;
	invTilesNum.y = (float)tileRes.y / renderRes.y;

	tileIdx = vec2i(-1, tilesNum.y - 1);


	// Create frame buffer for pathTrace
	glGenFramebuffers(1, &pathTraceFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, pathTraceFBO);
	// Create color attachment for pathTrace frame buffer
	glGenTextures(1, &pathTraceTex);
	glBindTexture(GL_TEXTURE_2D, pathTraceTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, tileRes.x, tileRes.y, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pathTraceTex, 0);


	// Create frame buffer for pathTrace preview
	glGenFramebuffers(1, &pathTraceFBOLowRes);
	glBindFramebuffer(GL_FRAMEBUFFER, pathTraceFBOLowRes);
	// Create color attachment for pathTrace preview frame buffer
	glGenTextures(1, &pathTraceTexLowRes);
	glBindTexture(GL_TEXTURE_2D, pathTraceTexLowRes);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, renderRes.x*pixelRatio, renderRes.y*pixelRatio, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pathTraceTexLowRes, 0);


	// Create frame buffer for accumlate
	glGenFramebuffers(1, &accumFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, accumFBO);
	// Create color attachment for accumlate frame buffer
	glGenTextures(1, &accumTex);
	glBindTexture(GL_TEXTURE_2D, accumTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, renderRes.x, renderRes.y, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, accumTex, 0);


	// Create frame buffer for output
	glGenFramebuffers(1, &outputFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, outputFBO);
	// Create first color attachment(double buffer to present) for output frame buffer
	glGenTextures(1, &outputTex[0]);
	glBindTexture(GL_TEXTURE_2D, outputTex[0]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, renderRes.x, renderRes.y, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	// Create second color attachment(double buffer to present) for output frame buffer
	glGenTextures(1, &outputTex[1]);
	glBindTexture(GL_TEXTURE_2D, outputTex[1]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, renderRes.x, renderRes.y, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTex[curFrameBuffer], 0);

	// For Denoiser
	denoiserInputFramePtr = new vec3f[renderRes.x * renderRes.y];
	frameOutputPtr = new vec3f[renderRes.x * renderRes.y];

	glGenTextures(1, &denoisedTex);
	glBindTexture(GL_TEXTURE_2D, denoisedTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, renderRes.x, renderRes.y, 0, GL_RGB, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	printf("Window Resolution : %d %d\n", windowRes.x, windowRes.y);
	printf("Render Resolution : %d %d\n", renderRes.x, renderRes.y);
	printf("Preview Resolution : %d %d\n", (int)((float)renderRes.x * pixelRatio), (int)((float)renderRes.y * pixelRatio));
	printf("Tile Size : %d %d\n", tileRes.x, tileRes.y);
}

void Renderer::ReloadShaders()
{
	// 删除着色器，重新载入
	delete pathTraceShader; delete pathTraceShaderLowRes;
	delete tonemapShader; delete outputShader;

	InitShaders();
}

void Renderer::InitShaders()
{
	ShaderSource vertexShaderSrcObj = Shader::LoadFullShaderCode(shadersDir + "common/vertex.vert");
	ShaderSource pathTraceShaderSrcObj = Shader::LoadFullShaderCode(shadersDir + "tile.frag");
	ShaderSource pathTraceShaderLowResSrcObj = Shader::LoadFullShaderCode(shadersDir + "preview.frag");
	ShaderSource tonemapShaderSrcObj = Shader::LoadFullShaderCode(shadersDir + "tonemap.frag");
	ShaderSource outputShaderSrcObj = Shader::LoadFullShaderCode(shadersDir + "output.frag");

	// Add preprocessor defines for conditional compilation
	std::string pathtraceDefines = "";
	std::string tonemapDefines = "";

	if (scene->renderOptions->enableEnvMap && scene->envMap != nullptr)
		pathtraceDefines += "#define NAGI_ENVMAP\n";

	if (!scene->lights.empty())
		pathtraceDefines += "#define NAGI_LIGHTS\n";

	if (scene->renderOptions->enableRR)
	{
		pathtraceDefines += "#define NAGI_RR\n";
		pathtraceDefines += "#define NAGI_RR_DEPTH " + std::to_string(scene->renderOptions->RRDepth) + "\n";
	}

	if (scene->renderOptions->enableUniformLight)
		pathtraceDefines += "#define NAGI_UNIFORM_LIGHT\n";

	if (scene->renderOptions->enableOpenglNormalMap)
		pathtraceDefines += "#define NAGI_OPENGL_NORMALMAP\n";

	if (scene->renderOptions->enableHideEmitters)
		pathtraceDefines += "#define NAGI_HIDE_EMITTERS\n";

	if (scene->renderOptions->enableBackground)
	{
		pathtraceDefines += "#define NAGI_BACKGROUND\n";
		tonemapDefines += "#define NAGI_BACKGROUND\n";
	}

	if (scene->renderOptions->enableTransparentBackground)
	{
		pathtraceDefines += "#define NAGI_TRANSPARENT_BACKGROUND\n";
		tonemapDefines += "#define NAGI_TRANSPARENT_BACKGROUND\n";
	}

	for (int i = 0; i < scene->materials.size(); i++)
	{
		if ((int)scene->materials[i].alphaMode != Material::AlphaMode::Opaque)
		{
			pathtraceDefines += "#define NAGI_ALPHA_TEST\n";
			break;
		}
	}

	if (scene->renderOptions->enableRoughnessMollification)
		pathtraceDefines += "#define NAGI_ROUGHNESS_MOLLIFICATION\n";

	for (int i = 0; i < scene->materials.size(); i++)
	{
		if ((int)scene->materials[i].mediumType != Material::MediumType::None)
		{
			pathtraceDefines += "#define NAGI_MEDIUM\n";
			break;
		}
	}

	if (scene->renderOptions->enableVolumeMIS)
		pathtraceDefines += "#define NAGI_VOL_MIS\n";

	if (pathtraceDefines.size() > 0)
	{
		size_t idx = pathTraceShaderSrcObj.src.find("#version");
		if (idx != -1)
			idx = pathTraceShaderSrcObj.src.find("\n", idx);
		else
			idx = 0;
		pathTraceShaderSrcObj.src.insert(idx + 1, pathtraceDefines);

		idx = pathTraceShaderLowResSrcObj.src.find("#version");
		if (idx != -1)
			idx = pathTraceShaderLowResSrcObj.src.find("\n", idx);
		else
			idx = 0;
		pathTraceShaderLowResSrcObj.src.insert(idx + 1, pathtraceDefines);
	}

	if (tonemapDefines.size() > 0)
	{
		size_t idx = tonemapShaderSrcObj.src.find("#version");
		if (idx != -1)
			idx = tonemapShaderSrcObj.src.find("\n", idx);
		else
			idx = 0;
		tonemapShaderSrcObj.src.insert(idx + 1, tonemapDefines);
	}

	// 加载shader，link为program
	pathTraceShader = LoadShaders(vertexShaderSrcObj, pathTraceShaderSrcObj);
	pathTraceShaderLowRes = LoadShaders(vertexShaderSrcObj, pathTraceShaderLowResSrcObj);
	outputShader = LoadShaders(vertexShaderSrcObj, outputShaderSrcObj);
	tonemapShader = LoadShaders(vertexShaderSrcObj, tonemapShaderSrcObj);

	// 设置pathTraceShader的uniform
	pathTraceShader->use();
	if (scene->envMap) {
		pathTraceShader->setVec2("envMapRes", scene->envMap->width, scene->envMap->height);
		pathTraceShader->setFloat("envMapTotalSum", scene->envMap->totalSum);
	}
	pathTraceShader->setVec2("resolution", (float)renderRes.x, (float)renderRes.y);
	pathTraceShader->setVec2("invTilesNum", invTilesNum);
	pathTraceShader->setInt("lightsNum", (int)scene->lights.size());
	pathTraceShader->setInt("tlasBVHStartOffset", (int)scene->tlasBVHStartOffset);
	pathTraceShader->setInt("accumTex", 0);
	pathTraceShader->setInt("BVHTex", 1);
	pathTraceShader->setInt("vertexIndicesTex", 2);
	pathTraceShader->setInt("verticesTex", 3);
	pathTraceShader->setInt("normalsTex", 4);
	pathTraceShader->setInt("materialsTex", 5);
	pathTraceShader->setInt("transformsTex", 6);
	pathTraceShader->setInt("lightsTex", 7);
	pathTraceShader->setInt("textureMapsArrayTex", 8);
	pathTraceShader->setInt("envMapTex", 9);
	pathTraceShader->setInt("envMapCDFTex", 10);
	pathTraceShader->stop();

	// 设置pathTraceShaderLowRes的uniform
	pathTraceShaderLowRes->use();
	if (scene->envMap) {
		pathTraceShaderLowRes->setVec2("envMapRes", scene->envMap->width, scene->envMap->height);
		pathTraceShaderLowRes->setFloat("envMapTotalSum", scene->envMap->totalSum);
	}
	pathTraceShaderLowRes->setVec2("resolution", (float)renderRes.x, (float)renderRes.y);
	pathTraceShaderLowRes->setInt("lightsNum", (int)scene->lights.size());
	pathTraceShaderLowRes->setInt("tlasBVHStartOffset", (int)scene->tlasBVHStartOffset);
	pathTraceShaderLowRes->setInt("accumTex", 0);
	pathTraceShaderLowRes->setInt("BVHTex", 1);
	pathTraceShaderLowRes->setInt("vertexIndicesTex", 2);
	pathTraceShaderLowRes->setInt("verticesTex", 3);
	pathTraceShaderLowRes->setInt("normalsTex", 4);
	pathTraceShaderLowRes->setInt("materialsTex", 5);
	pathTraceShaderLowRes->setInt("transformsTex", 6);
	pathTraceShaderLowRes->setInt("lightsTex", 7);
	pathTraceShaderLowRes->setInt("textureMapsArrayTex", 8);
	pathTraceShaderLowRes->setInt("envMapTex", 9);
	pathTraceShaderLowRes->setInt("envMapCDFTex", 10);
	pathTraceShaderLowRes->stop();
}

void Renderer::Render()
{
}

void Renderer::Present()
{
}

void Renderer::Update(float secondsElapsed)
{
}

NAMESPACE_END(nagi)