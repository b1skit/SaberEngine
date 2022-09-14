#include <memory>

#define STBI_FAILURE_USERMSG
#include <stb_image.h>	// STB image loader. No need to #define STB_IMAGE_IMPLEMENTATION, as it was already defined in SceneManager


#include "Scene.h"
#include "Light.h"
#include "Camera.h"
#include "GameObject.h"
#include "RenderMesh.h"
#include "CoreEngine.h"
#include "DebugConfiguration.h"

using gr::Camera;
using gr::Light;
using gr::Texture;
using gr::Material;
using gr::Bounds;
using std::string;
using std::vector;
using std::shared_ptr;
using std::unordered_map;
using glm::vec4;


// Data loading helpers:
namespace
{
	#define ERROR_TEXTURE_NAME "ErrorTexture"
	#define ERROR_TEXTURE_COLOR_VEC4 vec4(1.0f, 0.0f, 1.0f, 1.0f)
	#define DEFAULT_ALPHA_VALUE 1.0f // Default alpha value when loading texture data, if no alpha exists


	// Helper functions for loading Low/High Dynamic Range image formats in LoadTextureFileFromPath()
	// Note: targetTexture and imageData must be valid
	void LoadLDRHelper(
		std::vector<glm::vec4>& texels,
		const uint8_t* imageData,
		size_t width,
		size_t height,
		size_t numChannels,
		size_t firstTexelIndex = 0)
	{
		// Read texel values:
		const uint8_t* currentElement = imageData;
		for (size_t row = 0; row < height; row++)
		{
			for (size_t col = 0; col < width; col++)
			{
				vec4 currentPixel(0.0f, 0.0f, 0.0f, DEFAULT_ALPHA_VALUE);

				for (size_t channel = 0; channel < numChannels; channel++)
				{
					// LDR values are stored as normalized floats
					currentPixel[(uint32_t)channel] = (float)((float)((unsigned int)*currentElement) / 255.0f);
					currentElement++;
				}

				texels.at(firstTexelIndex + (row * width) + col) = currentPixel;
			}
		}
	}


	void LoadHDRHelper(
		std::vector<glm::vec4>& texels,
		float const* imageData,
		size_t width, size_t height,
		size_t numChannels,
		size_t firstTexelIndex = 0)
	{
		// Typically most HDRs will be RGB, with 32-bits per channel (https://www.hdrsoft.com/resources/dri.html)

		// Read texel values:
		const float* currentElement = imageData;
		for (size_t row = 0; row < height; row++)
		{
			for (size_t col = 0; col < width; col++)
			{
				// Start with an "empty" pixel, and fill values as we encounter them
				vec4 currentPixel(0.0f, 0.0f, 0.0f, DEFAULT_ALPHA_VALUE);

				// TODO: Support RBG formats for HDR images: We can copy the image data directly into our m_texels array

				for (size_t channel = 0; channel < numChannels; channel++)
				{
					currentPixel[(uint32_t)channel] = *currentElement;

					currentElement++;
				}

				texels.at(firstTexelIndex + (row * width) + col) = currentPixel;
			}
		}
	}


