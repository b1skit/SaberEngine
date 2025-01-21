// © 2025 Adam Badke. All rights reserved.
#include "AnimationComponent.h"
#include "Camera.h"
#include "CameraComponent.h"
#include "EntityManager.h"
#include "LightComponent.h"
#include "Load_Common.h"
#include "Load_GLTF.h"
#include "MaterialInstanceComponent.h"
#include "MeshConcept.h"
#include "MeshMorphComponent.h"
#include "MeshPrimitiveComponent.h"
#include "RelationshipComponent.h"
#include "SceneManager.h"
#include "SceneNodeConcept.h"
#include "SkinningComponent.h"
#include "TransformComponent.h"

#include "Core/Config.h"
#include "Core/Inventory.h"

#include "Core/Util/ByteVector.h"
#include "Core/Util/FileIOUtils.h"

#include "Renderer/Material_GLTF.h"
#include "Renderer/VertexStreamBuilder.h"

#pragma warning(disable : 4996) // Suppress error C4996 (Caused by use of fopen, strcpy, strncpy in cgltf.h)
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"


namespace
{
	// Each element/index corresponds to an animation: Multiple animations may target the same node
	using NodeToAnimationDataMaps = std::vector<std::unordered_map<cgltf_node const*, fr::AnimationData>>;

	// We pre-parse the GLTF scene hierarchy into our EnTT registry, and then update the entities later on
	using NodeToEntityMap = std::unordered_map<cgltf_node const*, entt::entity>;

	struct SkinMetadata
	{
		std::vector<glm::mat4> m_inverseBindMatrices;
	};
	using SkinToSkinMetadata = std::unordered_map<cgltf_skin const*, SkinMetadata>;

	struct MeshPrimitiveMetadata
	{
		core::InvPtr<gr::MeshPrimitive> m_meshPrimitive;
		core::InvPtr<gr::Material> m_material;
	};
	using PrimitiveToMeshPrimitiveMap = std::unordered_map<cgltf_primitive const*, MeshPrimitiveMetadata>;

	// Map from a MeshConcept entity, to a vector of Mesh/MeshPrimitive/Bounds entities. Used by SkinningComponent
	using MeshEntityToAllBoundsEntityMap = std::unordered_map<entt::entity, std::vector<entt::entity>>;

	
	struct FileMetadata
	{
		std::string m_filePath;
		std::string m_sceneRootPath;

		std::unique_ptr<fr::AnimationController> m_animationController;
		NodeToAnimationDataMaps m_nodeToAnimationData;

		SkinToSkinMetadata m_skinToSkinMetadata;
		std::unordered_set<cgltf_node const*> m_skeletonNodes;
		mutable std::mutex m_skinDataMutex;

		PrimitiveToMeshPrimitiveMap m_primitiveToMeshPrimitiveMetadata;
		std::mutex m_primitiveToMeshPrimitiveMetadataMutex;

		MeshEntityToAllBoundsEntityMap m_meshEntityToBoundsEntityMap;
		std::mutex m_meshEntityToBoundsEntityMapMutex;

		std::vector<load::CameraMetadata> m_cameraMetadata;
		std::mutex m_cameraMetadataMutex;

		NodeToEntityMap m_nodeToEntity;
	};


	struct GLTFSceneHandle
	{
		// Note: This is a bit of a hack, the actual GLTF scene data is managed/owned by the load context (as it is
		// still required to configure the scene after the initial Load() is complete). So we use this object as a dummy
		// type to satisfy the InvPtr system

		void Destroy() {/* Do nothing */ }
	};


	inline fr::InterpolationMode CGLTFInterpolationTypeToFrInterpolationType(
		cgltf_interpolation_type interpolationType,
		cgltf_animation_path_type animationPathType)
	{
		switch (interpolationType)
		{
		case cgltf_interpolation_type::cgltf_interpolation_type_linear:
		{
			if (animationPathType == cgltf_animation_path_type_rotation)
			{
				return fr::InterpolationMode::SphericalLinearInterpolation;
			}
			return fr::InterpolationMode::Linear;
		}
		case cgltf_interpolation_type::cgltf_interpolation_type_step: return fr::InterpolationMode::Step;
		case cgltf_interpolation_type::cgltf_interpolation_type_cubic_spline: return fr::InterpolationMode::CubicSpline;
		default: SEAssertF("Invalid interpolation type");
		}
		return fr::InterpolationMode::Linear; // This should never happen
	}


	inline fr::AnimationPath CGLTFAnimationPathToFrAnimationPath(cgltf_animation_path_type pathType)
	{
		switch (pathType)
		{
		case cgltf_animation_path_type::cgltf_animation_path_type_translation: return fr::AnimationPath::Translation;
		case cgltf_animation_path_type::cgltf_animation_path_type_rotation: return fr::AnimationPath::Rotation;
		case cgltf_animation_path_type::cgltf_animation_path_type_scale: return fr::AnimationPath::Scale;
		case cgltf_animation_path_type::cgltf_animation_path_type_weights: return fr::AnimationPath::Weights;
		case cgltf_animation_path_type::cgltf_animation_path_type_invalid:
		case cgltf_animation_path_type::cgltf_animation_path_type_max_enum:
		default: SEAssertF("Invalid animation path type");
		}
		return fr::AnimationPath::Translation; // This should never happen
	}


	inline gr::MeshPrimitive::PrimitiveTopology CGLTFPrimitiveTypeToPrimitiveTopology(cgltf_primitive_type primitiveType)
	{
		switch (primitiveType)
		{
		case cgltf_primitive_type::cgltf_primitive_type_points: return gr::MeshPrimitive::PrimitiveTopology::PointList;
		case cgltf_primitive_type::cgltf_primitive_type_lines: return gr::MeshPrimitive::PrimitiveTopology::LineList;
		case cgltf_primitive_type::cgltf_primitive_type_line_strip: return gr::MeshPrimitive::PrimitiveTopology::LineStrip;
		case cgltf_primitive_type::cgltf_primitive_type_triangles: return gr::MeshPrimitive::PrimitiveTopology::TriangleList;
		case cgltf_primitive_type::cgltf_primitive_type_triangle_strip: return gr::MeshPrimitive::PrimitiveTopology::TriangleStrip;
		case cgltf_primitive_type::cgltf_primitive_type_triangle_fan:
		case cgltf_primitive_type::cgltf_primitive_type_line_loop:
		case cgltf_primitive_type::cgltf_primitive_type_invalid:
		case cgltf_primitive_type::cgltf_primitive_type_max_enum:
		default: SEAssertF("Invalid/unsupported primitive type/draw mode. SE does not support line loops or triangle fans");
		}
		return gr::MeshPrimitive::PrimitiveTopology::TriangleList; // This should never happen
	}


	std::string GenerateGLTFTextureName(
		std::string const& sceneRootPath,
		cgltf_texture const* textureSrc)
	{
		SEAssert(textureSrc && textureSrc->image, "Invalid texture source");

		std::string texName;

		if (textureSrc->image->uri &&
			std::strncmp(textureSrc->image->uri, "data:image/", 11) == 0) // URI = embedded data
		{
			if (textureSrc->image->name)
			{
				texName = textureSrc->image->name;
			}
			else
			{
				// Data URIs are long; Just choose the first N characters and hope for the best...
				constexpr uint8_t k_maxURINameLength = 128;
				std::string const& uriString = textureSrc->image->uri;
				texName = uriString.substr(0, k_maxURINameLength);
			}
		}
		else if (textureSrc->image->uri) // uri is a filename (e.g. "myImage.png")
		{
			texName = sceneRootPath + textureSrc->image->uri;
		}
		else if (textureSrc->image->buffer_view) // texture data is already loaded in memory
		{
			if (textureSrc->image->name)
			{
				texName = textureSrc->image->name;
			}
			else if (textureSrc->image->buffer_view->name)
			{
				texName = textureSrc->image->buffer_view->name;
			}
			else
			{
				// Hail mary: We've got nothing else to go on, so use the buffer_view pointer address
				texName = std::format("UnnamedImageBuffer_{}_{}",
					reinterpret_cast<uint64_t>(textureSrc->image->buffer_view),
					sceneRootPath);
			}
		}

		return texName;
	}


	std::string GenerateGLTFTextureName(
		std::string const& sceneRootPath,
		cgltf_texture const* textureSrc,
		glm::vec4 const& colorFallback,
		re::Texture::Format formatFallback,
		re::Texture::ColorSpace colorSpace)
	{
		if (textureSrc && textureSrc->image)
		{
			return GenerateGLTFTextureName(sceneRootPath, textureSrc);
		}
		else
		{
			const size_t numChannels = re::Texture::GetNumberOfChannels(formatFallback);

			return load::GenerateTextureColorFallbackName(colorFallback, numChannels, colorSpace);
		}
	}


	// Generate a unique name for a CGLTF material from (some of) the values in the cgltf_material struct
	inline std::string GenerateGLTFMaterialName(
		std::shared_ptr<FileMetadata> const& fileMetadata, cgltf_material const* material)
	{
		if (!material) // No material? Use the default material
		{
			return en::DefaultResourceNames::k_defaultGLTFMaterialName;
		}

		util::HashKey matHash = 0;

		std::string matName;

		if (material->name)
		{
			util::AddDataBytesToHash(matHash, material->name);
			matName = material->name;
		}

		// pbr_metallic_roughness:
		if (material->has_pbr_metallic_roughness)
		{
			if (material->pbr_metallic_roughness.base_color_texture.texture)
			{
				util::AddDataBytesToHash(matHash, GenerateGLTFTextureName(
					fileMetadata->m_sceneRootPath,
					material->pbr_metallic_roughness.base_color_texture.texture));
			}
			if (material->pbr_metallic_roughness.metallic_roughness_texture.texture)
			{
				util::AddDataBytesToHash(matHash, GenerateGLTFTextureName(
					fileMetadata->m_sceneRootPath,
					material->pbr_metallic_roughness.metallic_roughness_texture.texture));
			}
			util::AddDataBytesToHash(matHash, material->pbr_metallic_roughness.base_color_factor[0]);
			util::AddDataBytesToHash(matHash, material->pbr_metallic_roughness.base_color_factor[1]);
			util::AddDataBytesToHash(matHash, material->pbr_metallic_roughness.base_color_factor[2]);
			util::AddDataBytesToHash(matHash, material->pbr_metallic_roughness.base_color_factor[3]);

			util::AddDataBytesToHash(matHash, material->pbr_metallic_roughness.metallic_factor);
			util::AddDataBytesToHash(matHash, material->pbr_metallic_roughness.roughness_factor);
		}

		if (material->has_pbr_specular_glossiness)
		{
			SEAssertF("TODO: Hash these");
		}

		if (material->has_clearcoat)
		{
			SEAssertF("TODO: Hash these");
		}

		if (material->has_transmission)
		{
			SEAssertF("TODO: Hash these");
		}

		if (material->has_volume)
		{
			SEAssertF("TODO: Hash these");
		}

		if (material->has_ior)
		{
			SEAssertF("TODO: Hash these");
		}

		if (material->has_specular)
		{
			SEAssertF("TODO: Hash these");
		}

		if (material->has_sheen)
		{
			SEAssertF("TODO: Hash these");
		}

		if (material->has_emissive_strength)
		{
			util::AddDataBytesToHash(matHash, material->emissive_strength.emissive_strength);	
		}

		if (material->has_iridescence)
		{
			SEAssertF("TODO: Hash these");
		}

		if (material->has_anisotropy)
		{
			SEAssertF("TODO: Hash these");
		}

		if (material->has_dispersion)
		{
			SEAssertF("TODO: Hash these");
		}

		if (material->normal_texture.texture)
		{
			util::AddDataBytesToHash(matHash, GenerateGLTFTextureName(
				fileMetadata->m_sceneRootPath,
				material->normal_texture.texture));
		}

		if (material->occlusion_texture.texture)
		{
			util::AddDataBytesToHash(matHash, GenerateGLTFTextureName(
				fileMetadata->m_sceneRootPath,
				material->occlusion_texture.texture));
		}

		if (material->emissive_texture.texture)
		{
			util::AddDataBytesToHash(matHash, GenerateGLTFTextureName(
				fileMetadata->m_sceneRootPath,
				material->emissive_texture.texture));
		}

		util::AddDataBytesToHash(matHash, material->emissive_factor[0]);
		util::AddDataBytesToHash(matHash, material->emissive_factor[1]);
		util::AddDataBytesToHash(matHash, material->emissive_factor[2]);

		util::AddDataBytesToHash(matHash, material->alpha_mode);
		util::AddDataBytesToHash(matHash, material->alpha_cutoff);
		util::AddDataBytesToHash(matHash, material->double_sided);

		util::AddDataBytesToHash(matHash, material->unlit);

		SEAssert(material->extras.data == nullptr, "TODO: Handle extra data");

		return std::format("{}_{}", matName.empty() ? "UnnamedMaterial" : matName, matHash);
	}


