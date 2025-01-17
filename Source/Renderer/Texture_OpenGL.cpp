// © 2022 Adam Badke. All rights reserved.
#include "Texture_Platform.h"
#include "Texture_OpenGL.h"
#include "Texture.h"
#include "TextureView.h"

#include "Core/Assert.h"
#include "Core/LogManager.h"

#include <GL/glew.h>


namespace
{
	bool GetFormatIsImageTextureCompatible(GLenum internalFormat)
	{
		// TODO: This list is not exhaustive, we should match the internal format with compatible formats:
		// https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBindImageTexture.xhtml
		// See also: glGetTextureParameter
		// https://registry.khronos.org/OpenGL-Refpages/gl4/html/glGetTexParameter.xhtml

		switch (internalFormat)
		{
			case GL_RGBA32F:
			case GL_RGBA16F:
			case GL_RG32F:
			case GL_RG16F:
			case GL_R11F_G11F_B10F:
			case GL_R32F:
			case GL_R16F:
			case GL_RGBA32UI:
			case GL_RGBA16UI:
			case GL_RGB10_A2UI:
			case GL_RGBA8UI:
			case GL_RG32UI:
			case GL_RG16UI:
			case GL_RG8UI:
			case GL_R32UI:
			case GL_R16UI:
			case GL_R8UI:
			case GL_RGBA32I:
			case GL_RGBA16I:
			case GL_RGBA8I:
			case GL_RG32I:
			case GL_RG16I:
			case GL_RG8I:
			case GL_R32I:
			case GL_R16I:
			case GL_R8I:
			case GL_RGBA16:
			case GL_RGB10_A2:
			case GL_RGBA8:
			case GL_RG16:
			case GL_RG8:
			case GL_R16:
			case GL_R8:
			case GL_RGBA16_SNORM:
			case GL_RGBA8_SNORM:
			case GL_RG16_SNORM:
			case GL_RG8_SNORM:
			case GL_R16_SNORM:
			case GL_R8_SNORM:
				return true;
			default: 
			{
				return false;
			}
		}
		
	}
}

namespace opengl
{
	Texture::PlatformParams::PlatformParams(re::Texture& texture)
		: m_textureID(0)
		, m_format(GL_RGBA)
		, m_internalFormat(GL_RGBA32F)
		, m_type(GL_FLOAT)
	{

		re::Texture::TextureParams const& texParams = texture.GetTextureParams();

		// Format:
		/****************/
		switch (texParams.m_format)
		{
		case re::Texture::Format::RGBA32F:
		{
			SEAssert(texParams.m_colorSpace != re::Texture::ColorSpace::sRGB, "32-bit sRGB textures are not supported");
			m_format = GL_RGBA;
			m_internalFormat = GL_RGBA32F;
			m_type = GL_FLOAT;
		}
		break;
		case re::Texture::Format::RG32F:
		{
			SEAssert(texParams.m_colorSpace != re::Texture::ColorSpace::sRGB, "32-bit sRGB textures are not supported");
			m_format = GL_RG;
			m_internalFormat = GL_RG32F;
			m_type = GL_FLOAT;
		}
		break;
		case re::Texture::Format::R32F:
		{
			SEAssert(texParams.m_colorSpace != re::Texture::ColorSpace::sRGB, "32-bit sRGB textures are not supported");
			m_format = GL_R;
			m_internalFormat = GL_R32F;
			m_type = GL_FLOAT;
		}
		break;
		case re::Texture::Format::R32_UINT:
		{
			SEAssert(texParams.m_colorSpace != re::Texture::ColorSpace::sRGB, "32-bit sRGB textures are not supported");
			m_format = GL_R;
			m_internalFormat = GL_R32UI;
			m_type = GL_UNSIGNED_BYTE;
		}
		break;
		case re::Texture::Format::RGBA16F:
		{
			m_format = GL_RGBA;
			m_internalFormat = GL_RGBA16F;
			m_type = GL_HALF_FLOAT;
		}
		break;
		case re::Texture::Format::RG16F:
		{
			m_format = GL_RG;
			m_internalFormat = GL_RG16F;
			m_type = GL_HALF_FLOAT;
		}
		break;
		case re::Texture::Format::R16F:
		{
			m_format = GL_R;
			m_internalFormat = GL_R16F;
			m_type = GL_HALF_FLOAT;
		}
		break;
		case re::Texture::Format::R16_UNORM:
		{
			m_format = GL_R;
			m_internalFormat = GL_R16;
			m_type = GL_UNSIGNED_BYTE;
		}
		break;
		case re::Texture::Format::RGBA8_UNORM:
		{
			// Note: Alpha in GL_SRGB8_ALPHA8 is stored in linear color space, RGB are in sRGB color space
			m_format = GL_RGBA;
			m_type = GL_UNSIGNED_BYTE;

			m_internalFormat = texParams.m_colorSpace == re::Texture::ColorSpace::sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;

			//switch (texParams.m_usage)
			//{
			//case re::Texture::Usage::Color:
			//{
			//	m_internalFormat = texParams.m_colorSpace == re::Texture::ColorSpace::sRGB ? 
			//		GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM : GL_COMPRESSED_RGBA_BPTC_UNORM;
			//}
			//break;
			//case re::Texture::Usage::ColorTarget:
			//case re::Texture::Usage::SwapchainColorProxy:
			//{
			//	m_internalFormat = texParams.m_colorSpace == re::Texture::ColorSpace::sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
			//}
			//break;
			//default:
			//	SEAssertF("Invalid usage");
			//}
		}
		break;
		case re::Texture::Format::RG8_UNORM:
		{
			SEAssertF("Invalid/unsupported texture format");
		}
		break;
		case re::Texture::Format::R8_UNORM:
		{
			SEAssertF("Invalid/unsupported texture format");
		}
		break;
		case re::Texture::Format::R8_UINT:
		{
			m_format = GL_R;
			m_internalFormat = GL_R8;
			m_type = GL_UNSIGNED_BYTE;
		}
		break;
		case re::Texture::Format::Depth32F:
		{
			m_format = GL_DEPTH_COMPONENT;
			m_internalFormat = GL_DEPTH_COMPONENT32F;
			m_type = GL_FLOAT;
		}
		break;
		default:
			SEAssertF("Invalid/unsupported texture format");
		}

		// Is this Texture compatible with compute workloads?
		m_formatIsImageTextureCompatible = GetFormatIsImageTextureCompatible(m_internalFormat);
	}