	std::shared_ptr<gr::Texture> LoadTextureFileFromPath(
		vector<string> texturePaths)
	{
		SEAssert("Can load single faces or cubemaps only", texturePaths.size() == 1 || texturePaths.size() == 6);
		SEAssert("Invalid number of texture paths", texturePaths.size() == 1 || texturePaths.size() == 6);

		LOG("Attempting to load " + to_string(texturePaths.size()) + " textures: \"" + texturePaths[0] + "\"...");

		// Flip the y-axis on loading (so pixel (0,0) is in the bottom-left of the image if using OpenGL
		platform::RenderingAPI const& api =
			en::CoreEngine::GetCoreEngine()->GetConfig()->GetRenderingAPI();
		const bool flipY = api == platform::RenderingAPI::OpenGL ? true : false;

		stbi_set_flip_vertically_on_load(flipY);

		const uint32_t totalFaces = (uint32_t)texturePaths.size();

		// Start with parameters suitable for an error texture:
		Texture::TextureParams texParams;
		texParams.m_width = 2;
		texParams.m_height = 2;
		texParams.m_faces = totalFaces;

		texParams.m_texUse = gr::Texture::TextureUse::Color;
		texParams.m_texDimension = totalFaces == 1 ?
			gr::Texture::TextureDimension::Texture2D : gr::Texture::TextureDimension::TextureCubeMap;
		texParams.m_texFormat = gr::Texture::TextureFormat::RGBA8;
		texParams.m_texColorSpace = Texture::TextureColorSpace::Unknown;

		texParams.m_clearColor = ERROR_TEXTURE_COLOR_VEC4;
		texParams.m_texturePath = ERROR_TEXTURE_NAME;
		texParams.m_useMIPs = true;

		// Load the texture, face-by-face:
		shared_ptr<Texture> texture(nullptr);
		for (size_t face = 0; face < totalFaces; face++)
		{
			// Get the image data:
			int width, height, numChannels;
			void* imageData = nullptr;
			size_t bitDepth = 0;

			if (stbi_is_hdr(texturePaths[face].c_str()))	// HDR
			{
				imageData = stbi_loadf(texturePaths[face].c_str(), &width, &height, &numChannels, 0);
				bitDepth = 32;
			}
			else if (stbi_is_16_bit(texturePaths[face].c_str()))
			{
				// TODO: Support loading 16 bit images
				LOG_WARNING("Loading 16 bit image as 8 bit");
				imageData = stbi_load(texturePaths[face].c_str(), &width, &height, &numChannels, 0);
				bitDepth = 8;
				//imageData = stbi_load_16(texturePaths[face].c_str(), &width, &height, &numChannels, 0);
				//bitDepth = 16;
			}
			else // Non-HDR
			{
				imageData = stbi_load(texturePaths[face].c_str(), &width, &height, &numChannels, 0);
				bitDepth = 8;
			}

			if (imageData)
			{
				LOG("Found " + to_string(width) + "x" + to_string(height) + ", " + std::to_string(bitDepth) +
					"-bit texture with " + to_string(numChannels) + " channels");

				if (texture == nullptr)
				{
					// Update the texture parameters:
					texParams.m_width = width;
					texParams.m_height = height;
					texParams.m_faces = (uint32_t)totalFaces;

					if ((width == 1 || height == 1) && (width != height))
					{
						LOG_WARNING("Found 1D texture, but 1D textures are currently not supported. Treating "
							"this texture as 2D");
						texParams.m_texDimension = gr::Texture::TextureDimension::Texture2D; // TODO: Support 1D textures
						/*texParams.m_texDimension = gr::Texture::TextureDimension::Texture1D;*/
					}

					// Currently, we force-pack everything into a 4-channel, 32-bit RGBA texture (in our LDR/HDR helpers).
					// TODO: Support arbitrary texture layouts
					texParams.m_texFormat = gr::Texture::TextureFormat::RGBA32F;

					texParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // Replace default error color
					texParams.m_texturePath = texturePaths[0]; // Note: Texture lookup also uses the path of the first face

					texture = std::make_shared<gr::Texture>(texParams);
				}
				else
				{
					SEAssert("Parameter mismatch", texParams.m_width == width && texParams.m_height == height);
				}

				const size_t firstTexelIndex = face * width * height;
				if (bitDepth == 32)
				{
					float const* castImageData = static_cast<float const*>(imageData);

					LoadHDRHelper(texture->Texels(), castImageData, width, height, numChannels, firstTexelIndex);
				}
				else if (bitDepth == 8 || bitDepth == 16)
				{
					uint8_t const* castImageData = static_cast<uint8_t const*>(imageData);
					LoadLDRHelper(texture->Texels(), castImageData, width, height, numChannels, firstTexelIndex);
				}
				else
				{
					SEAssert("Invalid bit depth", false);
				}

				// Cleanup:
				stbi_image_free(imageData);
			}
			else
			{
				char const* failResult = stbi_failure_reason();
				LOG_WARNING("Failed to load image \"" + texturePaths[0] + "\": " + string(failResult));
				return nullptr;
			}
		}

		// Note: Texture color space must be set, and Create() must be called
		return texture; 
	}

}


namespace fr
{
	SceneData::SceneData(string const& sceneName) :
		m_name(sceneName),
		m_ambientLight(nullptr),
		m_keyLight(nullptr)
	{
	}


	void SceneData::Destroy()
	{
		m_gameObjects.clear();
		m_renderMeshes.clear();
		m_meshes.clear();
		m_textures.clear();
		m_materials.clear();
		m_ambientLight = nullptr;
		m_keyLight = nullptr;
		m_pointLights.clear();
		m_cameras.clear();
		m_sceneWorldSpaceBounds = Bounds();
	}


