#pragma once

#include <vector>
#include <string>
#include <memory>

#include <glm/glm.hpp>
#include <GL/glew.h>

using glm::vec3;
using glm::vec4;
using std::vector;
using std::string;

// Predeclarations:
namespace gr
{
	class Texture;
}
namespace SaberEngine
{
	class Shader;
}


namespace SaberEngine
{
	// TODO: Make all of these literal, instead of using offsets to match the shader layout
	// -> Array indexes should be another mapping
	// -> Write a helper function to translate, with an enum to select between shader layout and material indexes?

	enum TEXTURE_TYPE
	{
		TEXTURE_0							= 0,	// RESERVED: Starting offset for *BINDING* Textures to a texture unit: TEXTURE_0 + TEXTURE_<texture type>

		TEXTURE_ALBEDO						= 0,	// Contains transparency in the alpha channel
		TEXTURE_NORMAL						= 1,
		TEXTURE_RMAO						= 2,	// Packed Roughness, Metalic, AmbientOcclusion (RGB) + unused A
		TEXTURE_EMISSIVE					= 3,

		TEXTURE_COUNT						= 4,	// RESERVED: Number of Texture slots a material has

		// GBuffer Texture Target names:
		RENDER_TEXTURE_0					= 4,	// RESERVED: Starting offset for *BINDING* RenderTextures to a texture unit: RENDER_TEXTURE_0 + RENDER_TEXTURE_<texture type>

		RENDER_TEXTURE_ALBEDO				= 0,
		RENDER_TEXTURE_WORLD_NORMAL			= 1,
		RENDER_TEXTURE_RMAO					= 2,
		RENDER_TEXTURE_EMISSIVE				= 3,
		RENDER_TEXTURE_WORLD_POSITION		= 4,
		RENDER_TEXTURE_MATERIAL_PROPERTY_0	= 5,	// MATERIAL_PROPERTY_0
		RENDER_TEXTURE_DEPTH				= 6,	// Make this the last element

		RENDER_TEXTURE_COUNT				= 7,	// RESERVED: Number of target slots a material has

		// Depth map texture units:
		DEPTH_TEXTURE_0						= 11,	// RESERVED: Starting offset for *BINDING* depth targets to a texture unit: DEPTH_TEXTURE_0 + DEPTH_TEXTURE_<texture tyep>. First unit must equal TEXTURE_COUNT + RENDER_TEXTURE_COUNT

		DEPTH_TEXTURE_SHADOW				= 0,

		DEPTH_TEXTURE_COUNT					= 1,	// RESERVED: Number of DEPTH target slots a material has

		// Generic additional texture samplers:
		GENERIC_TEXTURE_0					= 12,
		GENERIC_TEXTURE_1					= 13,
		GENERIC_TEXTURE_2					= 14,
		GENERIC_TEXTURE_3					= 15,
		GENERIC_TEXTURE_4					= 16,
		GENERIC_TEXTURE_5					= 17,
		GENERIC_TEXTURE_6					= 18,
		GENERIC_TEXTURE_7					= 19,

		GENERIC_TEXTURE_COUNT				= 8,	// RESERVED: Number of generic texture samplers

		// Cube maps:
		CUBE_MAP_0							= 20,	// RESERVED: Starting offset for *BINDING* cube RenderTextures to a texture unit: CUBE_MAP_0 + CUBE_MAP_TEXTURE_<texture tyep>. First unit must equal TEXTURE_COUNT + RENDER_TEXTURE_COUNT + DEPTH_TEXTURE_COUNT
		CUBE_MAP_1							= 26,	// RESERVED: Starting offset for *BINDING* cube RenderTextures to a texture unit: CUBE_MAP_1 + CUBE_MAP_TEXTURE_<texture tyep>. First unit must equal TEXTURE_COUNT + RENDER_TEXTURE_COUNT + DEPTH_TEXTURE_COUNT + CUBE_MAP_0

		CUBE_MAP_COUNT						= 2,	// RESERVED: Total number of cube maps allocated