	inline std::string GenerateGLTFNodeName(
		std::shared_ptr<FileMetadata> const& fileMetadata, cgltf_node const* gltfNode, size_t nodeIdx)
	{
		return gltfNode->name ? gltfNode->name : std::format("{}_Node_[{}]", fileMetadata->m_filePath, nodeIdx);
	}


	inline std::string GenerateGLTFMeshName(
		std::shared_ptr<FileMetadata> const& fileMetadata, cgltf_mesh const* curMesh, size_t meshIdx)
	{
		return curMesh->name ? curMesh->name : std::format("{}_Mesh[{}]", fileMetadata->m_filePath, meshIdx);
	}


	inline std::string GenerateGLTFMeshPrimitiveName(
		std::shared_ptr<FileMetadata> const& fileMetadata, cgltf_mesh const* curMesh, size_t meshIdx, size_t primIdx)
	{
		return std::format("{}_Primitive[{}]", GenerateGLTFMeshName(fileMetadata, curMesh, meshIdx), primIdx);
	}


	inline std::string GenerateGLTFCameraName(
		std::shared_ptr<FileMetadata> const& fileMetadata, cgltf_camera const* camNode, size_t nodeIdx)
	{
		return camNode->name ? camNode->name : std::format("{}_Camera_[{}]", fileMetadata->m_filePath, nodeIdx);
	}


	inline std::string GenerateGLTFLightName(
		std::shared_ptr<FileMetadata> const& fileMetadata, cgltf_light const* lightNode, size_t nodeIdx)
	{
		return lightNode->name ? lightNode->name : std::format("{}_Light_[{}]", fileMetadata->m_filePath, nodeIdx);
	}


	inline std::string GenerateGLTFAnimationControllerName(
		std::shared_ptr<FileMetadata> const& fileMetadata)
	{
		return std::format("AnimationController: {}", fileMetadata->m_filePath);
	}


	util::ByteVector UnpackGLTFColorAttributeAsVec4(cgltf_attribute const& colorAttribute)
	{
		SEAssert(colorAttribute.type == cgltf_attribute_type::cgltf_attribute_type_color,
			"Attribute is not a color attribute");

		const size_t numComponents = cgltf_num_components(colorAttribute.data->type);
		const size_t numElements = colorAttribute.data->count;
		const size_t totalFloatElements = numComponents * numElements;

		util::ByteVector colors = util::ByteVector::Create<glm::vec4>(colorAttribute.data->count);

		switch (numComponents)
		{
		case 3:
		{
			std::vector<glm::vec3> tempColors(colorAttribute.data->count);

			const bool unpackResult = cgltf_accessor_unpack_floats(
				colorAttribute.data,
				&tempColors[0].r,
				totalFloatElements);
			SEAssert(unpackResult, "Failed to unpack data");

			for (size_t colIdx = 0; colIdx < tempColors.size(); ++colIdx)
			{
				// GLTF specs: Color attributes of vec3 type are assumed to have an alpha of 1
				colors.at<glm::vec4>(colIdx) = glm::vec4(tempColors[colIdx], 1.f);
			}
		}
		break;
		case 4:
		{
			const bool unpackResult = cgltf_accessor_unpack_floats(
				colorAttribute.data,
				static_cast<float*>(colors.data<float>()),
				totalFloatElements);
			SEAssert(unpackResult, "Failed to unpack data");
		}
		break;
		default: SEAssertF("Invalid number of color components");
		}

		return colors;
	}


	template<typename T>
	struct TextureFromCGLTF final : public virtual core::ILoadContext<re::Texture>
	{
		void OnLoadBegin(core::InvPtr<re::Texture>&) override
		{
			LOG(std::format("Creating texture \"{}\" from GLTF", m_texName).c_str());
		}

		std::unique_ptr<re::Texture> Load(core::InvPtr<re::Texture>& newTex) override
		{
			re::Texture::TextureParams texParams{};
			std::vector<re::Texture::ImageDataUniquePtr> imageData;

			bool loadSuccess = false;
			if (m_srcTexture && m_srcTexture->image)
			{
				if (m_srcTexture->image->uri &&
					std::strncmp(m_srcTexture->image->uri, "data:image/", 11) == 0) // uri = embedded data
				{
					// Unpack the base64 data embedded in the URI. Note: Usage of cgltf's cgltf_load_buffer_base64
					// function is currently not well documented. This solution was cribbed from Google's filament
					// usage (parseDataUri, line 285):
					// https://github.com/google/filament/blob/676694e4589dca55c1cdbbb669cf3dba0e2b576f/libs/gltfio/src/ResourceLoader.cpp

					const char* comma = strchr(m_srcTexture->image->uri, ',');
					if (comma && comma - m_srcTexture->image->uri >= 7 && strncmp(comma - 7, ";base64", 7) == 0)
					{
						const char* base64 = comma + 1;
						const size_t base64Size = strlen(base64);
						size_t size = base64Size - base64Size / 4;
						if (base64Size >= 2) {
							size -= base64[base64Size - 2] == '=';
							size -= base64[base64Size - 1] == '=';
						}
						void* data = 0;
						cgltf_options options = {};
						cgltf_result result = cgltf_load_buffer_base64(&options, size, base64, &data);

						// Data is decoded, now load it as usual:
						loadSuccess = load::LoadTextureDataFromMemory(
							texParams,
							imageData,
							m_texName,
							static_cast<unsigned char const*>(data),
							static_cast<uint32_t>(size),
							m_colorSpace);
					}
				}
				else if (m_srcTexture->image->uri) // uri is a filename (e.g. "myImage.png")
				{
					loadSuccess = load::LoadTextureDataFromFilePath(
						texParams,
						imageData,
						{ m_texName },
						m_texName,
						m_colorSpace,
						false,
						false,
						re::Texture::k_errorTextureColor);
				}
				else if (m_srcTexture->image->buffer_view) // texture data is already loaded in memory
				{
					unsigned char const* texSrc = static_cast<unsigned char const*>(
						m_srcTexture->image->buffer_view->buffer->data) + m_srcTexture->image->buffer_view->offset;

					const uint32_t texSrcNumBytes = static_cast<uint32_t>(m_srcTexture->image->buffer_view->size);
					loadSuccess = load::LoadTextureDataFromMemory(
						texParams,
						imageData,
						m_texName,
						texSrc,
						texSrcNumBytes,
						m_colorSpace);
				}
			}
			else // Create a error color fallback:
			{
				texParams = re::Texture::TextureParams{
					.m_width = 2,
					.m_height = 2,
					.m_usage = static_cast<re::Texture::Usage>(re::Texture::ColorSrc | re::Texture::ColorTarget),
					.m_dimension = re::Texture::Dimension::Texture2D,
					.m_format = m_formatFallback,
					.m_colorSpace = m_colorSpace,
				};

				std::unique_ptr<re::Texture::InitialDataVec> errorData = std::make_unique<re::Texture::InitialDataVec>(
					texParams.m_arraySize,
					1, // 1 face
					re::Texture::ComputeTotalBytesPerFace(texParams),
					std::vector<uint8_t>());

				// Initialize with the error color:
				re::Texture::Fill(static_cast<re::Texture::IInitialData*>(errorData.get()), texParams, m_colorFallback);

				re::RenderManager::Get()->RegisterForCreate(newTex);
				return std::unique_ptr<re::Texture>(new re::Texture(m_texName, texParams, std::move(errorData)));
			}

			SEAssert(loadSuccess, "Failed to load texture: Does the asset exist?");

			// Finally, register for creation before waiting threads are unblocked
			re::RenderManager::Get()->RegisterForCreate(newTex);
			return std::unique_ptr<re::Texture>(new re::Texture(m_texName, texParams, std::move(imageData)));
		}


		std::string m_texName;

		std::shared_ptr<cgltf_data const> m_data;
		cgltf_texture const* m_srcTexture = nullptr;
		glm::vec4 m_colorFallback = re::Texture::k_errorTextureColor;
		re::Texture::Format m_formatFallback = re::Texture::Format::Invalid;
		re::Texture::ColorSpace m_colorSpace = re::Texture::ColorSpace::Linear;
	};


	core::InvPtr<re::Texture> LoadGLTFTextureOrColor(
		core::Inventory* inventory,
		std::shared_ptr<cgltf_data const> const& data, // So we can keep this alive while we're accessing the cgltf_texture*
		std::string const& sceneRootPath,
		cgltf_texture const* texture,
		glm::vec4 const& colorFallback,
		re::Texture::Format formatFallback,
		re::Texture::ColorSpace colorSpace)
	{
		SEAssert(formatFallback != re::Texture::Format::Depth32F && formatFallback != re::Texture::Format::Invalid,
			"Invalid fallback format");

		std::string const& texName =
			GenerateGLTFTextureName(sceneRootPath, texture, colorFallback, formatFallback, colorSpace);

		if (inventory->Has<re::Texture>(texName))
		{
			return inventory->Get<re::Texture>(texName);
		}

		std::shared_ptr<TextureFromCGLTF<re::Texture>> loadContext = std::make_shared<TextureFromCGLTF<re::Texture>>();

		loadContext->m_texName = texName;

		loadContext->m_data = data;
		loadContext->m_srcTexture = texture;

		loadContext->m_colorFallback = colorFallback;
		loadContext->m_formatFallback = formatFallback;
		loadContext->m_colorSpace = colorSpace;

		core::InvPtr<re::Texture> const& newTexture = inventory->Get(
			util::HashKey(texName),
			static_pointer_cast<core::ILoadContext<re::Texture>>(loadContext));

		return newTexture;
	}


	template<typename T>
	struct MaterialLoadContext_GLTF final : public virtual core::ILoadContext<gr::Material>
	{
		void OnLoadBegin(core::InvPtr<gr::Material>&) override
		{
			LOG(std::format("Loading material \"{}\" from GLTF", m_matName).c_str());
		}

