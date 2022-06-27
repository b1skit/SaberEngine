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
		Skybox(Material* skyMaterial, gr::Mesh* skyMesh);
		Skybox(string sceneName);
		~Skybox();

		Material*	GetSkyMaterial()	{ return m_skyMaterial; }
		gr::Mesh*		GetSkyMesh()	{ return m_skyMesh; }


	private:
		Material* m_skyMaterial	= nullptr;	// Deallocated in destructor
		gr::Mesh* m_skyMesh		= nullptr;	// Deallocated in destructor
	};
}