		// Cube map face offsets (Eg. CUBE_MAP_0 + CUBE_MAP_RIGHT)
		CUBE_MAP_RIGHT						= 0,	// X+
		CUBE_MAP_LEFT						= 1,	// X-
		CUBE_MAP_TOP						= 2,	// Y+
		CUBE_MAP_BOTTOM						= 3,	// Y-
		CUBE_MAP_NEAR						= 4,	// Z+
		CUBE_MAP_FAR						= 5,	// Z-

		CUBE_MAP_NUM_FACES					= 6,	// RESERVED: Number of faces in a cube map

	}; // Note: If new enums are added, don't forget to update Material::RENDER_TEXTURE_SAMPLER_NAMES[] as well!
	// TODO: Make this an assert^^^^


	// TODO: Materials should contain generic blocks of parameters (eg. within a struct?)
	enum MATERIAL_PROPERTY_INDEX
	{
		MATERIAL_PROPERTY_0,		// Shader's matProperty0
		//MATERIAL_PROPERTY_1,		// Shader's matProperty1
		//MATERIAL_PROPERTY_2,		// Shader's matProperty2
		//MATERIAL_PROPERTY_3,		// Shader's matProperty3
		//MATERIAL_PROPERTY_4,		// Shader's matProperty4
		//MATERIAL_PROPERTY_5,		// Shader's matProperty5
		//MATERIAL_PROPERTY_6,		// Shader's matProperty6
		//MATERIAL_PROPERTY_7,		// Shader's matProperty7

		MATERIAL_PROPERTY_COUNT		// Reserved: Number of properties a material can hold
	}; // Note: If new enums are added, don't forget to update Material::MATERIAL_PROPERTY_NAMES[] as well!


	class Material
	{
	public:
		Material(string materialName, string shaderName, TEXTURE_TYPE textureCount = TEXTURE_COUNT);
		Material(string materialName, std::shared_ptr<Shader> shader, TEXTURE_TYPE textureCount = TEXTURE_COUNT);

		~Material() { Destroy(); }

		void Destroy();

		// TODO: Copy constructor, assignment operator

		// Getters/Setters:
		inline string const& Name() { return m_name; }
		inline std::shared_ptr<Shader>&	GetShader()	{ return m_shader; }
		inline vec4& Property(MATERIAL_PROPERTY_INDEX index) { return m_properties[index]; }

		std::shared_ptr<gr::Texture>& AccessTexture(TEXTURE_TYPE textureType);
		inline size_t const	NumTextureSlots() { return m_textures.size(); }

		vector<string> const& ShaderKeywords() const { return m_shaderKeywords; }
		void AddShaderKeyword(string const& newKeyword);
		

		// TODO: Materials should specify the texture unit; we shouldn't need to specify textureUnit
		// -> Deleting all Texture constructors etc ensures textures are unique w.r.t the GPU
		// Bind operation should probably go through the material???
		void BindAllTextures(int startingTextureUnit, bool doBind);

		// Helper function: Attaches an array of 6 textures
		void AttachCubeMapTextures(std::shared_ptr<gr::Texture> cubemapTexture);


		// Texture target sampler names:
		//-----------------------------
		const static string TEXTURE_SAMPLER_NAMES[TEXTURE_COUNT];
		const static string RENDER_TEXTURE_SAMPLER_NAMES[RENDER_TEXTURE_COUNT];
		const static string DEPTH_TEXTURE_SAMPLER_NAMES[DEPTH_TEXTURE_COUNT];
		const static string CUBE_MAP_TEXTURE_SAMPLER_NAMES[CUBE_MAP_COUNT];
		const static string MATERIAL_PROPERTY_NAMES[MATERIAL_PROPERTY_COUNT];
		
	protected:
		

	private:
		string m_name;	// Must be unique: Identifies this material

		std::shared_ptr<Shader>	m_shader = nullptr;

		std::vector<std::shared_ptr<gr::Texture>> m_textures;
		
		vec4 m_properties[MATERIAL_PROPERTY_COUNT];	// Generic material properties

		vector<string> m_shaderKeywords;

		// Private member functions:
		//--------------------------

		// Helper function: Initialize arrays
		void Init();
	};
}


