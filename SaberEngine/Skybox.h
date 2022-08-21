#pragma once

#include "Mesh.h"
#include <string>

using std::string;


namespace SaberEngine
{
	class Material;
	class Mesh;

	class Skybox
	{
	public:
		Skybox(std::shared_ptr<Material> skyMaterial, std::shared_ptr<gr::Mesh> skyMesh);
		Skybox(string sceneName);
		~Skybox();

		std::shared_ptr<Material>	GetSkyMaterial()	{ return m_skyMaterial; }
		std::shared_ptr<gr::Mesh> GetSkyMesh()	{ return m_skyMesh; }


	private:
		std::shared_ptr<Material> m_skyMaterial	= nullptr;
		std::shared_ptr<gr::Mesh> m_skyMesh		= nullptr;
	};
}