		std::unique_ptr<gr::Material> Load(core::InvPtr<gr::Material>& newMatHandle) override
		{
			SEAssert(m_srcMaterial, "Source material is null, this is unexpected");
			SEAssert(m_srcMaterial->has_pbr_metallic_roughness == 1,
				"We currently only support the PBR metallic/roughness material model");

			// GLTF specifications: If a texture is not given, all texture components are assumed to be 1.f
			// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metallic-roughness-material
			constexpr glm::vec4 k_defaultTextureColor(1.f, 1.f, 1.f, 1.f);

			std::unique_ptr<gr::Material> newMat(new gr::Material_GLTF(m_matName));

			// BaseColorTex
			newMat->SetTexture(
				gr::Material_GLTF::TextureSlotIdx::BaseColor,
				newMatHandle.AddDependency(LoadGLTFTextureOrColor(
					m_inventory,
					m_data,
					m_sceneRootPath,
					m_srcMaterial->pbr_metallic_roughness.base_color_texture.texture,
					k_defaultTextureColor,
					gr::Material_GLTF::GetDefaultTextureFormat(gr::Material_GLTF::TextureSlotIdx::BaseColor),
					gr::Material_GLTF::GetDefaultTextureColorSpace(gr::Material_GLTF::TextureSlotIdx::BaseColor))),
				m_srcMaterial->pbr_metallic_roughness.base_color_texture.texcoord);


			// MetallicRoughnessTex
			newMat->SetTexture(
				gr::Material_GLTF::TextureSlotIdx::MetallicRoughness,
				newMatHandle.AddDependency(LoadGLTFTextureOrColor(
					m_inventory,
					m_data,
					m_sceneRootPath,
					m_srcMaterial->pbr_metallic_roughness.metallic_roughness_texture.texture,
					k_defaultTextureColor,
					gr::Material_GLTF::GetDefaultTextureFormat(gr::Material_GLTF::TextureSlotIdx::MetallicRoughness),
					gr::Material_GLTF::GetDefaultTextureColorSpace(gr::Material_GLTF::TextureSlotIdx::MetallicRoughness))),
				m_srcMaterial->pbr_metallic_roughness.metallic_roughness_texture.texcoord);

			// NormalTex
			newMat->SetTexture(
				gr::Material_GLTF::TextureSlotIdx::Normal,
				newMatHandle.AddDependency(LoadGLTFTextureOrColor(
					m_inventory,
					m_data,
					m_sceneRootPath,
					m_srcMaterial->normal_texture.texture,
					glm::vec4(0.5f, 0.5f, 1.0f, 0.0f), // Equivalent to a [0,0,1] normal after unpacking
					gr::Material_GLTF::GetDefaultTextureFormat(gr::Material_GLTF::TextureSlotIdx::Normal),
					gr::Material_GLTF::GetDefaultTextureColorSpace(gr::Material_GLTF::TextureSlotIdx::Normal))),
				m_srcMaterial->normal_texture.texcoord);

			// OcclusionTex
			newMat->SetTexture(
				gr::Material_GLTF::TextureSlotIdx::Occlusion,
				newMatHandle.AddDependency(LoadGLTFTextureOrColor(
					m_inventory,
					m_data,
					m_sceneRootPath,
					m_srcMaterial->occlusion_texture.texture,
					k_defaultTextureColor,	// Completely unoccluded
					gr::Material_GLTF::GetDefaultTextureFormat(gr::Material_GLTF::TextureSlotIdx::Occlusion),
					gr::Material_GLTF::GetDefaultTextureColorSpace(gr::Material_GLTF::TextureSlotIdx::Occlusion))),
				m_srcMaterial->occlusion_texture.texcoord);

			// EmissiveTex
			newMat->SetTexture(
				gr::Material_GLTF::TextureSlotIdx::Emissive,
				newMatHandle.AddDependency(LoadGLTFTextureOrColor(
					m_inventory,
					m_data,
					m_sceneRootPath,
					m_srcMaterial->emissive_texture.texture,
					k_defaultTextureColor,
					gr::Material_GLTF::GetDefaultTextureFormat(gr::Material_GLTF::TextureSlotIdx::Emissive),
					gr::Material_GLTF::GetDefaultTextureColorSpace(gr::Material_GLTF::TextureSlotIdx::Emissive))),
				m_srcMaterial->emissive_texture.texcoord);


			gr::Material_GLTF* newGLTFMat = newMat->GetAs<gr::Material_GLTF*>();

			newGLTFMat->SetBaseColorFactor(glm::make_vec4(m_srcMaterial->pbr_metallic_roughness.base_color_factor));
			newGLTFMat->SetMetallicFactor(m_srcMaterial->pbr_metallic_roughness.metallic_factor);
			newGLTFMat->SetRoughnessFactor(m_srcMaterial->pbr_metallic_roughness.roughness_factor);
			newGLTFMat->SetNormalScale(m_srcMaterial->normal_texture.texture ? m_srcMaterial->normal_texture.scale : 1.0f);
			newGLTFMat->SetOcclusionStrength(
				m_srcMaterial->occlusion_texture.texture ? m_srcMaterial->occlusion_texture.scale : 1.0f);

			newGLTFMat->SetEmissiveFactor(glm::make_vec3(m_srcMaterial->emissive_factor));
			newGLTFMat->SetEmissiveStrength(
				m_srcMaterial->has_emissive_strength ? m_srcMaterial->emissive_strength.emissive_strength : 1.0f);

			switch (m_srcMaterial->alpha_mode)
			{
			case cgltf_alpha_mode::cgltf_alpha_mode_opaque:
			{
				newGLTFMat->SetAlphaMode(gr::Material::AlphaMode::Opaque);
				newGLTFMat->SetShadowCastMode(true);
			}
			break;
			case cgltf_alpha_mode::cgltf_alpha_mode_mask:
			{
				newGLTFMat->SetAlphaMode(gr::Material::AlphaMode::Mask);
				newGLTFMat->SetShadowCastMode(true);
			}
			break;
			case cgltf_alpha_mode::cgltf_alpha_mode_blend:
			{
				newGLTFMat->SetAlphaMode(gr::Material::AlphaMode::Blend);
				newGLTFMat->SetShadowCastMode(false);
			}
			break;
			}

			newGLTFMat->SetAlphaCutoff(m_srcMaterial->alpha_cutoff);
			newGLTFMat->SetDoubleSidedMode(m_srcMaterial->double_sided);

			return std::move(newMat);
		}


		core::Inventory* m_inventory = nullptr;

		std::string m_sceneRootPath;
		std::shared_ptr<cgltf_data const> m_data;
		cgltf_material const* m_srcMaterial = nullptr;

		std::string m_matName;
	};


	template<typename T>
	struct DefaultMaterialLoadContext_GLTF final : public virtual core::ILoadContext<gr::Material>
	{
		void OnLoadBegin(core::InvPtr<gr::Material>&) override
		{
			LOG("Generating a default GLTF pbrMetallicRoughness material \"%s\"...",
				en::DefaultResourceNames::k_defaultGLTFMaterialName);
		}

		std::unique_ptr<gr::Material> Load(core::InvPtr<gr::Material>& newMat) override
		{
			// Default error material:
			std::unique_ptr<gr::Material> defaultMaterialGLTF(
				new gr::Material_GLTF(en::DefaultResourceNames::k_defaultGLTFMaterialName));

			constexpr uint8_t k_defaultUVChannelIdx = 0;

			const re::Texture::TextureParams defaultSRGBTexParams{
				.m_width = 1,
				.m_height = 1,
				.m_usage = re::Texture::Usage::ColorSrc,
				.m_dimension = re::Texture::Dimension::Texture2D,
				.m_format = re::Texture::Format::RGBA8_UNORM,
				.m_colorSpace = re::Texture::ColorSpace::sRGB,
				.m_mipMode = re::Texture::MipMode::None,
				.m_createAsPermanent = true,
			};

			const re::Texture::TextureParams defaultLinearTexParams{
				.m_width = 1,
				.m_height = 1,
				.m_usage = re::Texture::Usage::ColorSrc,
				.m_dimension = re::Texture::Dimension::Texture2D,
				.m_format = re::Texture::Format::RGBA8_UNORM,
				.m_colorSpace = re::Texture::ColorSpace::sRGB,
				.m_mipMode = re::Texture::MipMode::None,
				.m_createAsPermanent = true,
			};

			// BaseColorTex
			defaultMaterialGLTF->SetTexture(gr::Material_GLTF::TextureSlotIdx::BaseColor,
				newMat.AddDependency(re::Texture::Create(
					en::DefaultResourceNames::k_defaultAlbedoTexName,
					defaultSRGBTexParams,
					glm::vec4(1.f))),
				k_defaultUVChannelIdx);

			// MetallicRoughnessTex
			defaultMaterialGLTF->SetTexture(gr::Material_GLTF::TextureSlotIdx::MetallicRoughness,
				newMat.AddDependency(re::Texture::Create(
					en::DefaultResourceNames::k_defaultMetallicRoughnessTexName,
					defaultLinearTexParams,
					glm::vec4(0.f, 1.f, 1.f, 0.f))), // GLTF specs: .BG = metalness, roughness, Default: .BG = 1, 1
				k_defaultUVChannelIdx);

			// NormalTex
			defaultMaterialGLTF->SetTexture(gr::Material_GLTF::TextureSlotIdx::Normal,
				newMat.AddDependency(re::Texture::Create(
					en::DefaultResourceNames::k_defaultNormalTexName,
					defaultLinearTexParams,
					glm::vec4(0.5f, 0.5f, 1.f, 0.f))),
				k_defaultUVChannelIdx);

			// OcclusionTex
			defaultMaterialGLTF->SetTexture(gr::Material_GLTF::TextureSlotIdx::Occlusion,
				newMat.AddDependency(re::Texture::Create(
					en::DefaultResourceNames::k_defaultOcclusionTexName,
					defaultLinearTexParams,
					glm::vec4(1.f))),
				k_defaultUVChannelIdx);

			// EmissiveTex
			defaultMaterialGLTF->SetTexture(gr::Material_GLTF::TextureSlotIdx::Emissive,
				newMat.AddDependency(re::Texture::Create(
					en::DefaultResourceNames::k_defaultEmissiveTexName,
					defaultSRGBTexParams,
					glm::vec4(0.f))),
				k_defaultUVChannelIdx);

			return std::move(defaultMaterialGLTF);
		}
	};


	void SetGLTFTransformValues(fr::EntityManager* em, cgltf_node const* current, entt::entity sceneNode)
	{
		SEAssert((current->has_matrix != (current->has_rotation || current->has_scale || current->has_translation)) ||
			(current->has_matrix == 0 && current->has_rotation == 0 &&
				current->has_scale == 0 && current->has_translation == 0),
			"Transform has both matrix and decomposed properties");

		SEAssert(em->HasComponent<fr::TransformComponent>(sceneNode), "Entity does not have a TransformComponent");

		fr::Transform& targetTransform = em->GetComponent<fr::TransformComponent>(sceneNode).GetTransform();

		if (current->has_matrix)
		{
			const glm::mat4 nodeModelMatrix = glm::make_mat4(current->matrix);
			glm::vec3 scale;
			glm::quat rotation;
			glm::vec3 translation;
			glm::vec3 skew;
			glm::vec4 perspective;
			glm::decompose(nodeModelMatrix, scale, rotation, translation, skew, perspective);

			targetTransform.SetLocalRotation(rotation);
			targetTransform.SetLocalScale(scale);
			targetTransform.SetLocalPosition(translation);
		}
		else
		{
			if (current->has_scale)
			{
				targetTransform.SetLocalScale(glm::vec3(current->scale[0], current->scale[1], current->scale[2]));
			}
			if (current->has_rotation)
			{
				// Note: GLM expects quaternions to be specified in WXYZ order
				targetTransform.SetLocalRotation(
					glm::quat(current->rotation[3], current->rotation[0], current->rotation[1], current->rotation[2]));
			}
			if (current->has_translation)
			{
				targetTransform.SetLocalPosition(
					glm::vec3(current->translation[0], current->translation[1], current->translation[2]));
			}
		}
	};


	inline entt::entity CreateGLTFSceneNode(
		fr::EntityManager* em,
		std::shared_ptr<FileMetadata> const& fileMetadata,
		cgltf_node const* gltfNode,
		entt::entity parent,
		size_t nodeIdx)
	{
		std::string const& nodeName = GenerateGLTFNodeName(fileMetadata, gltfNode, nodeIdx);

		entt::entity newSceneNode = fr::SceneNode::Create(*em, nodeName, parent);

		// We ensure there is a Transform (even just the identity) for all skeleton nodes
		bool isSkeletonNode = false;
		{
			std::lock_guard<std::mutex> lock(fileMetadata->m_skinDataMutex);
			isSkeletonNode = fileMetadata->m_skeletonNodes.contains(gltfNode);
		}

		if (gltfNode->has_translation ||
			gltfNode->has_rotation ||
			gltfNode->has_scale ||
			gltfNode->has_matrix ||
			isSkeletonNode)
		{
			fr::TransformComponent::AttachTransformComponent(*em, newSceneNode);
			SetGLTFTransformValues(em, gltfNode, newSceneNode);
		}

		return newSceneNode;
	}


