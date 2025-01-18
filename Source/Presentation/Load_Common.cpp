// © 2025 Adam Badke. All rights reserved.
#include "CameraComponent.h"
#include "EntityManager.h"
#include "LightComponent.h"
#include "Load_Common.h"
#include "SceneNodeConcept.h"
#include "TransformComponent.h"

#include "Core/Config.h"
#include "Core/Inventory.h"
#include "Core/LogManager.h"
#include "Core/PerformanceTimer.h"

#include "Core/Util/FileIOUtils.h"

#include "Renderer/RenderManager.h"
#include "Renderer/Texture.h"

// Note: We can't include STBI in our pch, as the following define can only be included ONCE in the project
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb_image.h>


namespace
{
	re::Texture::ImageDataUniquePtr CreateImageDataUniquePtr(void* imageData)
	{
		re::Texture::ImageDataUniquePtr imageDataPtr(
			imageData,
			[](void* stbImageData) { stbi_image_free(stbImageData); });

		return std::move(imageDataPtr);
	};
}

namespace load
{
	template<>
	void TextureFromFilePath<re::Texture>::OnLoadBegin(core::InvPtr<re::Texture>& newTex)
	{
		LOG(std::format("Creating texture from file path \"{}\"", m_filePath).c_str());

		// Register for API-layer creation now to ensure we don't miss our chance for the current frame
		re::RenderManager::Get()->RegisterForCreate(newTex);
	}


	template<>
	std::unique_ptr<re::Texture> TextureFromFilePath<re::Texture>::Load(core::InvPtr<re::Texture>&)
	{
		re::Texture::TextureParams texParams{};
		std::vector<re::Texture::ImageDataUniquePtr> imageData;

		const bool loadSuccess = LoadTextureDataFromFilePath(
			texParams,
			imageData,
			{ m_filePath },
			m_filePath,
			m_colorSpace,
			true,
			false,
			m_colorFallback);

		if (!loadSuccess)
		{
			// Create a error color fallback:
			texParams = re::Texture::TextureParams{
				.m_width = 2,
				.m_height = 2,
				.m_usage = re::Texture::Usage::ColorSrc,
				.m_dimension = re::Texture::Dimension::Texture2D,
				.m_format = m_formatFallback,
				.m_colorSpace = m_colorSpace,
				.m_mipMode = re::Texture::MipMode::None,
			};

			std::unique_ptr<re::Texture::InitialDataVec> errorData = std::make_unique<re::Texture::InitialDataVec>(
				texParams.m_arraySize,
				1, // 1 face
				re::Texture::ComputeTotalBytesPerFace(texParams),
				std::vector<uint8_t>());

			// Initialize with the error color:
			re::Texture::Fill(static_cast<re::Texture::IInitialData*>(errorData.get()), texParams, m_colorFallback);

			return std::unique_ptr<re::Texture>(new re::Texture(m_filePath, texParams, std::move(errorData)));
		}
		SEAssert(loadSuccess, "Failed to load texture: Does the asset exist?");

		// Update the tex params with our preferences:
		texParams.m_mipMode = m_mipMode;

		return std::unique_ptr<re::Texture>(new re::Texture(m_filePath, texParams, std::move(imageData)));
	}


	core::InvPtr<re::Texture> ImportTexture(
		core::Inventory* inventory,
		std::string const& filepath,
		glm::vec4 const& colorFallback /*= re::Texture::k_errorTextureColor*/,
		re::Texture::Format formatFallback /*= re::Texture::Format::RGBA8_UNORM*/,
		re::Texture::ColorSpace colorSpace /*= re::Texture::ColorSpace::sRGB*/,
		re::Texture::MipMode mipMode /*= re::Texture::MipMode::AllocateGenerate*/,
		bool makePermanent /*= false*/)
	{
		std::shared_ptr<load::TextureFromFilePath<re::Texture>> texLoadCtx =
			std::make_shared<load::TextureFromFilePath<re::Texture>>(makePermanent ?
				core::ILoadContextBase::RetentionPolicy::Permanent :
				core::ILoadContextBase::RetentionPolicy::Reusable);

		texLoadCtx->m_filePath = filepath;
		texLoadCtx->m_colorFallback = colorFallback;
		texLoadCtx->m_formatFallback = formatFallback;
		texLoadCtx->m_colorSpace = colorSpace;
		texLoadCtx->m_mipMode = mipMode;

		return inventory->Get<re::Texture>(util::HashKey(filepath), texLoadCtx);
	}


