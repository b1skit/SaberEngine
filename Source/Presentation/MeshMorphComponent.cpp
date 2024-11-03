// © 2024 Adam Badke. All rights reserved.
#include "AnimationComponent.h"
#include "EntityManager.h"
#include "MarkerComponents.h"
#include "MeshConcept.h"
#include "MeshMorphComponent.h"
#include "RenderDataComponent.h"


namespace fr
{
	MeshMorphComponent::MeshMorphComponent(PrivateCTORTag)
	{
		m_morphTargetWeights.reserve(gr::VertexStream::k_maxVertexStreams); // Best guess
	}


	MeshMorphComponent* MeshMorphComponent::AttachMeshAnimationComponent(
		fr::EntityManager& em, entt::entity entity, float const* defaultWeights, uint32_t count)
	{
		SEAssert(em.HasComponent<fr::Mesh::MeshConceptMarker>(entity),
			"An MeshAnimationComponent can only be attached to nodes that have a Mesh::MeshConceptMarker");

		SEAssert(em.HasComponent<fr::RenderDataComponent>(entity),
			"A MeshAnimationComponent's owningEntity requires a RenderDataComponent");

		fr::MeshMorphComponent* meshAnimCmpt =
			em.EmplaceComponent<fr::MeshMorphComponent>(entity, PrivateCTORTag{});

		SEAssert(defaultWeights && count > 0, "Invalid default weights");
		for (uint32_t weightIdx = 0; weightIdx < count; ++weightIdx)
		{
			meshAnimCmpt->SetMorphWeight(weightIdx, defaultWeights[weightIdx]);
		}

		em.EmplaceComponent<DirtyMarker<fr::MeshMorphComponent>>(entity);

		return meshAnimCmpt;
	}


	void MeshMorphComponent::ApplyAnimation(
		entt::entity meshConcept,
		fr::AnimationComponent const& animCmpt,
		fr::MeshMorphComponent& meshAnimCmpt)
	{
		if (animCmpt.GetAnimationController()->GetAnimationState() != AnimationState::Playing)
		{
			return;
		}

		AnimationData const* animationData =
			animCmpt.GetAnimationData(animCmpt.GetAnimationController()->GetActiveAnimationIdx());
		if (!animationData)
		{
			return; // Node is not animated by the given keyframe times index
		}

		bool didAnimate = false;
		for (auto const& channel : animationData->m_channels)
		{
			if (channel.m_targetPath != AnimationPath::Weights)
			{
				continue;
			}

			// Find the next smallest/next largest keyframe time value about our current animation time:
			size_t prevKeyframeIdx = 0;
			size_t nextKeyframeIdx = 0;
			AnimationComponent::GetPrevNextKeyframeIdx(
				animCmpt.GetAnimationController(), channel, prevKeyframeIdx, nextKeyframeIdx);

			// Select the appropriate channelData values:
			const float currentTimeSec = animCmpt.GetAnimationController()->GetActiveClampedAnimationTimeSec();

			std::vector<float> const& keyframeTimes =
				animCmpt.GetAnimationController()->GetKeyframeTimes(channel.m_keyframeTimesIdx);

			std::vector<float> const& channelData = animCmpt.GetAnimationController()->GetChannelData(channel.m_dataIdx);

			SEAssert(channel.m_dataFloatsPerKeyframe > 0 &&
				channel.m_dataFloatsPerKeyframe != AnimationData::k_invalidFloatsPerKeyframe,
				"Weight data must be 1 or more floats");

			for (uint8_t weightIdx = 0; weightIdx < channel.m_dataFloatsPerKeyframe; ++weightIdx)
			{
				const size_t prevIdx = prevKeyframeIdx * channel.m_dataFloatsPerKeyframe + weightIdx;
				const size_t nextIdx = nextKeyframeIdx * channel.m_dataFloatsPerKeyframe + weightIdx;

				const float interpolatedValue = fr::GetInterpolatedValue<float>(
					channel.m_interpolationMode,
					channelData.data(),
					channelData.size(),
					prevIdx,
					nextIdx,
					keyframeTimes[prevKeyframeIdx],
					keyframeTimes[nextKeyframeIdx],
					currentTimeSec);

				meshAnimCmpt.SetMorphWeight(weightIdx, interpolatedValue);
			}

			didAnimate = true;
		}

		if (didAnimate)
		{
			fr::EntityManager::Get()->TryEmplaceComponent<DirtyMarker<fr::MeshMorphComponent>>(meshConcept);
		}
	}


	gr::MeshPrimitive::MeshMorphRenderData MeshMorphComponent::CreateRenderData(
		entt::entity entity, MeshMorphComponent const& meshAnimCmpt)
	{
		gr::MeshPrimitive::MeshMorphRenderData meshRenderData{};

		meshRenderData.m_morphTargetWeights = meshAnimCmpt.m_morphTargetWeights;

		return meshRenderData;
	}
}