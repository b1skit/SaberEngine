#pragma once

#include "grMesh.h"
#include <string>

using std::string;


namespace SaberEngine
{
	class Material;
	class Mesh;

	class Skybox
	{
	public:
		Skybox(Material* skyMaterial, std::shared_ptr<gr::Mesh> skyMesh);
		Skybox(string sceneName);
		~Skybox();

		Material*	GetSkyMaterial()	{ return m_skyMaterial; }
		std::shared_ptr<gr::Mesh> GetSkyMesh()	{ return m_skyMesh; }


	private:
		Material* m_skyMaterial	= nullptr;	// Deallocated in destructor
		std::shared_ptr<gr::Mesh> m_skyMesh		= nullptr;	// Deallocated in destructor
	};
}


