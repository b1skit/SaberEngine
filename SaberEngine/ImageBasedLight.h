#pragma once

#include <string>

#include "Light.h"	// Base class
#include "TextureTarget.h"



namespace gr
{
	// TODO: This light type is largely deprecated; Remove inheritance, handle it as a simple Light object

	class ImageBasedLight : public virtual gr::Light
	{
	public:
		ImageBasedLight(std::string const& lightName) : Light(lightName, AmbientIBL, glm::vec3(0)), SceneObject(lightName) {};
		~ImageBasedLight() = default;

		// Only 1 image-based light per scene; no need to copy/duplicate
		ImageBasedLight(ImageBasedLight const&) = delete;
		ImageBasedLight(ImageBasedLight&&) = delete;
		ImageBasedLight& operator=(ImageBasedLight const&) = delete;
		ImageBasedLight() = delete;
		
	private:
		
	};
}


