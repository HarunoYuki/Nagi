#include <iostream>
#include "logger.h"

NAMESPACE_BEGIN(nagi)

class Scene;
class RenderOptions;

class Parser
{
public:
	Parser() {}
	~Parser() {}

	bool ParseFromSceneFile(std::string sceneFile, Scene* scene, RenderOptions& renderOptions);
};

NAMESPACE_END(nagi)