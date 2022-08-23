#pragma once

#include <memory>
#include <string>


namespace gr
{
	class Texture;
	class Mesh;
	class Shader;
}

namespace SaberEngine
{
	class Skybox
	{
	public:
		Skybox(std::string const& sceneName);
		Skybox() = delete;

		~Skybox();

		std::shared_ptr<gr::Texture> GetSkyTexture() { return m_skyTexture; }
		std::shared_ptr<gr::Shader> GetSkyShader() { return m_skyShader; }
		std::shared_ptr<gr::Mesh> GetSkyMesh()	{ return m_skyMesh; }


	private:
		std::shared_ptr<gr::Texture> m_skyTexture = nullptr;
		std::shared_ptr<gr::Shader> m_skyShader = nullptr;
		std::shared_ptr<gr::Mesh> m_skyMesh	= nullptr;
	};
}


