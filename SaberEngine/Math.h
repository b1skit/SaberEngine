#pragma once

/* 
*	SaberEngine math header.
*	For now, we rely on GLM and configure it here.
*/

//#define GLM_FORCE_MESSAGES // View compilation/configuration details
// GLM: GLM_FORCE_DEPTH_ZERO_TO_ONE is undefined.Using negative one to one depth clip space.
// GLM: GLM_FORCE_LEFT_HANDED is undefined.Using right handed coordinate system.

#define GLM_FORCE_SWIZZLE // Enable swizzle operators
#define GLM_ENABLE_EXPERIMENTAL // Recommended for common.hpp
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/common.hpp>	// fmod
#include <glm/gtx/matrix_decompose.hpp>