	void LoadAddGLTFCamera(
		fr::EntityManager* em,
		cgltf_node const* current,
		size_t nodeIdx,
		entt::entity sceneNodeEntity,		
		std::shared_ptr<FileMetadata>& fileMetadata)
	{
		SEAssert(current != nullptr, "Null cgltf_node");
		SEAssert(sceneNodeEntity != entt::null, "Null scene node entity");
		SEAssert(current->camera != nullptr, "Must supply a scene node that has a camera");

		cgltf_camera const* camera = current->camera;

		std::string const& camName = GenerateGLTFCameraName(fileMetadata, camera, nodeIdx);
		LOG("Loading camera \"%s\"", camName.c_str());

		gr::Camera::Config camConfig;
		camConfig.m_projectionType = camera->type == cgltf_camera_type_orthographic ?
			gr::Camera::Config::ProjectionType::Orthographic : gr::Camera::Config::ProjectionType::Perspective;
		if (camConfig.m_projectionType == gr::Camera::Config::ProjectionType::Orthographic)
		{
			camConfig.m_yFOV = 0;
			camConfig.m_near = camera->data.orthographic.znear;
			camConfig.m_far = camera->data.orthographic.zfar;
			camConfig.m_orthoLeftRightBotTop.x = -camera->data.orthographic.xmag / 2.0f;
			camConfig.m_orthoLeftRightBotTop.y = camera->data.orthographic.xmag / 2.0f;
			camConfig.m_orthoLeftRightBotTop.z = -camera->data.orthographic.ymag / 2.0f;
			camConfig.m_orthoLeftRightBotTop.w = camera->data.orthographic.ymag / 2.0f;
		}
		else
		{
			camConfig.m_yFOV = camera->data.perspective.yfov;
			camConfig.m_near = camera->data.perspective.znear;
			camConfig.m_far = camera->data.perspective.zfar;
			camConfig.m_aspectRatio =
				camera->data.perspective.has_aspect_ratio ? camera->data.perspective.aspect_ratio : 1.0f;
			camConfig.m_orthoLeftRightBotTop.x = 0.f;
			camConfig.m_orthoLeftRightBotTop.y = 0.f;
			camConfig.m_orthoLeftRightBotTop.z = 0.f;
			camConfig.m_orthoLeftRightBotTop.w = 0.f;
		}

		// Create the camera and set the transform values on the parent object:
		fr::CameraComponent::CreateCameraConcept(*em, sceneNodeEntity, camName.c_str(), camConfig);

		// Update the camera metadata:
		{
			std::lock_guard<std::mutex> lock(fileMetadata->m_cameraMetadataMutex);

			fileMetadata->m_cameraMetadata.emplace_back(load::CameraMetadata{
				.m_srcNodeIdx = nodeIdx,
				.m_owningEntity = sceneNodeEntity, });
		}
	}


	void LoadAddGLTFLight(
		fr::EntityManager* em, 
		cgltf_node const* current, 
		size_t nodeIdx,
		entt::entity sceneNode,
		std::shared_ptr<FileMetadata> const& fileMetadata)
	{
		SEAssert(current && current->light, "Invalid light node");

		std::string const& lightName = GenerateGLTFLightName(fileMetadata, current->light, nodeIdx);

		LOG("Found light \"%s\"", lightName.c_str());

		// For now we always attach a shadow and let light graphics systems decide to render it or not
		const bool attachShadow = true;

		const glm::vec4 colorIntensity(
			current->light->color[0],
			current->light->color[1],
			current->light->color[2],
			current->light->intensity);

		// The GLTF 2.0 KHR_lights_punctual extension supports directional, point, and spot light types
		switch (current->light->type)
		{
		case cgltf_light_type::cgltf_light_type_directional:
		{
			fr::LightComponent::AttachDeferredDirectionalLightConcept(
				*em, sceneNode, lightName, colorIntensity, attachShadow);
		}
		break;
		case cgltf_light_type::cgltf_light_type_point:
		{
			fr::LightComponent::AttachDeferredPointLightConcept(*em, sceneNode, lightName, colorIntensity, attachShadow);
		}
		break;
		case cgltf_light_type::cgltf_light_type_spot:
		{
			fr::LightComponent::AttachDeferredSpotLightConcept(*em, sceneNode, lightName, colorIntensity, attachShadow);
		}
		break;
		case cgltf_light_type::cgltf_light_type_invalid:
		case cgltf_light_type::cgltf_light_type_max_enum:
		default:
			SEAssertF("Invalid light type");
		}
	}


	template<typename T>
	struct MeshPrimitiveFromCGLTF final : public virtual core::ILoadContext<gr::MeshPrimitive>
	{
		std::unique_ptr<gr::MeshPrimitive> Load(core::InvPtr<gr::MeshPrimitive>& newMeshPrimHandle) override
		{
			// Populate the mesh params:
			const gr::MeshPrimitive::MeshPrimitiveParams meshPrimitiveParams{
				.m_primitiveTopology = CGLTFPrimitiveTypeToPrimitiveTopology(m_srcPrimitive->type),
			};

			// Vertex streams:
			// Each vector element corresponds to the m_setIdx of the entries in the array elements
			std::vector<std::array<gr::VertexStream::CreateParams,
				gr::VertexStream::Type_Count>> vertexStreamCreateParams;

			auto AddVertexStreamCreateParams = [&vertexStreamCreateParams](
				gr::VertexStream::CreateParams&& streamCreateParams)
				{
					// Insert enough elements to make our set index valid:
					while (vertexStreamCreateParams.size() <= streamCreateParams.m_setIdx)
					{
						vertexStreamCreateParams.emplace_back();
					}

					const size_t streamTypeIdx = static_cast<size_t>(streamCreateParams.m_streamDesc.m_type);

					SEAssert(vertexStreamCreateParams.at(
						streamCreateParams.m_setIdx)[streamTypeIdx].m_streamData == nullptr,
						"Stream data is not null, this suggests we've already populated this slot");

					vertexStreamCreateParams.at(streamCreateParams.m_setIdx)[streamTypeIdx] =
						std::move(streamCreateParams);
				};


			// Index stream:
			if (m_srcPrimitive->indices != nullptr)
			{
				const size_t indicesComponentNumBytes =
					cgltf_component_size(m_srcPrimitive->indices->component_type);
				SEAssert(indicesComponentNumBytes == 1 ||
					indicesComponentNumBytes == 2 ||
					indicesComponentNumBytes == 4,
					"Unexpected index component byte size");

				const size_t numIndices = cgltf_accessor_unpack_indices(
					m_srcPrimitive->indices, nullptr, indicesComponentNumBytes, m_srcPrimitive->indices->count);

				util::ByteVector indices = (indicesComponentNumBytes == 1 || indicesComponentNumBytes == 2) ?
					util::ByteVector::Create<uint16_t>(numIndices) : // We'll expand 8 -> 16 bits
					util::ByteVector::Create<uint32_t>(numIndices);

				re::DataType indexDataType = re::DataType::DataType_Count;
				switch (indicesComponentNumBytes)
				{
				case 1: // uint8_t -> uint16_t
				{
					// DX12 does not support 8 bit indices; Here we expand 8 -> 16 bits
					indexDataType = re::DataType::UShort;

					std::vector<uint8_t> tempIndices(numIndices);
					cgltf_accessor_unpack_indices(
						m_srcPrimitive->indices, tempIndices.data(), indicesComponentNumBytes, numIndices);

					for (size_t i = 0; i < tempIndices.size(); ++i)
					{
						indices.at<uint16_t>(i) = static_cast<uint16_t>(tempIndices[i]);
					}
				}
				break;
				case 2: // uint16_t
				{
					indexDataType = re::DataType::UShort;
					cgltf_accessor_unpack_indices(
						m_srcPrimitive->indices, indices.data<uint16_t>(), indicesComponentNumBytes, numIndices);
				}
				break;
				case 4: // uint32_t
				{
					indexDataType = re::DataType::UInt;
					cgltf_accessor_unpack_indices(
						m_srcPrimitive->indices, indices.data<uint32_t>(), indicesComponentNumBytes, numIndices);
				}
				break;
				default: SEAssertF("Unexpected number of bytes in indices component");
				}

				AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
					.m_streamData = std::make_unique<util::ByteVector>(std::move(indices)),
					.m_streamDesc = gr::VertexStream::StreamDesc{
						.m_type = gr::VertexStream::Type::Index,
						.m_dataType = indexDataType,
					},
					.m_setIdx = 0 // Index stream is always in set 0
					});
			}