	size_t SceneData::AddMeshAndUpdateSceneBounds(std::shared_ptr<gr::Mesh> newMesh)
	{
		// Update scene (world) bounds to contain the new mesh:
		Bounds meshWorldBounds(newMesh->GetLocalBounds().GetTransformedBounds(newMesh->GetTransform().Model()));

		if (meshWorldBounds.xMin() < m_sceneWorldSpaceBounds.xMin())
		{
			m_sceneWorldSpaceBounds.xMin() = meshWorldBounds.xMin();
		}
		if (meshWorldBounds.xMax() > m_sceneWorldSpaceBounds.xMax())
		{
			m_sceneWorldSpaceBounds.xMax() = meshWorldBounds.xMax();
		}

		if (meshWorldBounds.yMin() < m_sceneWorldSpaceBounds.yMin())
		{
			m_sceneWorldSpaceBounds.yMin() = meshWorldBounds.yMin();
		}
		if (meshWorldBounds.yMax() > m_sceneWorldSpaceBounds.yMax())
		{
			m_sceneWorldSpaceBounds.yMax() = meshWorldBounds.yMax();
		}

		if (meshWorldBounds.zMin() < m_sceneWorldSpaceBounds.zMin())
		{
			m_sceneWorldSpaceBounds.zMin() = meshWorldBounds.zMin();
		}
		if (meshWorldBounds.zMax() > m_sceneWorldSpaceBounds.zMax())
		{
			m_sceneWorldSpaceBounds.zMax() = meshWorldBounds.zMax();
		}

		// Add the mesh to our array:
		size_t meshIndex = m_meshes.size();
		m_meshes.push_back(newMesh);
		return meshIndex;

	}


	void SceneData::AddLight(std::shared_ptr<Light> newLight)
	{
		// TODO: Seems arbitrary that we cannot duplicate directional (and even ambient?) lights... Why even bother 
		// enforcing this? Just treat all lights the same

		switch (newLight->Type())
		{
		// Check if we've got any existing ambient or directional lights:
		case Light::AmbientIBL:
		{
			SEAssert("Ambient light already exists, cannot have 2 ambient lights", m_ambientLight == nullptr);
			m_ambientLight = newLight;
		}
		break;
		case Light::Directional:
		{
			SEAssert("Direction light already exists, cannot have 2 directional lights", m_keyLight == nullptr);
			m_keyLight = newLight;
		}
		break;
		case Light::Point:
		{
			m_pointLights.emplace_back(newLight);
		}
		break;
		case Light::Spot:
		case Light::Area:
		case Light::Tube:
		default:
			LOG_ERROR("Ignorring unsupported light type");
		break;
		}
	}

	
	void SceneData::AddGameObject(std::shared_ptr<fr::GameObject> newGameObject)
	{
		m_gameObjects.push_back(newGameObject);
		m_renderMeshes.push_back(newGameObject->GetRenderMesh()); // Add the rendermesh to our tracking list
	}


	void SceneData::AddUniqueTexture(shared_ptr<gr::Texture>& newTexture)
	{
		SEAssert("Cannot add null texture to textures table", newTexture != nullptr);

		unordered_map<string, shared_ptr<gr::Texture>>::const_iterator texturePosition =
			m_textures.find(newTexture->GetTexturePath());
		if (texturePosition != m_textures.end()) // Found existing
		{
			newTexture = texturePosition->second;
		}
		else  // Add new
		{
			m_textures[newTexture->GetTexturePath()] = newTexture;
			LOG("Texture \"" + newTexture->GetTexturePath() + "\" registered with scene");
		}
	}


	shared_ptr<Texture> SceneData::GetLoadTextureByPath(vector<string> texturePaths)
	{
		unordered_map<string, shared_ptr<gr::Texture>>::const_iterator texturePosition =
			m_textures.find(texturePaths[0]);
		if (texturePosition != m_textures.end())
		{
			LOG("Texture(s) at \"" + texturePaths[0] + "\" has already been loaded");
			return texturePosition->second;
		}

		SEAssert("Expected either 1 or 6 texture paths", texturePaths.size() == 1 || texturePaths.size() == 6);
		shared_ptr<gr::Texture> result = LoadTextureFileFromPath(vector<string>(texturePaths));
		if (result)
		{
			AddUniqueTexture(result);
		}
		return result;
	}


	void SceneData::AddUniqueMaterial(shared_ptr<Material>& newMaterial)
	{
		SEAssert("Cannot add null material to material table", newMaterial != nullptr);

		// Note: Materials are uniquely identified by name, regardless of the MaterialDefinition they might use
		unordered_map<string, shared_ptr<gr::Material>>::const_iterator matPosition = 
			m_materials.find(newMaterial->Name());		
		if (matPosition != m_materials.end()) // Found existing
		{
			newMaterial = matPosition->second;
		}
		else // Add new
		{
			m_materials[newMaterial->Name()] = newMaterial;
			LOG("Material \"" + newMaterial->Name() + "\" registered with scene");
		}
	}


	std::shared_ptr<gr::Material> const SceneData::GetMaterial(std::string const& materialName) const
	{
		unordered_map<string, shared_ptr<gr::Material>>::const_iterator matPos = m_materials.find(materialName);
		SEAssert("Could not find material", matPos != m_materials.end());

		return matPos->second;
	}
}