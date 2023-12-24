// © 2023 Adam Badke. All rights reserved.
#include "ParameterBlock.h"
#include "TransformRenderData.h"


namespace gr
{
	constexpr glm::vec3 Transform::WorldAxisX = glm::vec3(1.0f, 0.0f, 0.0f);
	constexpr glm::vec3 Transform::WorldAxisY = glm::vec3(0.0f, 1.0f, 0.0f);
	constexpr glm::vec3 Transform::WorldAxisZ = glm::vec3(0.0f, 0.0f, 1.0f); // Note: SaberEngine uses a RHCS


	std::shared_ptr<re::ParameterBlock> Transform::CreateInstancedTransformParams(
		glm::mat4 const* model, glm::mat4* transposeInvModel)
	{
		gr::Transform::InstancedTransformParams instancedMeshPBData;

		instancedMeshPBData.g_model = model ? *model : glm::mat4(1.f);
		instancedMeshPBData.g_transposeInvModel = transposeInvModel ? *transposeInvModel : glm::mat4(1.f);

		return re::ParameterBlock::CreateFromArray(
			gr::Transform::InstancedTransformParams::s_shaderName,
			&instancedMeshPBData,
			sizeof(gr::Transform::InstancedTransformParams),
			1,
			re::ParameterBlock::PBType::SingleFrame);
	}


	std::shared_ptr<re::ParameterBlock> Transform::CreateInstancedTransformParams(
		gr::Transform::RenderData const& renderData)
	{
		gr::Transform::InstancedTransformParams instancedMeshPBData{
			.g_model = renderData.g_model,
			.g_transposeInvModel = renderData.g_transposeInvModel
		};

		return re::ParameterBlock::CreateFromArray(
			gr::Transform::InstancedTransformParams::s_shaderName,
			&instancedMeshPBData,
			sizeof(gr::Transform::InstancedTransformParams),
			1,
			re::ParameterBlock::PBType::SingleFrame);
	}


	std::shared_ptr<re::ParameterBlock> Transform::CreateInstancedTransformParams(
		std::vector<gr::Transform::RenderData const*> const& transformRenderData)
	{
		const uint32_t numInstances = static_cast<uint32_t>(transformRenderData.size());

		std::vector<gr::Transform::InstancedTransformParams> instancedMeshPBData;
		instancedMeshPBData.reserve(numInstances);

		for (size_t transformIdx = 0; transformIdx < numInstances; transformIdx++)
		{
			instancedMeshPBData.emplace_back(InstancedTransformParams
				{
					.g_model = transformRenderData[transformIdx]->g_model,
					.g_transposeInvModel = transformRenderData[transformIdx]->g_transposeInvModel
				});
		}

		std::shared_ptr<re::ParameterBlock> instancedMeshParams = re::ParameterBlock::CreateFromArray(
			gr::Transform::InstancedTransformParams::s_shaderName,
			instancedMeshPBData.data(),
			sizeof(gr::Transform::InstancedTransformParams),
			numInstances,
			re::ParameterBlock::PBType::SingleFrame);

		return instancedMeshParams;
	}
}