// © 2023 Adam Badke. All rights reserved.
#include "CastUtils.h"
#include "ParameterBlock.h"
#include "TransformRenderData.h"


namespace gr
{
	constexpr glm::vec3 Transform::WorldAxisX = glm::vec3(1.0f, 0.0f, 0.0f);
	constexpr glm::vec3 Transform::WorldAxisY = glm::vec3(0.0f, 1.0f, 0.0f);
	constexpr glm::vec3 Transform::WorldAxisZ = glm::vec3(0.0f, 0.0f, 1.0f); // Note: SaberEngine uses a RHCS


	Transform::InstancedTransformParams Transform::CreateInstancedTransformParamsData(
		gr::Transform::RenderData const& transformData)
	{
		return gr::Transform::InstancedTransformParams {
			.g_model = transformData.g_model,
			.g_transposeInvModel = transformData.g_transposeInvModel
		};
	}

	std::shared_ptr<re::ParameterBlock> Transform::CreateInstancedTransformParams(
		re::ParameterBlock::PBType pbType, glm::mat4 const* model, glm::mat4* transposeInvModel)
	{
		gr::Transform::InstancedTransformParams instancedMeshPBData;

		instancedMeshPBData.g_model = model ? *model : glm::mat4(1.f);
		instancedMeshPBData.g_transposeInvModel = transposeInvModel ? *transposeInvModel : glm::mat4(1.f);

		return re::ParameterBlock::CreateArray(
			gr::Transform::InstancedTransformParams::s_shaderName,
			&instancedMeshPBData,
			1,
			pbType);
	}


	std::shared_ptr<re::ParameterBlock> Transform::CreateInstancedTransformParams(
		re::ParameterBlock::PBType pbType, gr::Transform::RenderData const& transformData)
	{
		gr::Transform::InstancedTransformParams const& instancedMeshPBData = 
			CreateInstancedTransformParamsData(transformData);

		return re::ParameterBlock::CreateArray(
			gr::Transform::InstancedTransformParams::s_shaderName,
			&instancedMeshPBData,
			1,
			pbType);
	}


	std::shared_ptr<re::ParameterBlock> Transform::CreateInstancedTransformParams(
		re::ParameterBlock::PBType pbType, std::vector<gr::Transform::RenderData const*> const& transformRenderData)
	{
		const uint32_t numInstances = util::CheckedCast<uint32_t>(transformRenderData.size());

		std::vector<gr::Transform::InstancedTransformParams> instancedMeshPBData;
		instancedMeshPBData.reserve(numInstances);

		for (size_t transformIdx = 0; transformIdx < numInstances; transformIdx++)
		{
			instancedMeshPBData.emplace_back(CreateInstancedTransformParamsData(*transformRenderData[transformIdx]));
		}

		std::shared_ptr<re::ParameterBlock> instancedMeshParams = re::ParameterBlock::CreateArray(
			gr::Transform::InstancedTransformParams::s_shaderName,
			&instancedMeshPBData[0],
			numInstances,
			pbType);

		return instancedMeshParams;
	}
}