	// ---


	bool LoadTextureDataFromFilePath(
		re::Texture::TextureParams& texParamsOut,
		std::vector<re::Texture::ImageDataUniquePtr>& imageDataOut,
		std::vector<std::string> const& texturePaths,
		std::string const& idName,
		re::Texture::ColorSpace colorSpace,
		bool returnErrorTex,
		bool createAsPermanent /*= false*/,
		glm::vec4 const& errorTexFillColor /*= glm::vec4(1.f, 0.f, 1.f, 1.f)*/)
	{
		SEAssert(texturePaths.size() == 1 || texturePaths.size() == 6, "Can load single faces or cubemaps only");
		SEAssert(texturePaths.size() == 1 || texturePaths.size() == 6, "Invalid number of texture paths");

		LOG("Attempting to load %d texture(s): \"%s\"...", texturePaths.size(), texturePaths[0].c_str());

		util::PerformanceTimer timer;
		timer.Start();

		const uint8_t totalFaces = util::CheckedCast<uint8_t>(texturePaths.size());

		// Modify default TextureParams to be suitable for a generic error texture:
		texParamsOut = re::Texture::TextureParams{
			.m_usage = static_cast<re::Texture::Usage>(re::Texture::Usage::ColorSrc | re::Texture::Usage::ColorTarget),
			.m_dimension = (totalFaces == 1 ?
				re::Texture::Dimension::Texture2D : re::Texture::Dimension::TextureCube),
			.m_format = re::Texture::Format::RGBA8_UNORM,
			.m_colorSpace = colorSpace,
			.m_createAsPermanent = createAsPermanent,
		};
		glm::vec4 fillColor = errorTexFillColor;

		SEAssert(imageDataOut.empty(), "Image data is not empty. This is unexpected");

		// Load the texture, face-by-face:
		for (size_t face = 0; face < totalFaces; face++)
		{
			// Get the image data:
			int width = 0;
			int height = 0;
			int numChannels = 0;
			stbi_info(texturePaths[face].c_str(), &width, &height, &numChannels);

			// We don't support 3-channel textures, allow 1 or 2 channels, or force 4-channel instead
			const int desiredChannels = numChannels == 3 ? 4 : numChannels;

			uint8_t bitDepth = 0;
			void* imageData = nullptr;

			if (stbi_is_hdr(texturePaths[face].c_str())) // HDR
			{
				imageData = stbi_loadf(texturePaths[face].c_str(), &width, &height, &numChannels, desiredChannels);
				bitDepth = 32;
			}
			else if (stbi_is_16_bit(texturePaths[face].c_str()))
			{
				imageData = stbi_load_16(texturePaths[face].c_str(), &width, &height, &numChannels, desiredChannels);
				bitDepth = 16;
			}
			else // Non-HDR
			{
				imageData = stbi_load(texturePaths[face].c_str(), &width, &height, &numChannels, desiredChannels);
				bitDepth = 8;
			}

			if (imageData)
			{
				LOG("Texture \"%s\" is %dx%d, %d-bit, %d channels",
					texturePaths[face].c_str(), width, height, bitDepth, desiredChannels);

				imageDataOut.emplace_back(CreateImageDataUniquePtr(imageData));

				if (face == 0) // 1st face: Update the texture parameters
				{
					texParamsOut.m_width = width;
					texParamsOut.m_height = height;

					if ((width == 1 || height == 1) && (width != height))
					{
						texParamsOut.m_dimension = re::Texture::Dimension::Texture1D;
					}

					switch (desiredChannels)
					{
					case 1:
					{
						if (bitDepth == 8) texParamsOut.m_format = re::Texture::Format::R8_UNORM;
						else if (bitDepth == 16) texParamsOut.m_format = re::Texture::Format::R16F;
						else texParamsOut.m_format = re::Texture::Format::R32F;
					}
					break;
					case 2:
					{
						if (bitDepth == 8) texParamsOut.m_format = re::Texture::Format::RG8_UNORM;
						else if (bitDepth == 16) texParamsOut.m_format = re::Texture::Format::RG16F;
						else texParamsOut.m_format = re::Texture::Format::RG32F;
					}
					break;
					case 4:
					{
						if (bitDepth == 8) texParamsOut.m_format = re::Texture::Format::RGBA8_UNORM;
						else if (bitDepth == 16) texParamsOut.m_format = re::Texture::Format::RGBA16F;
						else texParamsOut.m_format = re::Texture::Format::RGBA32F;
					}
					break;
					default:
						SEAssertF("Invalid number of channels");
					}

					fillColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // Replace default error color
				}
				else // texture already exists: Ensure the face has the same dimensions
				{
					SEAssert(texParamsOut.m_width == width && texParamsOut.m_height == height, "Parameter mismatch");
				}
			}
			else if (returnErrorTex)
			{
				if (!imageDataOut.empty())
				{
					imageDataOut.clear();

					// Reset texParamsOut to be suitable for an error texture
					texParamsOut.m_width = 2;
					texParamsOut.m_height = 2;
					texParamsOut.m_dimension = totalFaces == 1 ?
						re::Texture::Dimension::Texture2D : re::Texture::Dimension::TextureCube;
					texParamsOut.m_format = re::Texture::Format::RGBA8_UNORM;
					texParamsOut.m_colorSpace = re::Texture::ColorSpace::sRGB;
					texParamsOut.m_mipMode = re::Texture::MipMode::AllocateGenerate;

					fillColor = errorTexFillColor;
				}

				SEAssert((texParamsOut.m_usage & re::Texture::Usage::ColorSrc), "Trying to fill a non-color texture");

				const uint8_t numFaces = re::Texture::GetNumFaces(texParamsOut.m_dimension);

				re::Texture::ImageDataUniquePtr newImgDataPtr(
					new re::Texture::InitialDataVec(
						texParamsOut.m_arraySize,
						numFaces,
						re::Texture::ComputeTotalBytesPerFace(texParamsOut),
						std::vector<uint8_t>()),
					[](void*) { std::default_delete<re::Texture::InitialDataVec>(); });

				imageDataOut.emplace_back(std::move(newImgDataPtr));

				// Initialize with the error color:
				re::Texture::Fill(
					static_cast<re::Texture::IInitialData*>(imageDataOut.back().get()), texParamsOut, fillColor);

				return true;
			}
			else
			{
				char const* failResult = stbi_failure_reason();
				LOG_WARNING("Failed to load image \"%s\": %s", texturePaths[0].c_str(), failResult);
				timer.StopSec();
				return false;
			}
		}

		LOG("Loaded texture \"%s\" from \"%s\"in %f seconds...", idName.c_str(), texturePaths[0].c_str(), timer.StopSec());

		// Note: Texture color space must still be set
		return true;
	}


