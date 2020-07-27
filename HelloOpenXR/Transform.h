#pragma once
#include "glm/glm.hpp"
//#include "glm/vec4.hpp"
#include "glm/gtc/quaternion.hpp"
class Transform
{
	glm::vec3 position;
	glm::quat rotation;
	glm::vec3 scale;
};