			// Unpack each of the primitive's vertex attrbutes:
			for (size_t attrib = 0; attrib < m_srcPrimitive->attributes_count; attrib++)
			{
				cgltf_attribute const& curAttribute = m_srcPrimitive->attributes[attrib];

				const size_t numComponents = cgltf_num_components(curAttribute.data->type);

				// GLTF mesh vertex attributes are stored as vecN's only:
				// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes-overview
				SEAssert(numComponents <= 4, "Invalid vertex attribute data type");

				const size_t numElements = curAttribute.data->count;
				const size_t totalFloatElements = numComponents * numElements;

				const uint8_t setIdx = util::CheckedCast<uint8_t>(curAttribute.index);

				const cgltf_attribute_type vertexAttributeType = curAttribute.type;
				switch (vertexAttributeType)
				{
				case cgltf_attribute_type::cgltf_attribute_type_position:
				{
					util::ByteVector positions =
						util::ByteVector::Create<glm::vec3>(curAttribute.data->count);

					const bool unpackResult = cgltf_accessor_unpack_floats(
						curAttribute.data,
						static_cast<float*>(positions.data<float>()),
						totalFloatElements);
					SEAssert(unpackResult, "Failed to unpack data");

					SEAssert(vertexStreamCreateParams.empty() ||
						vertexStreamCreateParams[0][gr::VertexStream::Type::Position].m_streamData == nullptr,
						"Only a single position stream is supported");

					SEAssert(setIdx == 0, "Unexpected stream index for position stream");

					AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
						.m_streamData = std::make_unique<util::ByteVector>(std::move(positions)),
						.m_streamDesc = gr::VertexStream::StreamDesc{
							.m_type = gr::VertexStream::Type::Position,
							.m_dataType = re::DataType::Float3,
						},
						.m_setIdx = setIdx
						});
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_normal:
				{
					util::ByteVector normals = util::ByteVector::Create<glm::vec3>(curAttribute.data->count);

					const bool unpackResult = cgltf_accessor_unpack_floats(
						curAttribute.data,
						static_cast<float*>(normals.data<float>()),
						totalFloatElements);
					SEAssert(unpackResult, "Failed to unpack data");

					AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
						.m_streamData = std::make_unique<util::ByteVector>(std::move(normals)),
						.m_streamDesc = gr::VertexStream::StreamDesc{
							.m_type = gr::VertexStream::Type::Normal,
							.m_dataType = re::DataType::Float3,
							.m_doNormalize = gr::VertexStream::Normalize::True,
						},
						.m_setIdx = setIdx
						});
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_tangent:
				{
					util::ByteVector tangents = util::ByteVector::Create<glm::vec4>(curAttribute.data->count);

					const bool unpackResult = cgltf_accessor_unpack_floats(
						curAttribute.data,
						static_cast<float*>(tangents.data<float>()),
						totalFloatElements);
					SEAssert(unpackResult, "Failed to unpack data");

					AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
						.m_streamData = std::make_unique<util::ByteVector>(std::move(tangents)),
						.m_streamDesc = gr::VertexStream::StreamDesc{
							.m_type = gr::VertexStream::Type::Tangent,
							.m_dataType = re::DataType::Float4,
							.m_doNormalize = gr::VertexStream::Normalize::True,
						},
						.m_setIdx = setIdx
						});
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_texcoord:
				{
					util::ByteVector uvs = util::ByteVector::Create<glm::vec2>(curAttribute.data->count);

					const bool unpackResult = cgltf_accessor_unpack_floats(
						curAttribute.data,
						static_cast<float*>(uvs.data<float>()),
						totalFloatElements);
					SEAssert(unpackResult, "Failed to unpack data");

					AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
						.m_streamData = std::make_unique<util::ByteVector>(std::move(uvs)),
						.m_streamDesc = gr::VertexStream::StreamDesc{
							.m_type = gr::VertexStream::Type::TexCoord,
							.m_dataType = re::DataType::Float2,
						},
						.m_setIdx = setIdx
						});
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_color:
				{
					util::ByteVector colors = UnpackGLTFColorAttributeAsVec4(curAttribute);

					AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
							.m_streamData = std::make_unique<util::ByteVector>(std::move(colors)),
							.m_streamDesc = gr::VertexStream::StreamDesc{
								.m_type = gr::VertexStream::Type::Color,
								.m_dataType = re::DataType::Float4,
							},
							.m_setIdx = setIdx
						});
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_joints: // joints_n = indexes from skin.joints array
				{
					// GLTF specs: Max 4 joints (per set) can influence 1 vertex; Joints are stored as
					// vec4's of unsigned bytes/shorts
					util::ByteVector joints = util::ByteVector::Create<glm::vec4>(curAttribute.data->count);

					const bool unpackResult = cgltf_accessor_unpack_floats(
						curAttribute.data,
						static_cast<float*>(joints.data<float>()),
						totalFloatElements);
					SEAssert(unpackResult, "Failed to unpack data");

					AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
						.m_streamData = std::make_unique<util::ByteVector>(std::move(joints)),
						.m_streamDesc = gr::VertexStream::StreamDesc{
							.m_type = gr::VertexStream::Type::BlendIndices,
							.m_dataType = re::DataType::Float4,
						},
						.m_setIdx = setIdx
						});
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_weights:
				{
					// Weights are stored as vec4's of unsigned bytes/shorts
					util::ByteVector weights = util::ByteVector::Create<glm::vec4>(curAttribute.data->count);

					const bool unpackResult = cgltf_accessor_unpack_floats(
						curAttribute.data,
						static_cast<float*>(weights.data<float>()),
						totalFloatElements);
					SEAssert(unpackResult, "Failed to unpack data");

					AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
						.m_streamData = std::make_unique<util::ByteVector>(std::move(weights)),
						.m_streamDesc = gr::VertexStream::StreamDesc{
							 .m_type = gr::VertexStream::Type::BlendWeight,
							 .m_dataType = re::DataType::Float4,
						},
						.m_setIdx = setIdx
						});
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_custom:
				{
					SEAssertF("Custom vertex attributes are not (currently) supported");
				}
				break;
				case cgltf_attribute_type::cgltf_attribute_type_max_enum:
				case cgltf_attribute_type::cgltf_attribute_type_invalid:
				default:
					SEAssertF("Invalid attribute type");
				}
			} // End vertex attribute unpacking


			// Morph targets:
			auto AddMorphCreateParams = [&vertexStreamCreateParams](
				uint8_t setIdx, gr::VertexStream::Type streamType, gr::VertexStream::MorphData&& morphData)
				{
					SEAssert(setIdx < vertexStreamCreateParams.size(),
						"Trying to add a morph target to a vertex stream that does not exist");

					std::vector<gr::VertexStream::MorphData>& morphTargetData =
						vertexStreamCreateParams[setIdx][streamType].m_morphTargetData;

					morphTargetData.emplace_back(std::move(morphData));
				};

			for (size_t targetIdx = 0; targetIdx < m_srcPrimitive->targets_count; ++targetIdx)
			{
				cgltf_morph_target const& curTarget = m_srcPrimitive->targets[targetIdx];
				for (size_t targetAttribIdx = 0; targetAttribIdx < curTarget.attributes_count; ++targetAttribIdx)
				{
					cgltf_attribute const& curTargetAttribute = curTarget.attributes[targetAttribIdx];

					const size_t numTargetFloats =
						cgltf_accessor_unpack_floats(curTargetAttribute.data, nullptr, 0);

					const uint8_t targetStreamIdx = util::CheckedCast<uint8_t>(curTargetAttribute.index);

					cgltf_attribute_type targetAttributeType = curTargetAttribute.type;
					switch (targetAttributeType)
					{
					case cgltf_attribute_type::cgltf_attribute_type_position:
					{
						SEAssert(curTargetAttribute.data->type == cgltf_type::cgltf_type_vec3,
							"Unexpected data type");

						util::ByteVector posMorphData =
							util::ByteVector::Create<glm::vec3>(curTargetAttribute.data->count);

						const bool unpackResult = cgltf_accessor_unpack_floats(
							curTargetAttribute.data,
							static_cast<float*>(posMorphData.data<float>()),
							numTargetFloats);
						SEAssert(unpackResult, "Failed to unpack data");

						AddMorphCreateParams(
							targetStreamIdx,
							gr::VertexStream::Position,
							gr::VertexStream::MorphData{
								.m_displacementData = std::make_unique<util::ByteVector>(std::move(posMorphData)),
								.m_dataType = re::DataType::Float3 });
					}
					break;
					case cgltf_attribute_type::cgltf_attribute_type_normal:
					{
						SEAssert(curTargetAttribute.data->type == cgltf_type::cgltf_type_vec3,
							"Unexpected data type");

						util::ByteVector normalMorphData =
							util::ByteVector::Create<glm::vec3>(curTargetAttribute.data->count);

						const bool unpackResult = cgltf_accessor_unpack_floats(
							curTargetAttribute.data,
							static_cast<float*>(normalMorphData.data<float>()),
							numTargetFloats);
						SEAssert(unpackResult, "Failed to unpack data");

						AddMorphCreateParams(
							targetStreamIdx,
							gr::VertexStream::Normal,
							gr::VertexStream::MorphData{
								.m_displacementData = std::make_unique<util::ByteVector>(std::move(normalMorphData)),
								.m_dataType = re::DataType::Float3 });
					}
					break;
					case cgltf_attribute_type::cgltf_attribute_type_tangent:
					{
						// Note: Tangent morph targets are vec3's
						SEAssert(curTargetAttribute.data->type == cgltf_type::cgltf_type_vec3, "Unexpected data type");

						util::ByteVector tangentMorphData =
							util::ByteVector::Create<glm::vec3>(curTargetAttribute.data->count);

						const bool unpackResult = cgltf_accessor_unpack_floats(
							curTargetAttribute.data,
							static_cast<float*>(tangentMorphData.data<float>()),
							numTargetFloats);
						SEAssert(unpackResult, "Failed to unpack data");

						AddMorphCreateParams(
							targetStreamIdx,
							gr::VertexStream::Tangent,
							gr::VertexStream::MorphData{
								.m_displacementData = std::make_unique<util::ByteVector>(std::move(tangentMorphData)),
								.m_dataType = re::DataType::Float3 });
					}
					break;
					case cgltf_attribute_type::cgltf_attribute_type_texcoord:
					{
						SEAssert(curTargetAttribute.data->type == cgltf_type::cgltf_type_vec2, "Unexpected data type");

						util::ByteVector uvMorphData =
							util::ByteVector::Create<glm::vec2>(curTargetAttribute.data->count);

						const bool unpackResult = cgltf_accessor_unpack_floats(
							curTargetAttribute.data,
							static_cast<float*>(uvMorphData.data<float>()),
							numTargetFloats);
						SEAssert(unpackResult, "Failed to unpack data");

						AddMorphCreateParams(
							targetStreamIdx,
							gr::VertexStream::TexCoord,
							gr::VertexStream::MorphData{
								.m_displacementData = std::make_unique<util::ByteVector>(std::move(uvMorphData)),
								.m_dataType = re::DataType::Float2 });
					}
					break;
					case cgltf_attribute_type::cgltf_attribute_type_color:
					{
						SEAssert(curTargetAttribute.data->type == cgltf_type::cgltf_type_vec3 ||
							curTargetAttribute.data->type == cgltf_type::cgltf_type_vec4,
							"Unexpected data type");

						util::ByteVector morphColors = UnpackGLTFColorAttributeAsVec4(curTargetAttribute);

						AddMorphCreateParams(
							targetStreamIdx,
							gr::VertexStream::Color,
							gr::VertexStream::MorphData{
								.m_displacementData = std::make_unique<util::ByteVector>(std::move(morphColors)),
								.m_dataType = re::DataType::Float4 });
					}
					break;
					case cgltf_attribute_type::cgltf_attribute_type_joints:
					case cgltf_attribute_type::cgltf_attribute_type_weights:
					{
						SEAssertF("Invalid attribute type for morph target data");
					}
					break;
					case cgltf_attribute_type::cgltf_attribute_type_custom:
					{
						SEAssertF("Custom vertex attributes are not (currently) supported");
					}
					break;
					case cgltf_attribute_type::cgltf_attribute_type_max_enum:
					case cgltf_attribute_type::cgltf_attribute_type_invalid:
					default: SEAssertF("Invalid attribute type");
					}
				}
			}


			// Create empty containers for anything the VertexStreamBuilder can create.
			// Note: GLTF only supports a single position/normal/tangent (but multiple UV channels etc)
			const bool hasIndices =
				vertexStreamCreateParams[0][gr::VertexStream::Index].m_streamData != nullptr;
			const bool hasNormal0 =
				vertexStreamCreateParams[0][gr::VertexStream::Normal].m_streamData != nullptr;
			const bool hasTangent0 =
				vertexStreamCreateParams[0][gr::VertexStream::Tangent].m_streamData != nullptr;
			const bool hasUV0 =
				vertexStreamCreateParams[0][gr::VertexStream::TexCoord].m_streamData != nullptr;
			const bool hasColor = vertexStreamCreateParams[0][gr::VertexStream::Color].m_streamData != nullptr;

			if (!hasIndices)
			{
				const size_t numPositions =
					vertexStreamCreateParams[0][gr::VertexStream::Position].m_streamData->size();

				std::unique_ptr<util::ByteVector> indexData;
				re::DataType indexDataType = re::DataType::DataType_Count;
				if (numPositions < std::numeric_limits<uint16_t>::max())
				{
					indexData = std::make_unique<util::ByteVector>(util::ByteVector::Create<uint16_t>());
					indexDataType = re::DataType::UShort;
				}
				else
				{
					indexData = std::make_unique<util::ByteVector>(util::ByteVector::Create<uint32_t>());
					indexDataType = re::DataType::UInt;
				}

				AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
					.m_streamData = std::move(indexData),
					.m_streamDesc = gr::VertexStream::StreamDesc{
						.m_type = gr::VertexStream::Type::Index,
						.m_dataType = indexDataType,
					},
					.m_setIdx = 0,
					});
			}
			if (!hasNormal0)
			{
				AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
						.m_streamData = std::make_unique<util::ByteVector>(util::ByteVector::Create<glm::vec3>()),
						.m_streamDesc = gr::VertexStream::StreamDesc{
							.m_type = gr::VertexStream::Type::Normal,
							.m_dataType = re::DataType::Float3,
							.m_doNormalize = gr::VertexStream::Normalize::True,
						},
						.m_setIdx = 0,
					});
			}
			if (!hasTangent0)
			{
				AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
					.m_streamData = std::make_unique<util::ByteVector>(util::ByteVector::Create<glm::vec4>()),
					.m_streamDesc = gr::VertexStream::StreamDesc{
						.m_type = gr::VertexStream::Type::Tangent,
						.m_dataType = re::DataType::Float4,
						.m_doNormalize = gr::VertexStream::Normalize::True,
					},
					.m_setIdx = 0,
					});
			}
			if (!hasUV0)
			{
				AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
					.m_streamData = std::make_unique<util::ByteVector>(util::ByteVector::Create<glm::vec2>()),
					.m_streamDesc = gr::VertexStream::StreamDesc{
						.m_type = gr::VertexStream::Type::TexCoord,
						.m_dataType = re::DataType::Float2,
					},
					.m_setIdx = 0,
					});
			}
			if (!hasColor) // SE (currently) expects at least 1 color channel
			{
				const size_t numPositionVerts =
					vertexStreamCreateParams[0][gr::VertexStream::Position].m_streamData->size();

				AddVertexStreamCreateParams(gr::VertexStream::CreateParams{
					.m_streamData = std::make_unique<util::ByteVector>(
						util::ByteVector::Create<glm::vec4>(numPositionVerts, glm::vec4(1.f) /*= GLTF default*/)),
					.m_streamDesc = gr::VertexStream::StreamDesc{
							.m_type = gr::VertexStream::Type::Color,
							.m_dataType = re::DataType::Float4,
						},
					.m_setIdx = 0,
					});
			}

			// Assemble the data for the VertexStreamBuilder:					
			std::vector<util::ByteVector*> extraChannelsData;
			extraChannelsData.reserve(vertexStreamCreateParams.size());
			for (auto& streams : vertexStreamCreateParams)
			{
				for (auto& stream : streams)
				{
					if (stream.m_streamData == nullptr)
					{
						continue;
					}

					switch (stream.m_streamDesc.m_type)
					{
					case gr::VertexStream::Index:
					{
						SEAssert(stream.m_setIdx == 0, "Found an index stream beyond index 0. This is unexpected");
						continue;
					}
					break;
					case gr::VertexStream::Color:
					case gr::VertexStream::BlendIndices:
					case gr::VertexStream::BlendWeight:
					{
						extraChannelsData.emplace_back(stream.m_streamData.get());
					}
					break;
					case gr::VertexStream::TexCoord:
					case gr::VertexStream::Position:
					case gr::VertexStream::Normal:
					case gr::VertexStream::Tangent:
					{
						// Position0/Normal0/Tangent0/UV0 are handled elsewhere; But we do add their morph data below
						if (stream.m_setIdx > 0)
						{
							extraChannelsData.emplace_back(stream.m_streamData.get());
						}
					}
					break;
					case gr::VertexStream::Binormal:
					{
						SEAssertF("Binormal streams are nto supported by GLTF, this is unexpected");
					}
					break;
					default: SEAssertF("Invalid stream type");
					}

					// Add any morph target data
					if (!stream.m_morphTargetData.empty())
					{
						for (auto const& morphData : stream.m_morphTargetData)
						{
							extraChannelsData.emplace_back(morphData.m_displacementData.get());
						}
					}
				}
			}

			// If our Mesh is animated, add the structured flag to the animated vertex stream buffers
			if (m_meshHasMorphTargets || m_meshHasSkin)
			{
				for (auto& streamIndexElement : vertexStreamCreateParams)
				{
					for (auto& createParams : streamIndexElement)
					{
						if (createParams.m_streamDesc.m_type != gr::VertexStream::Index)
						{
							if (!createParams.m_morphTargetData.empty() ||
								(m_meshHasSkin &&
									(createParams.m_streamDesc.m_type == gr::VertexStream::Position ||
										createParams.m_streamDesc.m_type == gr::VertexStream::Normal ||
										createParams.m_streamDesc.m_type == gr::VertexStream::Tangent ||
										createParams.m_streamDesc.m_type == gr::VertexStream::BlendIndices ||
										createParams.m_streamDesc.m_type == gr::VertexStream::BlendWeight)))
							{
								createParams.m_extraUsageBits |= re::Buffer::Usage::Structured;
							}
						}
					}
				}
			}

			// Construct any missing vertex attributes for the mesh:
			grutil::VertexStreamBuilder::MeshData meshData
			{
				.m_name = m_meshName,
				.m_meshParams = &meshPrimitiveParams,
				.m_indices = vertexStreamCreateParams[0][gr::VertexStream::Index].m_streamData.get(),
				.m_indicesStreamDesc = &vertexStreamCreateParams[0][gr::VertexStream::Index].m_streamDesc,
				.m_positions = vertexStreamCreateParams[0][gr::VertexStream::Position].m_streamData.get(),
				.m_normals = vertexStreamCreateParams[0][gr::VertexStream::Normal].m_streamData.get(),
				.m_tangents = vertexStreamCreateParams[0][gr::VertexStream::Tangent].m_streamData.get(),
				.m_UV0 = vertexStreamCreateParams[0][gr::VertexStream::TexCoord].m_streamData.get(),
				.m_extraChannels = &extraChannelsData,
			};
			grutil::VertexStreamBuilder::BuildMissingVertexAttributes(&meshData);


			std::unique_ptr<gr::MeshPrimitive> newMeshPrimitive = std::unique_ptr<gr::MeshPrimitive>(new gr::MeshPrimitive(
				m_primitiveName.c_str(),
				std::move(vertexStreamCreateParams),
				meshPrimitiveParams));

			return newMeshPrimitive;

		} // Load()

		std::shared_ptr<FileMetadata> m_sceneMetadata;

		std::string m_meshName;
		std::string m_primitiveName;

		std::shared_ptr<cgltf_data const> m_data;
		cgltf_primitive const* m_srcPrimitive = nullptr;

		bool m_meshHasMorphTargets = false;
		bool m_meshHasSkin = false;
	};


	void LoadGLTFMeshData(
		core::Inventory* inventory,
		std::shared_ptr<cgltf_data const> const& data,
		std::shared_ptr<FileMetadata>& fileMetadata,
		core::InvPtr<GLTFSceneHandle>& gltfScene)
	{
		for (size_t meshIdx = 0; meshIdx < data->meshes_count; ++meshIdx)
		{
			cgltf_mesh const* curMesh = &data->meshes[meshIdx];

			std::string const& meshName = GenerateGLTFMeshName(fileMetadata, curMesh, meshIdx);

			// Parse the mesh in advance to determine if it has any animation:
			bool meshHasMorphTargets = false;
			bool meshHasSkin = false;
			for (size_t primIdx = 0; primIdx < curMesh->primitives_count; ++primIdx)
			{
				cgltf_primitive& curPrimitive = curMesh->primitives[primIdx];

				if (curPrimitive.targets_count > 0)
				{
					meshHasMorphTargets = true;
				}

				for (size_t attrib = 0; attrib < curPrimitive.attributes_count; ++attrib)
				{
					cgltf_attribute const& curAttribute = curPrimitive.attributes[attrib];
					const cgltf_attribute_type vertexAttributeType = curAttribute.type;
					if (vertexAttributeType == cgltf_attribute_type::cgltf_attribute_type_joints ||
						vertexAttributeType == cgltf_attribute_type::cgltf_attribute_type_weights)
					{
						meshHasSkin = true;
						break;
					}
				}

				if (meshHasMorphTargets && meshHasSkin)
				{
					break; // Nothing more to search for
				}
			}

			// Load each primitive:
			for (size_t primIdx = 0; primIdx < curMesh->primitives_count; ++primIdx)
			{
				std::string const& primitiveName = GenerateGLTFMeshPrimitiveName(fileMetadata, curMesh, meshIdx, primIdx);

				std::shared_ptr<MeshPrimitiveFromCGLTF<gr::MeshPrimitive>> loadContext =
					std::make_shared<MeshPrimitiveFromCGLTF<gr::MeshPrimitive>>();

				loadContext->m_sceneMetadata = fileMetadata;

				loadContext->m_meshName = meshName;
				loadContext->m_primitiveName = primitiveName;

				loadContext->m_data = data;
				loadContext->m_srcPrimitive = &curMesh->primitives[primIdx];

				loadContext->m_meshHasMorphTargets = meshHasMorphTargets;
				loadContext->m_meshHasSkin = meshHasSkin;

				// Update the mesh primitive metadata
				{
					std::lock_guard<std::mutex> lock(fileMetadata->m_primitiveToMeshPrimitiveMetadataMutex);

					// Note: We must dispatch this while the m_primitiveToMeshPrimitiveMetadataMutex is locked to
					// prevent a race condition where the async loading thread tries to access the metadata before we've
					// populated it

					// Load the MeshPrimitive as a dependency of the GLTF scene:
					MeshPrimitiveMetadata& meshPrimMetadata = fileMetadata->m_primitiveToMeshPrimitiveMetadata.emplace(
						&curMesh->primitives[primIdx],
						MeshPrimitiveMetadata{
							.m_meshPrimitive = gltfScene.AddDependency(inventory->Get(
								util::HashKey(primitiveName),
								static_pointer_cast<core::ILoadContext<gr::MeshPrimitive>>(loadContext))),
						}).first->second;

					// Load the Material and add it as a dependency of the MeshPrimitive:
					std::shared_ptr<MaterialLoadContext_GLTF<gr::Material_GLTF>> matLoadCtx =
						std::make_shared<MaterialLoadContext_GLTF<gr::Material_GLTF>>();

					matLoadCtx->m_inventory = inventory;
					matLoadCtx->m_sceneRootPath = fileMetadata->m_sceneRootPath;
					matLoadCtx->m_data = data;
					matLoadCtx->m_srcMaterial = curMesh->primitives[primIdx].material;
					matLoadCtx->m_matName = GenerateGLTFMaterialName(fileMetadata, curMesh->primitives[primIdx].material);

					meshPrimMetadata.m_material = meshPrimMetadata.m_meshPrimitive.AddDependency(inventory->Get(
						util::HashKey(matLoadCtx->m_matName),
						std::static_pointer_cast<core::ILoadContext<gr::Material>>(matLoadCtx)));
				}
			}
		}
	}


	inline void PreLoadGLTFSkinData(
		std::shared_ptr<cgltf_data const> const& data,
		std::shared_ptr<FileMetadata>& fileMetadata,
		std::vector<std::future<void>>& skinFutures)
	{
		for (size_t skinIdx = 0; skinIdx < data->skins_count; ++skinIdx)
		{
			cgltf_skin const* skin = &data->skins[skinIdx];

			skinFutures.emplace_back(core::ThreadPool::Get()->EnqueueJob(
				[skin, &fileMetadata]()
				{
					std::vector<glm::mat4> inverseBindMatrices;
					if (skin->inverse_bind_matrices)
					{
						const cgltf_size numFloats = cgltf_accessor_unpack_floats(skin->inverse_bind_matrices, nullptr, 0);

						constexpr size_t k_numfloatsPerMat4 = sizeof(glm::mat4) / sizeof(float);
						inverseBindMatrices.resize(numFloats / k_numfloatsPerMat4);

						cgltf_accessor_unpack_floats(
							skin->inverse_bind_matrices,
							&inverseBindMatrices[0][0].x,
							numFloats);

						{
							std::lock_guard<std::mutex> lock(fileMetadata->m_skinDataMutex);

							fileMetadata->m_skinToSkinMetadata.emplace(skin, std::move(inverseBindMatrices));
							fileMetadata->m_skeletonNodes.emplace(skin->skeleton);
						}
					}
				}));
		}
	}


	void PreLoadGLTFAnimationData(
		std::shared_ptr<cgltf_data const> const& data,
		std::shared_ptr<FileMetadata>& fileMetadata)
	{
		fileMetadata->m_animationController = fr::AnimationController::CreateAnimationControllerObject();

		for (uint64_t animIdx = 0; animIdx < data->animations_count; ++animIdx)
		{
			std::string animationName;
			if (data->animations[animIdx].name)
			{
				animationName = data->animations[animIdx].name;
			}
			else
			{
				static std::atomic<uint32_t> unnamedAnimationIdx = 0;
				animationName = std::format("UnnamedAnimation_{}", unnamedAnimationIdx.fetch_add(1));
			}
			LOG("Loading animation \"%s\"...", animationName.c_str());

			fileMetadata->m_animationController->AddNewAnimation(animationName.c_str());

			// Pack the Channels of an AnimationData struct:
			std::unordered_map<cgltf_node const*, fr::AnimationData>& nodeToData =
				fileMetadata->m_nodeToAnimationData.emplace_back();
			for (uint64_t channelIdx = 0; channelIdx < data->animations[animIdx].channels_count; ++channelIdx)
			{
				// GLTF animation samplers define an "input/output pair":
				// - A set of floating-point scalar values representing linear time in seconds
				// - A set of vectors or scalars representing the animated property
				// 
				// Note: The GLTF specifications also mandate that within 1 animation, each target (i.e. target node and
				// animation path) MUST NOT be used more than once
				// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#animations

				cgltf_animation_sampler const* animSampler = data->animations[animIdx].channels[channelIdx].sampler;

				// Get/create a new AnimationData structure:
				cgltf_node const* targetNode = data->animations[animIdx].channels[channelIdx].target_node;
				fr::AnimationData* animationData = nullptr;
				if (nodeToData.contains(targetNode))
				{
					animationData = &nodeToData.at(targetNode);
				}
				else
				{
					animationData = &nodeToData.emplace(targetNode, fr::AnimationData{}).first->second;
				}

				animationData->m_animationIdx = animIdx;

				// Create a new animation channel entry:
				fr::AnimationData::Channel& animChannel = animationData->m_channels.emplace_back();

				// Channel interpolation mode:
				animChannel.m_interpolationMode = CGLTFInterpolationTypeToFrInterpolationType(
					animSampler->interpolation,
					data->animations[animIdx].channels[channelIdx].target_path);

				// Channel target path:
				animChannel.m_targetPath =
					CGLTFAnimationPathToFrAnimationPath(data->animations[animIdx].channels[channelIdx].target_path);

				// Channel input data: (Linear keyframe times, in seconds)
				const cgltf_size numKeyframeTimeEntries = cgltf_accessor_unpack_floats(animSampler->input, nullptr, 0);

				std::vector<float> keyframeTimesSec(numKeyframeTimeEntries);
				cgltf_accessor_unpack_floats(animSampler->input, keyframeTimesSec.data(), numKeyframeTimeEntries);

				animChannel.m_keyframeTimesIdx =
					fileMetadata->m_animationController->AddChannelKeyframeTimes(animIdx, std::move(keyframeTimesSec));

				// Channel output data:
				const cgltf_size numOutputFloats = cgltf_accessor_unpack_floats(animSampler->output, nullptr, 0);

				std::vector<float> outputFloatData(numOutputFloats);
				cgltf_accessor_unpack_floats(animSampler->output, outputFloatData.data(), numOutputFloats);

				animChannel.m_dataIdx = fileMetadata->m_animationController->AddChannelData(std::move(outputFloatData));

				SEAssert(numOutputFloats % numKeyframeTimeEntries == 0,
					"The number of keyframe entries must be an exact multiple of the number of output floats");

				animChannel.m_dataFloatsPerKeyframe = util::CheckedCast<uint8_t>(numOutputFloats / numKeyframeTimeEntries);
			}
		}
	}


	inline void GetGLTFMinMaxXYZ(
		cgltf_primitive const& primitive, glm::vec3& positionsMinXYZOut, glm::vec3& positionsMaxXYZOut)
	{
		bool foundMin = false;
		bool foundMax = false;
		for (size_t attribIdx = 0; attribIdx < primitive.attributes_count; ++attribIdx)
		{
			if (primitive.attributes[attribIdx].type == cgltf_attribute_type::cgltf_attribute_type_position)
			{
				if (primitive.attributes[attribIdx].data->has_min)
				{
					memcpy(&positionsMinXYZOut.x, primitive.attributes[attribIdx].data->min, sizeof(glm::vec3));
					foundMin = true;
				}

				if (primitive.attributes[attribIdx].data->has_max)
				{
					memcpy(&positionsMaxXYZOut.x, primitive.attributes[attribIdx].data->max, sizeof(glm::vec3));
					foundMax = true;
				}

				if (!foundMin || !foundMax)
				{
					SEAssert(primitive.attributes[attribIdx].data->type == cgltf_type::cgltf_type_vec3,
						"Unexpected position data type");

					SEAssertF("TODO: If you hit this assert, this is the first time this code has been exercised. "
						"Sanity check it and delete this!");

					const uint8_t* element = cgltf_buffer_view_data(primitive.attributes[attribIdx].data->buffer_view);
					if (element != nullptr)
					{
						element += primitive.attributes[attribIdx].data->offset;

						const size_t numFloats = cgltf_accessor_unpack_floats(primitive.attributes[attribIdx].data, nullptr, 0);
						const size_t floatsPerElement = cgltf_num_components(primitive.attributes[attribIdx].data->type);
						const size_t numElements = numFloats / floatsPerElement;

						for (size_t i = 0; i < numElements; ++i)
						{
							glm::vec3 const* curPos = reinterpret_cast<glm::vec3 const*>(element);

							if (!foundMin)
							{
								positionsMinXYZOut.x = std::min(positionsMinXYZOut.x, curPos->x);
								positionsMinXYZOut.y = std::min(positionsMinXYZOut.y, curPos->y);
								positionsMinXYZOut.z = std::min(positionsMinXYZOut.z, curPos->z);
							}

							if (!foundMax)
							{
								positionsMaxXYZOut.x = std::max(positionsMaxXYZOut.x, curPos->x);
								positionsMaxXYZOut.y = std::max(positionsMaxXYZOut.y, curPos->y);
								positionsMaxXYZOut.z = std::max(positionsMaxXYZOut.z, curPos->z);
							}

							element += primitive.attributes[attribIdx].data->stride;
						}
					}
				}

				break; // We've inspected the position attribute, we're done!
			}
		}
	}


	inline void AttachGLTFGeometry(
		fr::EntityManager* em,
		cgltf_node const* current,
		size_t nodeIdx, // For default/fallback name
		entt::entity sceneNodeEntity,
		std::shared_ptr<FileMetadata>& fileMetadata)
	{
		SEAssert(current->mesh, "Current node does not have mesh data");

		std::string meshName;
		if (current->mesh->name)
		{
			meshName = current->mesh->name;
		}
		else
		{
			meshName = std::format("GLTFNode[{}]_Mesh", nodeIdx);
		}

		// Record the entities we know will have Bounds, we'll update them from any SkinningComponents
		std::vector<entt::entity> meshAndMeshPrimitiveEntities;
		meshAndMeshPrimitiveEntities.reserve(current->mesh->primitives_count + 1);

		fr::Mesh::AttachMeshConceptMarker(sceneNodeEntity, meshName.c_str());
		meshAndMeshPrimitiveEntities.emplace_back(sceneNodeEntity);

		// Add each MeshPrimitive as a child of the SceneNode's Mesh:
		for (size_t primIdx = 0; primIdx < current->mesh->primitives_count; primIdx++)
		{
			cgltf_primitive const& curPrimitive = current->mesh->primitives[primIdx];

			SEAssert(fileMetadata->m_primitiveToMeshPrimitiveMetadata.contains(&curPrimitive),
				"Failed to find the primitive in our metadata map. This is unexpected");

			// Parse the min/max positions for our Bounds:
			glm::vec3 positionsMinXYZ = fr::BoundsComponent::k_invalidMinXYZ;
			glm::vec3 positionsMaxXYZ = fr::BoundsComponent::k_invalidMaxXYZ;
			GetGLTFMinMaxXYZ(curPrimitive, positionsMinXYZ, positionsMaxXYZ);

			// Note: No locks here, the work should have already finished and been waited on
			MeshPrimitiveMetadata& meshPrimMetadata =
				fileMetadata->m_primitiveToMeshPrimitiveMetadata.at(&curPrimitive);

			// Attach the MeshPrimitive to the MeshConcept:
			const entt::entity meshPrimimitiveEntity = fr::MeshPrimitiveComponent::CreateMeshPrimitiveConcept(
				*em,
				sceneNodeEntity,
				meshPrimMetadata.m_meshPrimitive,
				positionsMinXYZ,
				positionsMaxXYZ);

			meshAndMeshPrimitiveEntities.emplace_back(meshPrimimitiveEntity);

			// Attach the MaterialInstanceComponent to the MeshPrimitive:
			fr::MaterialInstanceComponent::AttachMaterialComponent(*em, meshPrimimitiveEntity, meshPrimMetadata.m_material);
		} // primitives loop

		// Store our Mesh entity -> vector of Mesh/MeshPrimive Bounds entities:
		{
			std::unique_lock<std::mutex> lock(fileMetadata->m_meshEntityToBoundsEntityMapMutex);
			fileMetadata->m_meshEntityToBoundsEntityMap[sceneNodeEntity] = std::move(meshAndMeshPrimitiveEntities);
		}
	}


	void AttachGLTFMeshAnimationComponents(
		fr::EntityManager* em,
		std::shared_ptr<cgltf_data const> const& data,
		std::shared_ptr<FileMetadata>& fileMetadata)
	{
		// Move our pre-populated AnimationController into an entity/component so we can obtain its final pointer:
		fr::AnimationController* animationController = fr::AnimationController::CreateAnimationController(
			*em, GenerateGLTFAnimationControllerName(fileMetadata).c_str(), std::move(fileMetadata->m_animationController));

		for (size_t nodeIdx = 0; nodeIdx < data->nodes_count; ++nodeIdx)
		{
			cgltf_node const* current = &data->nodes[nodeIdx];
			const entt::entity curSceneNodeEntity = fileMetadata->m_nodeToEntity.at(current);

			// Morph targets:
			bool meshHasWeights = false;
			if (current->mesh)
			{
				bool meshHasMorphTargets = false;
				for (size_t primIdx = 0; primIdx < current->mesh->primitives_count; primIdx++)
				{
					cgltf_primitive const& curPrimitive = current->mesh->primitives[primIdx];
					if (curPrimitive.targets_count > 0)
					{
						meshHasMorphTargets = true;
						break;
					}
				}

				if (meshHasMorphTargets)
				{
					float const* weights = current->weights;
					size_t weightsCount = current->weights_count;
					if (!weights)
					{
						// GLTF specs: The default target mesh.weights is optional, and must be used when node.weights is null
						weights = current->mesh->weights;
						weightsCount = current->mesh->weights_count;
					}

					meshHasWeights = weightsCount > 0;

					fr::MeshMorphComponent::AttachMeshMorphComponent(
						*em,
						curSceneNodeEntity,
						current->mesh->weights,
						util::CheckedCast<uint32_t>(current->mesh->weights_count));
				}
			}

			// Skinning:
			if (current->skin)
			{
				// Build our joint index to TransformID mapping table:
				std::vector<gr::TransformID> jointToTransformIDs;
				jointToTransformIDs.reserve(current->skin->joints_count);

				std::vector<entt::entity> jointEntities;
				jointEntities.reserve(current->skin->joints_count);

				for (size_t jointIdx = 0; jointIdx < current->skin->joints_count; ++jointIdx)
				{
					SEAssert(fileMetadata->m_nodeToEntity.contains(current->skin->joints[jointIdx]),
						"Node is not in the node to entity map. This should not be possible");

					const entt::entity jointNodeEntity =
						fileMetadata->m_nodeToEntity.at(current->skin->joints[jointIdx]);

					jointEntities.emplace_back(jointNodeEntity);

					fr::TransformComponent const* transformCmpt =
						em->TryGetComponent<fr::TransformComponent>(jointNodeEntity);

					// GLTF Specs: Animated nodes can only have TRS properties (no matrix)
					if (transformCmpt && !current->skin->joints[jointIdx]->has_matrix)
					{
						jointToTransformIDs.emplace_back(transformCmpt->GetTransformID());
					}
					else
					{
						jointToTransformIDs.emplace_back(gr::k_invalidTransformID);
					}
				}

				// We pre-loaded the skinning data
				std::vector<glm::mat4>* inverseBindMatrices = nullptr;
				if (fileMetadata->m_skinToSkinMetadata.contains(current->skin))
				{
					// Note: No locks here, the work should have already finished and been waited on
					inverseBindMatrices =
						&fileMetadata->m_skinToSkinMetadata.at(current->skin).m_inverseBindMatrices;
				}

				// The skeleton root node is part of the skeletal hierarchy
				entt::entity skeletonRootEntity = entt::null;
				gr::TransformID skeletonTransformID = gr::k_invalidTransformID;
				if (fileMetadata->m_nodeToEntity.contains(current->skin->skeleton))
				{
					skeletonRootEntity = fileMetadata->m_nodeToEntity.at(current->skin->skeleton);

					// Note: The entity associated with the skeleton node might not be the entity with the next 
					// TransformationComponent in the hierarchy above; it might be modified here
					fr::Relationship const& skeletonRootRelationship = em->GetComponent<fr::Relationship>(skeletonRootEntity);
					fr::TransformComponent const* skeletonTransformCmpt =
						skeletonRootRelationship.GetFirstAndEntityInHierarchyAbove<fr::TransformComponent>(skeletonRootEntity);
					if (skeletonTransformCmpt)
					{
						skeletonTransformID = skeletonTransformCmpt->GetTransformID();
					}
				}

				fr::SkinningComponent::AttachSkinningComponent(
					curSceneNodeEntity,
					std::move(jointToTransformIDs),
					std::move(jointEntities),
					inverseBindMatrices ? std::move(*inverseBindMatrices) : std::vector<glm::mat4>(),
					skeletonRootEntity,
					skeletonTransformID,
					animationController->GetActiveLongestChannelTimeSec(),
					std::move(fileMetadata->m_meshEntityToBoundsEntityMap.at(curSceneNodeEntity)));
			}


			// AnimationComponents (transform/weight animation):
			bool hasAnimation = meshHasWeights;
			if (!hasAnimation) // Also need to search the node animations
			{
				for (auto const& animation : fileMetadata->m_nodeToAnimationData)
				{
					if (animation.contains(current))
					{
						hasAnimation = true;
						break;
					}
				}
			}

			if (hasAnimation)
			{
				SEAssert((current->weights == nullptr && (!current->mesh || !current->mesh->weights)) ||
					(current->weights && current->weights_count > 0) ||
					(current->mesh && current->mesh->weights && current->mesh->weights_count > 0),
					"Mesh weights count is non-zero, but weights is null");

				SEAssert(fileMetadata->m_animationController == nullptr &&
					animationController != nullptr,
					"m_animationController should have already been moved, finalAnimationController cannot be null");

				SEAssert(!em->HasComponent<fr::AnimationComponent>(curSceneNodeEntity), "Node already has an animation component");

				fr::AnimationComponent* animationCmpt = fr::AnimationComponent::AttachAnimationComponent(
					*em, curSceneNodeEntity, animationController);

				// Attach each/all animations that target the current node to its animation component:
				for (auto const& animation : fileMetadata->m_nodeToAnimationData)
				{
					if (!animation.contains(current))
					{
						continue;
					}

					animationCmpt->SetAnimationData(animation.at(current));
				}
			}
		} // nodeIdx
	}


	void AttachGLTFNodeComponents(
		fr::EntityManager* em,
		std::shared_ptr<cgltf_data const> const& data,
		std::shared_ptr<FileMetadata>& fileMetadata)
	{
		for (size_t nodeIdx = 0; nodeIdx < data->nodes_count; ++nodeIdx)
		{
			cgltf_node const* current = &data->nodes[nodeIdx];

			SEAssert(fileMetadata->m_nodeToEntity.contains(current),
				"Node to entity map does not contain the current node. This should not be possible");

			const entt::entity curSceneNodeEntity = fileMetadata->m_nodeToEntity.at(current);

			if (current->mesh)
			{
				AttachGLTFGeometry(em, current, nodeIdx, curSceneNodeEntity, fileMetadata);
			}
			if (current->light)
			{
				LoadAddGLTFLight(em, current, nodeIdx, curSceneNodeEntity, fileMetadata);
			}
			if (current->camera)
			{
				LoadAddGLTFCamera(em, current, nodeIdx, curSceneNodeEntity, fileMetadata);
			}
		}
	}


	void CreateGLTFSceneNodeEntities(
		fr::EntityManager* em,
		std::shared_ptr<cgltf_data const> const& data,
		std::shared_ptr<FileMetadata>& fileMetadata)
	{
		for (size_t sceneIdx = 0; sceneIdx < data->scenes_count; ++sceneIdx)
		{
			// Create our scene node entity hierarchy with a DFS traversal starting from each root node of the GLTF scene
			std::stack<cgltf_node const*> nodes;
			for (size_t nodeIdx = 0; nodeIdx < data->scenes[sceneIdx].nodes_count; ++nodeIdx)
			{
				if (data->scenes[sceneIdx].nodes[nodeIdx]->parent == nullptr)
				{
					nodes.emplace(data->scenes[sceneIdx].nodes[nodeIdx]);
				}
			}

			size_t nodeIdx = 0; // So we can label any unnamed nodes
			while (!nodes.empty())
			{
				cgltf_node const* curNode = nodes.top();
				nodes.pop();

				// Get our parent entity:
				entt::entity curNodeParentEntity = entt::null;
				if (curNode->parent)
				{
					SEAssert(fileMetadata->m_nodeToEntity.contains(curNode->parent),
						"Failed to find the parent, this should not be possible");

					curNodeParentEntity = fileMetadata->m_nodeToEntity.at(curNode->parent);
				}

				// Create the current node's entity (and Transform, if it has one):
				fileMetadata->m_nodeToEntity.emplace(
					curNode,
					CreateGLTFSceneNode(em, fileMetadata, curNode, curNodeParentEntity, nodeIdx++));

				// Add the children:
				for (size_t childIdx = 0; childIdx < curNode->children_count; ++childIdx)
				{
					nodes.emplace(curNode->children[childIdx]);
				}
			}
		}
	}


	template<typename T>
	struct GLTFFileLoadContext final : public virtual core::ILoadContext<GLTFSceneHandle>
	{
		void OnLoadBegin(core::InvPtr<GLTFSceneHandle>&) override
		{
			LOG("Loading GLTF scene from \"%s\"", m_filePath.c_str());
		}

		std::unique_ptr<GLTFSceneHandle> Load(core::InvPtr<GLTFSceneHandle>& gltfScene) override
		{
			// Parse the the GLTF metadata:
			const bool gotFilePath = !m_filePath.empty();
			cgltf_options options = { (cgltf_file_type)0 };
			if (gotFilePath)
			{
				cgltf_data* rawData = nullptr;
				cgltf_result parseResult = cgltf_parse_file(&options, m_filePath.c_str(), &rawData);
				if (parseResult != cgltf_result::cgltf_result_success)
				{
					SEAssert(parseResult == cgltf_result_success, "Failed to parse scene file \"%s\"", m_filePath.c_str());
					return nullptr;
				}

				m_sceneData = std::shared_ptr<cgltf_data>(rawData);
				rawData = nullptr;
			}

			// FileMetadata is populated with tracking data as we go
			m_sceneMetadata = std::make_shared<FileMetadata>();
			m_sceneMetadata->m_filePath = m_filePath;
			m_sceneMetadata->m_sceneRootPath = util::ExtractDirectoryPathFromFilePath(m_filePath);

			cgltf_data* data = m_sceneData ? m_sceneData.get() : nullptr;

			// Load the GLTF data:
			if (data)
			{
				cgltf_result bufferLoadResult = cgltf_load_buffers(&options, data, m_filePath.c_str());
				if (bufferLoadResult != cgltf_result::cgltf_result_success)
				{
					SEAssert(bufferLoadResult == cgltf_result_success, "Failed to load scene data \"%s\"", m_filePath.c_str());
					return nullptr;
				}

#if defined(_DEBUG)
				cgltf_result validationResult = cgltf_validate(data);
				if (validationResult != cgltf_result::cgltf_result_success)
				{
					SEAssert(validationResult == cgltf_result_success, "GLTF file failed validation!");
					return nullptr;
				}
#endif

				LoadGLTFMeshData(m_inventory, m_sceneData, m_sceneMetadata, gltfScene);

				std::vector<std::future<void>> loadFutures;
				PreLoadGLTFSkinData(m_sceneData, m_sceneMetadata, loadFutures);

				PreLoadGLTFAnimationData(m_sceneData, m_sceneMetadata); // Single-threaded while everything else loads

				// Wait for the async creation tasks to be done:
				for (auto const& loadFuture : loadFutures)
				{
					loadFuture.wait();
				}
				loadFutures.clear();
			}

			// Return this dummy object to satisfy the InvPtr
			return std::make_unique<GLTFSceneHandle>();
		}

		void OnLoadComplete(core::InvPtr<GLTFSceneHandle>& gltfScene)
		{
			SEAssert(m_sceneMetadata, "Scene metadata should not be null here");

			fr::EntityManager* em = fr::EntityManager::Get();
			std::shared_ptr<FileMetadata> fileMetadata = m_sceneMetadata;
			std::shared_ptr<cgltf_data> sceneData = m_sceneData;
			em->EnqueueEntityCommand(
				[em, sceneData, fileMetadata]() mutable
				{
					// Create scene node entities:
					CreateGLTFSceneNodeEntities(em, sceneData, fileMetadata);

					// Attach the components to the entities, now that they exist:
					AttachGLTFNodeComponents(em, sceneData, fileMetadata);

					// Animation components:
					AttachGLTFMeshAnimationComponents(em, sceneData, fileMetadata);
				});


			// Add a camera::
			em->EnqueueEntityCommand(
				[em, fileMetadata]() mutable
				{
					// Set the main camera:
					entt::entity mainCameraEntity = entt::null;
					{
						std::lock_guard<std::mutex> lock(fileMetadata->m_cameraMetadataMutex);

						if (!fileMetadata->m_cameraMetadata.empty())
						{
							// Sort our cameras for deterministic ordering
							std::sort(fileMetadata->m_cameraMetadata.begin(), fileMetadata->m_cameraMetadata.end(),
								[](load::CameraMetadata const& a, load::CameraMetadata const& b)
								{
									return a.m_srcNodeIdx < b.m_srcNodeIdx;
								});

							// Make the last camera loaded active
							mainCameraEntity = fileMetadata->m_cameraMetadata.back().m_owningEntity;
						}
					}

					// Finally, set the main camera:
					// TODO: It would be nice to not need to double-enqueue this
					if (mainCameraEntity != entt::null)
					{
						em->EnqueueEntityCommand<fr::SetMainCameraCommand>(mainCameraEntity);
					}
				});
		}


	private:
		std::shared_ptr<cgltf_data> m_sceneData;
		std::shared_ptr<FileMetadata> m_sceneMetadata;


	public:
		core::Inventory* m_inventory = nullptr;
		std::string m_filePath;
	};
}


