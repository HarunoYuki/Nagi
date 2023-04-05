#include <time.h>
#include <math.h>
#include <string>
#include <vector>

#include "glad.h"
#include "glfw3.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ImGuizmo.h"
#include "tinydir.h"

#include "scene.h"
#include "renderer.h"
#include "logger.h"
#include "parser.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

using namespace std;
using namespace nagi;

const std::string assetsDir = "../assets/";
const std::string envMapsDir = assetsDir + "HDR/";
const std::string shadersDir = "../src/shaders/";

Scene* scene = nullptr;
Renderer* renderer = nullptr;

std::vector<std::string> sceneFiles;
std::vector<std::string> envMaps;

uint16_t selectedSceneIdx = 0;
uint16_t selectedEnvMapIdx = 0;

void GetSceneFiles()
{
	tinydir_dir dir;
	tinydir_open_sorted(&dir, assetsDir.c_str());

	for (size_t i = 0; i < dir.n_files; i++)
	{
		tinydir_file file;
		tinydir_readfile_n(&dir, &file, i);

		if (file.extension == "scene")
		{
			sceneFiles.push_back(assetsDir + file.name);
		}
	}

	tinydir_close(&dir);
}

void GetEnvMaps()
{
	tinydir_dir dir;
	tinydir_open_sorted(&dir, envMapsDir.c_str());

	for (size_t i = 0; i < dir.n_files; i++)
	{
		tinydir_file file;
		tinydir_readfile_n(&dir, &file, i);

		if (file.extension == "hdr")
		{
			envMaps.push_back(envMapsDir + file.name);
		}
	}

	tinydir_close(&dir);
}

void CreateScene(std::string filename)
{
	delete scene;
	scene = new Scene();
	std::string ext = filename.substr(filename.find_last_of(".") + 1);

	bool success = false;
	if (ext == "scene")
		success = ParseFromSceneFile(filename, scene);

	if (!success)
		Error("Fail to load scene from %s file", filename.c_str());

	if (!scene->envMap && !envMaps.empty())
	{
		scene->AddEnvMap(envMaps[selectedEnvMapIdx]);
		scene->renderOptions->enableEnvMap = scene->lights.empty() ? true : false;
		scene->renderOptions->envMapIntensity = 1.5f;
	}
}

bool initRenderer()
{

}

void MainLoop(GLFWwindow* window)
{

}

int main(int argc, char** argv) {
	srand((uint32_t)time(nullptr));

	std::string sceneFilename;

	for (size_t i = 1; i < argc; i++)
	{
		const std::string arg(argv[i]);
		if (arg == "-s" || arg == "--scene")
		{
			sceneFilename = argv[++i];
		}
		else if (arg[0] == '-')
		{
			Error("Unknown Option %s", arg.c_str());
		}
	}

	if (sceneFilename.empty())
	{
		GetSceneFiles();
		GetEnvMaps();
		if (sceneFiles.empty())
			Error("There is no scene file available!");
		CreateScene(sceneFiles[selectedSceneIdx]);
	}
	else {
		GetEnvMaps();
		CreateScene(sceneFilename);
	}

	// init glfw and glad
	if (!glfwInit())
		Error("Fail to init glfw!");

	// Decide GL+GLSL versions. GL 3.0 + GLSL 130
	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only

	// Create window with graphics context
	GLFWwindow* window = glfwCreateWindow(scene->renderOptions->windowResolution.x, scene->renderOptions->windowResolution.y, "Nagi", NULL, NULL);
	if (window == NULL)
		Error("Fail to create glfw window!");
	glfwMakeContextCurrent(window);
	glfwSwapInterval(0); // disable vsync

	// GLAD是用来管理OpenGL的函数指针的，所以在调用任何OpenGL的函数之前我们需要初始化GLAD
	if (gladLoadGL() == 0)
		Error("Fail to init glad!");

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	if(!initRenderer())
		Error("Fail to init Renderer!");

	while (!glfwWindowShouldClose(window)) {
		MainLoop(window);
	}

	std::cout << "Render Done." << std::endl;
	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}