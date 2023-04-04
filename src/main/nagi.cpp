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

#include "parser.h"
#include "scene.h"
#include "renderer.h"
#include "logger.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

using namespace std;
using namespace nagi;

const std::string assetsDir = "../../assets/";
const std::string envMapsDir = assetsDir + "HDR/";
const std::string shadersDir = "../../src/shdaers/";

Scene* scene = nullptr;
Renderer* renderer = nullptr;

std::vector<std::string> sceneFiles;
std::vector<std::string> envMaps;

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


int main(int argc, char** argv) {
	srand((uint32_t)time(nullptr));

	std::string sceneFile;

	for (size_t i = 1; i < argc; i++)
	{
		const std::string arg(argv[i]);
		if (arg == "-s" || arg == "--scene")
		{
			sceneFile = argv[++i];
		}
		else if (arg[0] == '-')
		{
			Error("Unknown Option %s \n", arg.c_str());
		}
	}

	if (sceneFile.empty())
	{
		GetSceneFiles();
		GetEnvMaps();
		
	}
	else {

	}

	return 0;
}