namespace load
{
	void ImportGLTFFile(core::Inventory* inventory, std::string const& filePath)
	{
		SEAssert(!filePath.empty(), "Invalid file path");

		// GLTF does not support IBLs so we handle it manually by loading any HDRs placed alongside the GLTF file:
		std::string const& importIBLFilePath =
			util::ExtractDirectoryPathFromFilePath(filePath) + core::configkeys::k_perFileDefaultIBLRelFilePath;
		if (util::FileExists(importIBLFilePath))
		{
			// We let this go out of scope, but it'll register itself during OnLoadComplete()
			ImportIBL(inventory, importIBLFilePath, IBLTextureFromFilePath::ActivationMode::Always);
		}

		std::shared_ptr<GLTFFileLoadContext<GLTFSceneHandle>> loadContext =
			std::make_shared<GLTFFileLoadContext<GLTFSceneHandle>>();

		loadContext->m_inventory = inventory;
		loadContext->m_filePath = filePath;

		// We let this go out of scope, it'll clean up after itself once loading is done
		inventory->Get(
			util::HashKey(filePath),
			static_pointer_cast<core::ILoadContext<GLTFSceneHandle>>(loadContext));
	}


	void GenerateDefaultGLTFMaterial(core::Inventory* inventory)
	{
		std::shared_ptr<DefaultMaterialLoadContext_GLTF<gr::Material_GLTF>> loadContext =
			std::make_shared<DefaultMaterialLoadContext_GLTF<gr::Material_GLTF>>();

		loadContext->m_retentionPolicy = core::RetentionPolicy::Permanent;

		inventory->Get(
			util::HashKey(en::DefaultResourceNames::k_defaultGLTFMaterialName),
			static_pointer_cast<core::ILoadContext<gr::Material>>(loadContext));
	}
}