	bool LoadTextureDataFromMemory(
		re::Texture::TextureParams& texParamsOut,
		std::vector<re::Texture::ImageDataUniquePtr>& imageDataOut,
		std::string const& texName,
		unsigned char const* texSrc,
		uint32_t texSrcNumBytes,
		re::Texture::ColorSpace colorSpace)
	{
		SEAssert(texSrc != nullptr && texSrcNumBytes > 0, "Invalid texture memory allocation");

		LOG("Attempting to load texture \"%s\" from memory...", texName.c_str());
		util::PerformanceTimer timer;
		timer.Start();

		bool loadSuccess = false;

		// Modify default TextureParams to be suitable for a generic error texture:
		texParamsOut = re::Texture::TextureParams{
			.m_usage = static_cast<re::Texture::Usage>(re::Texture::Usage::ColorSrc | re::Texture::Usage::ColorTarget),
			.m_dimension = re::Texture::Dimension::Texture2D,
			.m_format = re::Texture::Format::RGBA8_UNORM,
			.m_colorSpace = colorSpace
		};
		glm::vec4 fillColor = re::Texture::k_errorTextureColor;

		SEAssert(imageDataOut.empty(), "Image data is not empty. This is unexpected");

		// Get the image data:
		int width = 0;
		int height = 0;
		int numChannels = 0;
		stbi_info_from_memory(static_cast<stbi_uc const*>(texSrc), texSrcNumBytes, &width, &height, &numChannels);

		// We don't support 3-channel textures, allow 1 or 2 channels, or force 4-channel instead
		const int desiredChannels = numChannels == 3 ? 4 : numChannels;

		uint8_t bitDepth = 0;
		void* imageData = nullptr;
		if (stbi_is_hdr_from_memory(texSrc, texSrcNumBytes))
		{
			imageData = stbi_loadf_from_memory(texSrc, texSrcNumBytes, &width, &height, &numChannels, desiredChannels);
			bitDepth = 32;
		}
		else if (stbi_is_16_bit_from_memory(texSrc, texSrcNumBytes))
		{
			imageData = stbi_load_16_from_memory(texSrc, texSrcNumBytes, &width, &height, &numChannels, desiredChannels);
			bitDepth = 16;
		}
		else // Non-HDR
		{
			imageData = stbi_load_from_memory(texSrc, texSrcNumBytes, &width, &height, &numChannels, desiredChannels);
			bitDepth = 8;
		}

		if (imageData)
		{
			LOG("Texture \"%s\" is %dx%d, %d-bit, %d channels",
				texName.c_str(), width, height, bitDepth, desiredChannels);

			imageDataOut.emplace_back(std::move(CreateImageDataUniquePtr(imageData)));

			// Update the texture parameters:
			texParamsOut.m_width = width;
			texParamsOut.m_height = height;

			if ((width == 1 || height == 1) && (width != height))
			{
				texParamsOut.m_dimension = re::Texture::Dimension::Texture1D;
			}

			switch (desiredChannels)
			{
			case 1:
			{
				if (bitDepth == 8) texParamsOut.m_format = re::Texture::Format::R8_UNORM;
				else if (bitDepth == 16) texParamsOut.m_format = re::Texture::Format::R16F;
				else texParamsOut.m_format = re::Texture::Format::R32F;
			}
			break;
			case 2:
			{
				if (bitDepth == 8) texParamsOut.m_format = re::Texture::Format::RG8_UNORM;
				else if (bitDepth == 16) texParamsOut.m_format = re::Texture::Format::RG16F;
				else texParamsOut.m_format = re::Texture::Format::RG32F;
			}
			break;
			case 4:
			{
				if (bitDepth == 8) texParamsOut.m_format = re::Texture::Format::RGBA8_UNORM;
				else if (bitDepth == 16) texParamsOut.m_format = re::Texture::Format::RGBA16F;
				else texParamsOut.m_format = re::Texture::Format::RGBA32F;
			}
			break;
			default:
				SEAssertF("Invalid number of channels");
			}

			loadSuccess = true;
		}
		else
		{
			SEAssertF("Failed to load image data");
		}

		LOG(std::format("{} texture \"{}\" from memory in %f seconds...",
			loadSuccess ? "Loaded" : "Failed to load",
			texName.c_str(),
			timer.StopSec()).c_str());

		// Note: Texture color space must still be set
		return loadSuccess;
	}


