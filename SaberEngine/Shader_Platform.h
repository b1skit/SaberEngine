#pragma once

namespace gr
{
	class Shader;
	class Texture;
	class Sampler;
}

namespace platform
{
	class Shader
	{
	public:
		enum UNIFORM_TYPE
		{
			Matrix4x4f,		// glUniformMatrix4fv
			Matrix3x3f,		// glUniformMatrix3fv
			Vec3f,			// glUniform3fv
			Vec4f,			// glUniform4fv
			Float,			// glUniform1f
			Int,			// glUniform1i
		};

	public:
		struct PlatformParams
		{
			// Params contain unique GPU bindings that should not be arbitrarily copied/duplicated
			PlatformParams() = default;
			PlatformParams(PlatformParams&) = delete;
			PlatformParams(PlatformParams&&) = delete;
			PlatformParams& operator=(PlatformParams&) = delete;
			PlatformParams& operator=(PlatformParams&&) = delete;

			// API-specific GPU bindings should be destroyed here
			virtual ~PlatformParams() = 0;

			static void CreatePlatformParams(gr::Shader& shader);
		};

		// Static helpers:
		static std::string LoadShaderText(const std::string& filepath); // Loads file "filepath" within the shaders dir
		static void	InsertIncludedFiles(std::string& shaderText);
		static void	InsertDefines(std::string& shaderText, std::vector<std::string> const* shaderKeywords);
		

		// Static pointers:
		static void (*Create)(gr::Shader& shader);
		static void (*Bind)(gr::Shader const&, bool doBind);
		static void (*SetUniform)(gr::Shader const&, char const* uniformName, void const* value, UNIFORM_TYPE const& type, int count);
		static void (*Destroy)(gr::Shader&);
		static void (*SetTexture)(
			gr::Shader const& shader, 
			std::string const& shaderName, 
			std::shared_ptr<gr::Texture> texture,
			std::shared_ptr<gr::Sampler const> sampler);
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline platform::Shader::PlatformParams::~PlatformParams() {};
}	

