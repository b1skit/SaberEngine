#pragma once

#include <string>
#include <vector>
#include <memory>
using std::string;

#include <glm/glm.hpp>
using glm::vec4;

#include "Texture_Platform.h"


namespace platform
{
	bool RegisterPlatformFunctions();
}

namespace gr
{
	class Texture
	{
	public:
		static const uint32_t k_numCubeFaces = 6;

		enum class TextureUse
		{
			Color,
			ColorTarget,
			DepthTarget,
			/*	StencilTarget,
			DepthStencilTarget,	*/

			Invalid,
			TextureUse_Count = Invalid
		};

		enum class TextureDimension
		{
			/*Texture1D,*/
			Texture2D,
			/*Texture2DArray,
			Texture3D,*/
			TextureCubeMap,

			Invalid,
			TextureDimension_Count = Invalid
		};

		enum class TextureFormat
		{
			RGBA32F,	// 32 bits per channel x N channels
			RGB32F,
			RG32F,
			R32F,

			RGBA16F,	// 16 bits per channel x N channels
			RGB16F,
			RG16F,
			R16F,

			RGBA8,		// 8 bits per channel x N channels
			RGB8,
			RG8,
			R8,

			Depth32F,

			Invalid,
			TextureFormat_Count = Invalid
		};

		enum class TextureColorSpace
		{
			sRGB,
			Linear,
			Unknown,	// i.e. Texture loaded from disk

			Invalid,
			TextureSpace_Count = Invalid
		};

		enum class TextureSamplerMode
		{
			Wrap,
			Mirrored,
			Clamp,

			Invalid,
			TextureSamplerMode_Count = Invalid
		};

		enum class TextureMinFilter
		{
			Nearest,
			NearestMipMapLinear,
			Linear,
			LinearMipMapLinear,

			Invalid,
			TextureMinificationMode_Count = Invalid
		};

		enum class TextureMaxFilter
		{
			Nearest,
			Linear,

			Invalid,
			TextureMaxificationMode_Count = Invalid
		};


		struct TextureParams
		{
			uint32_t m_width = 1;
			uint32_t m_height = 1;
			uint32_t m_faces = 1;

			TextureUse m_texUse = TextureUse::Color;
			TextureDimension m_texDimension = TextureDimension::Texture2D;
			TextureFormat m_texFormat = TextureFormat::RGBA32F;
			TextureColorSpace m_texColorSpace = TextureColorSpace::sRGB;

			// Sampler configuration:
			TextureSamplerMode m_texSamplerMode = TextureSamplerMode::Wrap;
			TextureMinFilter m_texMinMode = TextureMinFilter::LinearMipMapLinear;
			TextureMaxFilter m_texMaxMode = TextureMaxFilter::Linear;

			glm::vec4 m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
			std::string m_texturePath = "UnnamedTexture";
			bool m_useMIPs = true; // Should MIPs be created for this texture?
		};

	public:
		Texture(TextureParams params);
		~Texture();

		Texture() = delete;
		Texture(Texture const& rhs) = delete;
		Texture(Texture const&& rhs) = delete;
		Texture& operator=(Texture const& rhs) = delete;

		

		// TODO: Decouple textures and samplers
		// +
		// TODO: Materials should specify the texture unit; a texture shouldn't need to specify textureUnit, which
		// restricts the texture to a single slot/use
		// -> Perhaps the re layer should handle this by examining a material and binding its textures?


		void Create(uint32_t textureUnit);
		void Bind(uint32_t textureUnit, bool doBind); // Can't be const, as we (currently) might call Create() inside...
		// TODO: Write an explicit unbind

		void Destroy();

		

		inline uint32_t const& Width() const { return m_texParams.m_width; }
		inline uint32_t const& Height() const { return m_texParams.m_height; }

		std::vector<glm::vec4>& GetTexels() { m_isDirty = true; return m_texels; }
		std::vector<glm::vec4> const& GetTexels() const {return m_texels; }

		vec4 const& GetTexel(uint32_t u, uint32_t v, uint32_t faceIdx = 0) const; // u == x == col, v == y == row
		glm::vec4 const& GetTexel(uint32_t index) const;

		void SetTexel(uint32_t u, uint32_t v, glm::vec4 value); // u == x == col, v == y == row

		void Fill(vec4 solidColor);	// Fill texture with a solid color
		void Fill(vec4 tl, vec4 bl, vec4 tr, vec4 br); // Fill texture with a color gradient

		std::vector<glm::vec4> const& Texels() const { return m_texels; }
		std::vector<glm::vec4>& Texels() { m_isDirty = true; return m_texels; }
		
		vec4 GetTexelDimenions() const;	// .xyzw = 1/width, 1/height, width, height

		uint32_t GetNumMips() const;
		uint32_t GetMipDimension(uint32_t mipLevel) const;

		platform::Texture::PlatformParams* const GetPlatformParams() { return m_platformParams.get(); } // TODO: WE SHOULD CREATE THESE AT CONSTRUCTION; USE THE m_isCreated FLAG FOR LOGIC
		platform::Texture::PlatformParams const* const GetPlatformParams() const { return m_platformParams.get(); }

		void SetTextureParams(gr::Texture::TextureParams const& params) { m_texParams = params; m_isDirty = true; }
		TextureParams const& GetTextureParams() const { return m_texParams; }

		inline void SetTexturePath(std::string path) { m_texParams.m_texturePath = path; m_isDirty = true; }
		inline string const& GetTexturePath() const { return m_texParams.m_texturePath; }



		// Public static functions:
		//-------------------------

		// Loads a texture object from a (relative) path. 
		// NOTE: Use SceneManager::FindLoadTextureByPath() instead of accessing this function directly, to ensure
		// duplicate textures can be shared
		static bool LoadTextureFileFromPath(
			std::shared_ptr<gr::Texture>& texture,
			string texturePath,
			TextureColorSpace colorSpace,
			bool returnErrorTexIfNotFound = false,			
			uint32_t totalFaces = 1,
			size_t faceIndex = 0);

		static std::shared_ptr<gr::Texture> LoadCubeMapTextureFilesFromPath(
			std::string const& textureRootPath, // folder containing posx/negx/posy/negy/posz/negz.jpg/jpeg/png/tga
			TextureColorSpace const& colorSpace);


	private:
		TextureParams m_texParams;
		std::unique_ptr<platform::Texture::PlatformParams> m_platformParams;

		std::vector<glm::vec4> m_texels;

		bool m_isCreated;
		bool m_isDirty;
		
		// Friends:
		friend bool platform::RegisterPlatformFunctions();
		friend void platform::Texture::PlatformParams::CreatePlatformParams(gr::Texture&);
	};
}