	Texture::PlatformParams::~PlatformParams()
	{
		SEAssert(glIsTexture(m_textureID) == false && m_textureViews.empty(),
			"opengl::Texture::PlatformParams::~PlatformParams() called before Destroy()");
	}


	void Texture::PlatformParams::Destroy()
	{
		if (glIsTexture(m_textureID))
		{
			glDeleteTextures(1, &m_textureID);
		}

		for (auto const& view : m_textureViews)
		{
			SEAssert(glIsTexture(view.second), "View has an invalid texture handle. This should not be possible");

			glDeleteTextures(1, &view.second);
		}
		m_textureViews.clear();
	}


	void opengl::Texture::Bind(core::InvPtr<re::Texture> const& texture, uint32_t textureUnit)
	{
		// Note: textureUnit is a binding point

		opengl::Texture::PlatformParams const* params =
			texture->GetPlatformParams()->As<opengl::Texture::PlatformParams const*>();

		// TODO: Support texture updates after modification
		SEAssert(params->m_isDirty == false, "Texture has been modified, and needs to be rebuffered");

		glBindTextureUnit(textureUnit, params->m_textureID);
	}


	void Texture::Bind(core::InvPtr<re::Texture> const& texture, uint32_t textureUnit, re::TextureView const& texView)
	{
		opengl::Texture::PlatformParams const* params =
			texture->GetPlatformParams()->As<opengl::Texture::PlatformParams const*>();

		// TODO: Support texture updates after modification
		SEAssert(params->m_isDirty == false, "Texture has been modified, and needs to be rebuffered");

		const GLuint textureID = opengl::Texture::GetOrCreateTextureView(texture, texView);

		glBindTextureUnit(textureUnit, textureID);
	}


	void Texture::BindAsImageTexture(
		core::InvPtr<re::Texture> const& texture, uint32_t textureUnit, re::TextureView const& texView, uint32_t accessMode)
	{
		SEAssert(accessMode == GL_READ_ONLY ||
			accessMode == GL_WRITE_ONLY ||
			accessMode == GL_READ_WRITE,
			"Invalid access mode");

		opengl::Texture::PlatformParams const* texPlatParams =
			texture->GetPlatformParams()->As<opengl::Texture::PlatformParams const*>();

		SEAssert(texPlatParams->m_isCreated, "Texture is not created");

		SEAssert((texture->GetTextureParams().m_usage & re::Texture::Usage::ColorTarget),
			"Texture is not marked for target usage");

		SEAssert(texPlatParams->m_formatIsImageTextureCompatible,
			"Format is not compatible. Note: We currently don't check for non-exact but compatible formats, "
			"but should. See Texture_OpenGL.cpp::GetFormatIsImageTextureCompatible");

		const GLuint textureID = opengl::Texture::GetOrCreateTextureView(texture, texView);

		glBindImageTexture(
			textureUnit,						// unit: Index to bind to
			textureID,							// texture: Name of the texture being bound
			0,									// level: 0, as this is relative to the view
			GL_TRUE,							// layered: Use layered binding? Binds the entire 1/2/3D array if true
			0,									// layer: Layer to bind. Ignored if layered == GL_TRUE
			accessMode,							// access: Type of access that will be performed
			texPlatParams->m_internalFormat);	// format: Internal format	
	}


