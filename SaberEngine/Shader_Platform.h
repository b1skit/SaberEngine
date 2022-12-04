#pragma once

namespace gr
{
	class Shader;
	class Texture;
	class Sampler;
}

namespace re
{
	class ParameterBlock;
}

namespace platform
{
	class Shader
	{
	public:
		enum UniformType
		{
			Matrix4x4f,
			Matrix3x3f,
			Vec3f,
			Vec4f,
			Float,
			Int,
			Texture,
			Sampler
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
		static void (*SetUniform)(
			gr::Shader const& shader, 
			std::string const& uniformName, 
			void* value, 
			UniformType const type,
			int const count);
		static void (*SetParameterBlock)(gr::Shader const& shader, re::ParameterBlock const& paramBlock);
		static void (*Destroy)(gr::Shader&);
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline platform::Shader::PlatformParams::~PlatformParams() {};
}	

