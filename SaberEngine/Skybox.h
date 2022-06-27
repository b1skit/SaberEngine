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

		Material*	GetSkyMaterial()	{ return skyMaterial; }
		gr::Mesh*		GetSkyMesh()	{ return skyMesh; }


	private:
		Material* skyMaterial	= nullptr;	// Deallocated in destructor
		gr::Mesh* skyMesh		= nullptr;	// Deallocated in destructor
	};
}