	std::string GenerateTextureColorFallbackName(
		glm::vec4 const& colorFallback, size_t numChannels, re::Texture::ColorSpace colorSpace)
	{
		std::string texName = "Color_" + std::to_string(colorFallback.x) + "_";
		if (numChannels >= 2)
		{
			texName += std::to_string(colorFallback.y) + "_";
			if (numChannels >= 3)
			{
				texName += std::to_string(colorFallback.z) + "_";
				if (numChannels >= 4)
				{
					texName += std::to_string(colorFallback.w) + "_";
				}
			}
		}
		texName += (colorSpace == re::Texture::ColorSpace::sRGB ? "sRGB" : "Linear");

		return texName;
	}


	// Assemble a name for textures loaded from memory: Either use the provided name, or create a unique one
	std::string GenerateEmbeddedTextureName(char const* texName)
	{
		std::string texNameStr;

		if (texName != nullptr)
		{
			texNameStr = std::string(texName);
		}
		else
		{
			static std::atomic<uint32_t> unnamedTexIdx = 0;
			const uint32_t thisTexIdx = unnamedTexIdx.fetch_add(1);
			texNameStr = "EmbeddedTexture_" + std::to_string(thisTexIdx);
		}

		return texNameStr;
	}


	// We override this so we can skip the early registration (which would make the render thread wait)
	void IBLTextureFromFilePath::OnLoadBegin(core::InvPtr<re::Texture>&)
	{
		LOG(std::format("Creating IBL texture from file path \"{}\"", m_filePath).c_str());
	}