	void opengl::Texture::Create(core::InvPtr<re::Texture> const& texture)
	{
		opengl::Texture::PlatformParams* params = texture->GetPlatformParams()->As<opengl::Texture::PlatformParams*>();
		SEAssert(!glIsTexture(params->m_textureID) && !params->m_isCreated,
			"Attempting to create a texture that already exists");
		params->m_isCreated = true;

		LOG("Creating & buffering texture: \"%s\"", texture->GetName().c_str());

		re::Texture::TextureParams const& texParams = texture->GetTextureParams();

		const uint32_t width = texture->Width();
		const uint32_t height = texture->Height();
		const uint32_t numMips = texture->GetNumMips();

		// Create texture handles and initialize them:
		switch (texParams.m_dimension)
		{
		case re::Texture::Dimension::Texture1D:
		{
			glCreateTextures(GL_TEXTURE_1D, 1, &params->m_textureID);

			glTextureStorage1D(
				params->m_textureID,
				numMips,
				params->m_internalFormat,
				width);
		}
		break;
		case re::Texture::Dimension::Texture1DArray:
		{
			glCreateTextures(GL_TEXTURE_1D_ARRAY, 1, &params->m_textureID);

			glTextureStorage2D(
				params->m_textureID,
				numMips,
				params->m_internalFormat,
				width,
				texParams.m_arraySize); // Height == no. of array layers
		}
		break;
		case re::Texture::Dimension::Texture2D:
		{
			glCreateTextures(GL_TEXTURE_2D, 1, &params->m_textureID);

			glTextureStorage2D(
				params->m_textureID,
				numMips,
				params->m_internalFormat,
				width,
				height);
		}
		break;
		case re::Texture::Dimension::Texture2DArray:
		{
			glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &params->m_textureID);

			glTextureStorage3D(
				params->m_textureID,
				numMips,
				params->m_internalFormat,
				width,
				height,
				texParams.m_arraySize);
		}
		break;
		case re::Texture::Dimension::Texture3D:
		{
			glCreateTextures(GL_TEXTURE_3D, 1, &params->m_textureID);

			glTextureStorage3D(
				params->m_textureID,
				numMips,
				params->m_internalFormat,
				width,
				height,
				texParams.m_arraySize);
		}
		break;
		case re::Texture::Dimension::TextureCube:
		{
			glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &params->m_textureID);

			glTextureStorage2D(
				params->m_textureID,
				numMips,
				params->m_internalFormat,
				width,
				height);
		}
		break;
		case re::Texture::Dimension::TextureCubeArray:
		{
			SEAssert(texture->GetTotalNumSubresources() == texParams.m_arraySize * 6 * texture->GetNumMips(),
				"Unexpected number of subresources");

			glCreateTextures(GL_TEXTURE_CUBE_MAP_ARRAY, 1, &params->m_textureID);

			glTextureStorage3D(
				params->m_textureID,
				numMips,
				params->m_internalFormat,
				width,
				height,
				texture->GetTotalNumSubresources()); // depth: No. of layer-faces (must be divisible by 6)
		}
		break;
		default:
			SEAssertF("Invalid texture dimension");
		}
		SEAssert(glIsTexture(params->m_textureID) == GL_TRUE, "OpenGL failed to generate new texture name");


		// RenderDoc object name:
		glObjectLabel(
			GL_TEXTURE, params->m_textureID, -1, std::format("{} ({})", texture->GetName(), params->m_textureID).c_str());

		const uint8_t numFaces = re::Texture::GetNumFaces(texture);

		// Upload data (if any) to the GPU:
		if ((texParams.m_usage & re::Texture::Usage::ColorSrc) && texture->HasInitialData())
		{
			for (uint32_t arrayIdx = 0; arrayIdx < texParams.m_arraySize; arrayIdx++)
			{
				for (uint32_t faceIdx = 0; faceIdx < numFaces; faceIdx++)
				{
					void* data = texture->GetTexelData(arrayIdx, faceIdx);
					SEAssert(data, "Color target must have data to buffer");

					switch (texParams.m_dimension)
					{
					case re::Texture::Texture1D:
					{
						glTextureSubImage1D(
							params->m_textureID,
							0,					// level
							0,					// xoffset
							width,				// width
							params->m_format,	// format
							params->m_type,		// type
							data);				// pixels
					}
					break;
					case re::Texture::Texture1DArray:
					{
						SEAssert(height == 1, "Invalid height");

						glTextureSubImage2D(
							params->m_textureID,
							0,					// Level: Mip level
							0,					// xoffset
							arrayIdx,			// yoffset
							width,
							height,				// height
							params->m_format,	// format
							params->m_type,		// type
							data);				// void* data. Nullptr for render targets
					}
					break;
					case re::Texture::Texture2D:
					{
						glTextureSubImage2D(
							params->m_textureID,
							0,					// Level: Mip level
							0,					// xoffset
							0,					// yoffset
							width,
							height,
							params->m_format,	// format
							params->m_type,		// type
							data);				// void* data. Nullptr for render targets
					}
					break;
					case re::Texture::Texture2DArray:
					{
						glTextureSubImage3D(
							params->m_textureID,
							0,						// Level: Mip level
							0,						// xoffset
							0,						// yoffset
							arrayIdx,				// zoffset
							width,
							height,
							1,						// depth: No. of subresources we're updating in this call
							params->m_format,		// format
							params->m_type,			// type
							data);					// void* data. Nullptr for render targets
					}
					break;
					case re::Texture::Texture3D:
					{
						glTextureSubImage3D(
							params->m_textureID,
							0,						// Level: Mip level
							0,						// xoffset
							0,						// yoffset
							arrayIdx,				// zoffset
							width,
							height,
							1,						// depth: No. of subresources we're updating in this call
							params->m_format,		// format
							params->m_type,			// type
							data);					// void* data. Nullptr for render targets
					}
					break;
					case re::Texture::TextureCube:
					{
						glTextureSubImage3D(
							params->m_textureID,
							0,					// Level: Mip level
							0,					// xoffset
							0,					// yoffset
							faceIdx,			// zoffset: Target face
							width,
							height,
							1,					// depth: No. of subresources we're updating in this call
							params->m_format,	// format
							params->m_type,		// type
							data);				// void* data. Nullptr for render targets
					}
					break;
					case re::Texture::TextureCubeArray:
					{
						glTextureSubImage3D(
							params->m_textureID,
							0,						// Level: Mip level
							0,						// xoffset
							0,						// yoffset
							arrayIdx * 6 + faceIdx,	// zoffset
							width,
							height,
							1,						// depth: No. of subresources we're updating in this call
							params->m_format,		// format
							params->m_type,			// type
							data);					// void* data. Nullptr for render targets
					}
					break;
					default: SEAssertF("Invalid dimension");
					}
				}
			}
		}

		// Create mips:
		opengl::Texture::GenerateMipMaps(texture);

		params->m_isDirty = false;

		// Note: We leave the texture and samplers bound
	}


	void Texture::GenerateMipMaps(core::InvPtr<re::Texture> const& texture)
	{
		opengl::Texture::PlatformParams const* params =
			texture->GetPlatformParams()->As<opengl::Texture::PlatformParams const*>();

		if (texture->GetTextureParams().m_mipMode == re::Texture::MipMode::AllocateGenerate)
		{
			glGenerateTextureMipmap(params->m_textureID);
		}
		else
		{
			const GLint maxLevel = GL_TEXTURE_MAX_LEVEL;
			glTextureParameteriv(params->m_textureID, GL_TEXTURE_MAX_LEVEL, &maxLevel);
		}
	}


	GLuint Texture::GetOrCreateTextureView(core::InvPtr<re::Texture> const& tex, re::TextureView const& texView)
	{
		re::TextureView::ValidateView(tex, texView); // _DEBUG only

		re::Texture::TextureParams const& texParams = tex->GetTextureParams();
		opengl::Texture::PlatformParams const* platParams =
			tex->GetPlatformParams()->As<opengl::Texture::PlatformParams const*>();

		const util::HashKey viewDataHash = texView.GetDataHash();

		std::string dimensionName;

		if (!platParams->m_textureViews.contains(viewDataHash))
		{
			GLuint newTexID = 0;

			glGenTextures(1, &newTexID); // We need a completely texture name that is otherwise uninitialized

			GLenum target = 0;

			uint32_t firstMip = 0;
			uint32_t mipLevels = 1;
			uint32_t firstArraySlice = 0;
			uint32_t arraySize = 1;

			switch (texView.m_viewDimension)
			{
			case re::Texture::Texture1D:
			{
				target = GL_TEXTURE_1D;

				firstMip = texView.Texture1D.m_firstMip;
				mipLevels = texView.Texture1D.m_mipLevels;

				dimensionName = "Texture1D";
			}
			break;
			case re::Texture::Texture1DArray:
			{
				target = GL_TEXTURE_1D_ARRAY;

				firstMip = texView.Texture1DArray.m_firstMip;
				mipLevels = texView.Texture1DArray.m_mipLevels;
				firstArraySlice = texView.Texture1DArray.m_firstArraySlice;
				arraySize = texView.Texture1DArray.m_arraySize;

				dimensionName = "Texture1DArray";
			}
			break;
			case re::Texture::Texture2D:
			{
				switch (texParams.m_multisampleMode)
				{
				case re::Texture::MultisampleMode::Disabled:
				{
					target = GL_TEXTURE_2D;

					firstMip = texView.Texture2D.m_firstMip;
					mipLevels = texView.Texture2D.m_mipLevels;

					dimensionName = "Texture2D";
				}
				break;
				case re::Texture::MultisampleMode::Enabled:
				{
					SEAssertF("TODO: Handle mutlisampling");

					dimensionName = "Texture2D_MS";
				}
				break;
				default: SEAssertF("Invalid multisample mode");
				}
			}
			break;
			case re::Texture::Texture2DArray:
			{
				switch (texParams.m_multisampleMode)
				{
				case re::Texture::MultisampleMode::Disabled:
				{
					target = GL_TEXTURE_2D_ARRAY;

					firstMip = texView.Texture2DArray.m_firstMip;
					mipLevels = texView.Texture2DArray.m_mipLevels;
					firstArraySlice = texView.Texture2DArray.m_firstArraySlice;
					arraySize = texView.Texture2DArray.m_arraySize;

					dimensionName = "Texture2DArray";
				}
				break;
				case re::Texture::MultisampleMode::Enabled:
				{
					SEAssertF("TODO: Handle mutlisampling");

					dimensionName = "Texture2DArray_MS";
				}
				break;
				default: SEAssertF("Invalid multisample mode");
				}
			}
			break;
			case re::Texture::Texture3D:
			{
				target = GL_TEXTURE_3D;

				firstMip = texView.Texture3D.m_firstMip;
				mipLevels = texView.Texture3D.m_mipLevels;
				firstArraySlice = texView.Texture3D.m_firstWSlice;
				arraySize = texView.Texture3D.m_wSize;

				dimensionName = "Texture3D";
			}
			break;
			case re::Texture::TextureCube:
			{
				target = GL_TEXTURE_CUBE_MAP;

				firstMip = texView.TextureCube.m_firstMip;
				mipLevels = texView.TextureCube.m_mipLevels;
				arraySize = 6;

				dimensionName = "TextureCube";
			}
			break;
			case re::Texture::TextureCubeArray:
			{
				target = GL_TEXTURE_CUBE_MAP_ARRAY;

				firstMip = texView.TextureCubeArray.m_firstMip;
				mipLevels = texView.TextureCubeArray.m_mipLevels;
				firstArraySlice = texView.TextureCubeArray.m_first2DArrayFace;
				arraySize = texView.TextureCubeArray.m_numCubes * 6;

				dimensionName = "TextureCubeArray";
			}
			break;
			default: SEAssertF("Invalid dimension");
			}

			glTextureView(
				newTexID,						// texture (to be initialized as the view)
				target,							// target
				platParams->m_textureID,		// origTexture
				platParams->m_internalFormat,	// internalFormat
				firstMip,						// minLevel
				mipLevels,						// numLevels
				firstArraySlice,				// minLayer
				arraySize);						// numLayers

			platParams->m_textureViews.emplace(texView.GetDataHash(), newTexID);
			
			// RenderDoc label:
			std::string const& debugName = std::format("{} {} view: 1stMip {}, mipLvls {}, 1stArrIdx {}, arrSize {}",
				platParams->m_textureID,
				dimensionName,
				firstMip,
				mipLevels,
				firstArraySlice,
				arraySize);
			glObjectLabel(GL_TEXTURE, newTexID, -1, debugName.c_str());

			return newTexID;
		}

		return platParams->m_textureViews.at(viewDataHash);
	}


	void Texture::Destroy(re::Texture& texture)
	{
		//
	}


	void Texture::ShowImGuiWindow(core::InvPtr<re::Texture> const& texture, float scale)
	{
		opengl::Texture::PlatformParams const* platParams =
			texture->GetPlatformParams()->As<opengl::Texture::PlatformParams const*>();

		ImGui::Image(platParams->m_textureID, ImVec2(texture->Width() * scale, texture->Height() * scale));
	}
}