	std::unique_ptr<re::Texture> IBLTextureFromFilePath::Load(core::InvPtr<re::Texture>& newIBL)
	{
		std::unique_ptr<re::Texture> result = load::TextureFromFilePath<re::Texture>::Load(newIBL);

		// Register for API-layer creation now that we've loaded the (typically large amount of) data
		re::RenderManager::Get()->RegisterForCreate(newIBL);

		return std::move(result);
	}


	void IBLTextureFromFilePath::OnLoadComplete(core::InvPtr<re::Texture>& newIBL)
	{
		fr::EntityManager* em = fr::EntityManager::Get();

		em->EnqueueEntityCommand([em, newIBL, activationMode = m_activationMode]()
			{
				const bool ambientExists = em->EntityExists<fr::LightComponent::AmbientIBLDeferredMarker>();

				// Create an Ambient LightComponent, and make it active if requested:
				const entt::entity ambientLight = fr::LightComponent::CreateDeferredAmbientLightConcept(
					*em,
					newIBL->GetName().c_str(),
					newIBL);

				switch (activationMode)
				{
				case ActivationMode::Always:
				{
					em->EnqueueEntityCommand<fr::SetActiveAmbientLightCommand>(ambientLight);
				}
				break;
				case ActivationMode::IfNoneExists:
				{
					if (!ambientExists)
					{
						em->EnqueueEntityCommand<fr::SetActiveAmbientLightCommand>(ambientLight);
					}
				}
				break;
				case ActivationMode::Never:
				{
					//
				}
				break;
				default: SEAssertF("Invalid activation mode");
				}
			});
	}

	core::InvPtr<re::Texture> ImportIBL(
		core::Inventory* inventory,
		std::string const& filepath,
		IBLTextureFromFilePath::ActivationMode activationMode,
		bool makePermanent /*= false*/)
	{
		std::shared_ptr<IBLTextureFromFilePath> importCmdIBLLoadCtx = std::make_shared<IBLTextureFromFilePath>(
			makePermanent ?
				core::ILoadContextBase::RetentionPolicy::Permanent :
				core::ILoadContextBase::RetentionPolicy::Reusable);

		importCmdIBLLoadCtx->m_colorSpace = re::Texture::ColorSpace::Linear;
		importCmdIBLLoadCtx->m_mipMode = re::Texture::MipMode::AllocateGenerate;
		importCmdIBLLoadCtx->m_filePath = filepath;
		importCmdIBLLoadCtx->m_activationMode = activationMode;

		return inventory->Get<re::Texture>(util::HashKey(filepath), importCmdIBLLoadCtx);
	}


	CameraMetadata CreateDefaultCamera(fr::EntityManager* em)
	{
		constexpr char const* k_defaultCamName = "DefaultCamera";

		const entt::entity sceneNodeEntity = 
			fr::SceneNode::Create(*em, std::format("{}_SceneNode", k_defaultCamName).c_str(), entt::null);
		
		fr::TransformComponent& cameraTransformCmpt = 
			fr::TransformComponent::AttachTransformComponent(*em, sceneNodeEntity);

		LOG("Creating a default camera");

		const gr::Camera::Config defaultCamConfig
		{
			.m_yFOV = core::Config::Get()->GetValue<float>(core::configkeys::k_defaultFOVKey),			
			.m_near = core::Config::Get()->GetValue<float>(core::configkeys::k_defaultNearKey),
			.m_far = core::Config::Get()->GetValue<float>(core::configkeys::k_defaultFarKey),
			.m_aspectRatio = re::RenderManager::Get()->GetWindowAspectRatio(),
		};	

		fr::CameraComponent::CreateCameraConcept(
			*em,
			sceneNodeEntity,
			k_defaultCamName,
			defaultCamConfig);

		// Offset the camera in an attempt to frame up things located on the origin
		cameraTransformCmpt.GetTransform().TranslateLocal(glm::vec3(0.f, 1.f, 2.f));

		return CameraMetadata
		{
			.m_srcNodeIdx = std::numeric_limits<size_t>::max(), // No source node
			.m_owningEntity = sceneNodeEntity,
		};
	}
}