// © 2024 Adam Badke. All rights reserved.
#include "AnimationComponent.h"
#include "EntityManager.h"
#include "MeshConcept.h"
#include "NameComponent.h"
#include "MarkerComponents.h"
#include "TransformComponent.h"

#include "Core/Util/ImGuiUtils.h"


namespace
{
	float ComputeSegmentNormalizedInterpolationFactor(float prevSec, float nextSec, float requestedSec)
	{
		const float stepDuration = glm::abs(nextSec - prevSec); // td
		return glm::abs(requestedSec - prevSec) / stepDuration;
	}


	template<typename T>
	T GetInterpolatedValue(
		fr::InterpolationMode mode,
		float const* channelData,
		size_t channelDataCount,
		size_t prevKeyframeIdx,
		size_t nextKeyframeIdx,
		float prevSec,
		float nextSec, 
		float requestedSec)
	{
		const float t = ComputeSegmentNormalizedInterpolationFactor(prevSec, nextSec, requestedSec);

		switch (mode)
		{
		case fr::InterpolationMode::Linear:
		{
			T const& prevValue = reinterpret_cast<T const*>(channelData)[prevKeyframeIdx];
			T const& nextValue = reinterpret_cast<T const*>(channelData)[nextKeyframeIdx];

			if (prevSec == nextSec || prevValue == nextValue)
			{
				return prevValue;
			}

			return (1.f - t) * prevValue + t * nextValue;
		}
		break;
		case fr::InterpolationMode::Step:
		{
			T const& prevValue = reinterpret_cast<T const*>(channelData)[prevKeyframeIdx];

			return prevValue;
		}
		break;
		case fr::InterpolationMode::CubicSpline:
		{
			const bool isFirstKeyframeTangent = prevKeyframeIdx == 0;
			const bool isLastKeyframeTangent = prevKeyframeIdx > nextKeyframeIdx;

			const float deltaTime = nextSec - prevSec; // t_d

			// Scale our indexes: Tangents are stored in the 3 elements of animation channel data
			// {input tangent, keyframe value, output tangent}
			prevKeyframeIdx *= 3;
			nextKeyframeIdx *= 3;

			T const& prevValue = reinterpret_cast<T const*>(channelData)[prevKeyframeIdx + 1];
			T prevOutputTangent = reinterpret_cast<T const*>(channelData)[prevKeyframeIdx + 2] * deltaTime;
			
			T nextInputTangent = reinterpret_cast<T const*>(channelData)[nextKeyframeIdx] * deltaTime;
			T const& nextValue = reinterpret_cast<T const*>(channelData)[nextKeyframeIdx + 1];

			// GLTF specs - the input tangent of the 1st keyframe, and output tangent of the last keyframe are ignored
			if (isFirstKeyframeTangent)
			{
				prevOutputTangent *= 0.f;
			}
			if (isLastKeyframeTangent)
			{
				nextInputTangent *= 0.f;
			}

			SEAssert(prevValue != -nextValue, "Invalid quaternion (all zeros) will be produced by the interpolation");

			const float t2 = t * t;
			const float t3 = t2 * t;

			return (2.f * t3 - 3.f * t2 + 1.f) * prevValue + 
				(t3 - 2.f * t2 + t) * prevOutputTangent + 
				(-2.f * t3 + 3.f * t2) * nextValue + 
				(t3 - t2) * nextInputTangent;
		}
		break;
		case fr::InterpolationMode::SphericalLinearInterpolation:
		default: SEAssertF("Invalid interpolation mode");
		}
		return reinterpret_cast<T const*>(channelData)[prevKeyframeIdx]; // This should never happen
	}


	glm::quat GetSphericalLinearInterpolatedValue(
		fr::InterpolationMode mode,
		float const* channelData,
		size_t channelDataCount,
		size_t prevKeyframeIdx,
		size_t nextKeyframeIdx,
		float prevSec,
		float nextSec,
		float requestedSec)
	{
		SEAssert(mode == fr::InterpolationMode::SphericalLinearInterpolation, "Invalid mode for this implementation");

		glm::quat const& prevValue = reinterpret_cast<glm::quat const*>(channelData)[prevKeyframeIdx];
		glm::quat const& nextValue = reinterpret_cast<glm::quat const*>(channelData)[nextKeyframeIdx];

		SEAssert(prevValue != -nextValue, "Invalid quaternion (all zeros) will be produced by the interpolation");

		if (prevSec == nextSec || prevValue == nextValue)
		{
			return prevValue;
		}

		const float t = ComputeSegmentNormalizedInterpolationFactor(prevSec, nextSec, requestedSec);

		return glm::slerp(prevValue, nextValue, t);
	}
}


namespace fr
{
	AnimationController* AnimationController::CreateAnimationController(fr::EntityManager& em, char const* name)
	{
		entt::entity newEntity = em.CreateEntity(name);

		AnimationController* newAnimationController = 
			em.EmplaceComponent<AnimationController>(newEntity, AnimationController::PrivateCTORTag{});

		SEAssert(newAnimationController, "Failed to create newAnimationController");

		return newAnimationController;
	}


	void AnimationController::UpdateAnimationController(AnimationController& animController, double stepTimeMs)
	{
		if (animController.HasAnimations())	
		{
			animController.UpdateCurrentAnimationTime(stepTimeMs);
		}
	}


	AnimationController::AnimationController(PrivateCTORTag)
		: m_animationState(AnimationState::Playing)
		, m_activeAnimationIdx(0)
		, m_animationSpeed(1.f)
	{
	}


	void AnimationController::UpdateCurrentAnimationTime(double timeStepMS)
	{
		if (m_animationState == AnimationState::Playing)
		{
			m_currentTimeSec[m_activeAnimationIdx] += m_animationSpeed * (timeStepMS / 1000.0); // Convert Ms -> Sec
		}
	}


	void AnimationController::SetAnimationState(AnimationState newState)
	{
		m_animationState = newState;

		if (m_animationState == AnimationState::Stopped)
		{
			m_currentTimeSec[m_activeAnimationIdx] = 0.0;
		}
	}


	void AnimationController::SetActiveAnimationIdx(size_t animationIdx)
	{
		SEAssert(animationIdx < m_keyframeTimesSec.size(), "OOB index");
		m_activeAnimationIdx = animationIdx;
	}


	size_t AnimationController::AddKeyframeTimes(std::vector<float>&& keyframeTimes)
	{
		SEAssert(m_keyframeTimesSec.size() == m_longestChannelTimesSec.size(),
			"Animation index is out of sync");

		const size_t keyframeTimesIdx = m_keyframeTimesSec.size();
		std::vector<float>& newKeyframeTimes = m_keyframeTimesSec.emplace_back(std::move(keyframeTimes));
		m_longestChannelTimesSec.emplace_back(std::numeric_limits<float>::min());

		// Update our longest channel timer
		for (float keyframeTime : newKeyframeTimes)
		{
			m_longestChannelTimesSec[keyframeTimesIdx] =
				std::max(m_longestChannelTimesSec[keyframeTimesIdx], keyframeTime);
		}		

		return keyframeTimesIdx;
	}


	size_t AnimationController::AddChannelData(std::vector<float>&& channelData)
	{
		const size_t channelIdx = m_channelData.size();
		m_channelData.emplace_back(std::move(channelData));
		return channelIdx;
	}


	void AnimationController::ShowImGuiWindow(fr::EntityManager& em, entt::entity animControllerEntity)
	{
		fr::NameComponent const& nameComponent = em.GetComponent<fr::NameComponent>(animControllerEntity);

		if (ImGui::CollapsingHeader(
			std::format("Animation Controller: \"{}\"##{}", 
				nameComponent.GetName(), 
				nameComponent.GetUniqueID()).c_str(),
			ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			fr::AnimationController& animController = em.GetComponent<fr::AnimationController>(animControllerEntity);

			if (animController.HasAnimations())
			{
				const size_t numAnimations = animController.GetAnimationCount();
				size_t currentAnimationIdx = animController.GetActiveAnimationIdx();

				std::vector<std::string> indexDropdownStrings;
				indexDropdownStrings.reserve(numAnimations);
				for (size_t i = 0; i < numAnimations; ++i)
				{
					indexDropdownStrings.emplace_back(std::format("{}: {}", i, animController.m_animationNames[i]));
				}

				ImGui::PushItemWidth(-ImGui::GetContentRegionAvail().x * 0.4f);
				if (util::ShowBasicComboBox(
					std::format("Active animation##{}", nameComponent.GetUniqueID()).c_str(),
					indexDropdownStrings.data(),
					indexDropdownStrings.size(),
					currentAnimationIdx))
				{
					animController.SetActiveAnimationIdx(currentAnimationIdx);
				}
				ImGui::PopItemWidth();

				constexpr ImVec2 k_buttonDims = ImVec2(50.f, 0.f);
				if (ImGui::Button(std::format("Stop##{}", nameComponent.GetUniqueID()).c_str(),
					k_buttonDims))
				{
					animController.SetAnimationState(fr::AnimationState::Stopped);
				}

				ImGui::SameLine();

				const fr::AnimationState currentAnimationState = animController.GetAnimationState();
				if (ImGui::Button(
					std::format("{}##{}", currentAnimationState != fr::AnimationState::Playing ? "Play" : "Pause",
						nameComponent.GetUniqueID()).c_str(),
					k_buttonDims))
				{
					if (currentAnimationState != fr::AnimationState::Playing)
					{
						animController.SetAnimationState(fr::AnimationState::Playing);
					}
					else
					{
						animController.SetAnimationState(fr::AnimationState::Paused);
					}
				}

				ImGui::SameLine();

				float animationSpeed = animController.GetAnimationSpeed();
				ImGui::PushItemWidth(-ImGui::GetContentRegionAvail().x * 0.5f);
				if (ImGui::SliderFloat(
					std::format("Animation speed##{}", nameComponent.GetUniqueID()).c_str(),
					&animationSpeed,
					-4.f,
					4.f))
				{
					animController.SetAnimationSpeed(animationSpeed);
				}
				ImGui::PopItemWidth();

				ImGui::PushItemWidth(-ImGui::GetContentRegionAvail().x * 0.4f);
				const float progress =
					animController.GetActiveClampedAnimationTimeSec() / animController.GetActiveLongestAnimationTimeSec();
				ImGui::ProgressBar(
					progress,
					ImVec2(0.f, 0.f),
					std::format("{:0.2f}%", progress * 100.f).c_str());
				ImGui::PopItemWidth();

				ImGui::SameLine();

				ImGui::Text(std::format("Time: {:0.2f} / {:0.2f} seconds", // Round to 2 decimal places
					animController.GetActiveClampedAnimationTimeSec(),
					animController.GetActiveLongestAnimationTimeSec()).c_str());
			}
			else
			{
				ImGui::Text("<No animations found>");
			}

			if (ImGui::CollapsingHeader(std::format("Metadata##{}", nameComponent.GetUniqueID()).c_str()))
			{
				ImGui::Indent();
				const size_t animationCount = animController.GetAnimationCount();
				ImGui::Text(std::format("{} animation{}", 
					animationCount,
					animationCount > 1 ? "s" : "").c_str());
				const size_t numKeyframeTimes = animController.GetNumKeyframeTimes();
				ImGui::Text(std::format("{} keyframe time channel{}", 
					numKeyframeTimes,
					numKeyframeTimes > 1 ? "s" : "").c_str());
				const size_t numDataChannels = animController.GetNumChannels();
				ImGui::Text(std::format("{} data channel{}",
					numDataChannels,
					numDataChannels > 1 ? "s" : "").c_str());
				ImGui::Text(std::format("Longest animation: {} sec", animController.GetActiveLongestAnimationTimeSec()).c_str());
				ImGui::Unindent();
			}

			ImGui::Unindent();
		}
	}


	// -----------------------------------------------------------------------------------------------------------------


	AnimationComponent* AnimationComponent::AttachAnimationComponent(
		fr::EntityManager& em, entt::entity entity, AnimationController const* animController)
	{
		SEAssert(animController != nullptr, "Animation controller cannot be null");
		SEAssert(em.HasComponent<fr::TransformComponent>(entity),
			"An animation component can only be attached to nodes that have a TransformComponent");

		fr::AnimationComponent* animationCmpt = 
			em.EmplaceComponent<fr::AnimationComponent>(entity, animController, PrivateCTORTag{});

		return animationCmpt;
	}


	void AnimationComponent::GetPrevNextKeyframeIdx(
		fr::AnimationController const* animController, 
		fr::AnimationData::Channel const& channel, 
		size_t& prevKeyframeIdxOut,
		size_t& nextKeyframeIdxOut)
	{
		// Find the next smallest/next largest keyframe time value about our current animation time:
		const float currentTimeSec = animController->GetActiveClampedAnimationTimeSec();

		std::vector<float> const& keyframeTimes = animController->GetKeyframeTimes(channel.m_keyframeTimesIdx);

		// Find the closest keyframe time to the current time:
		float minAbsDelta = std::numeric_limits<float>::max();
		size_t minAbsDeltaIdx = 0;
		float minKeyframeTimeSec = std::numeric_limits<float>::max();
		size_t minKeyframeTimeIdx = 0;
		float maxKeyframeTimeSec = std::numeric_limits<float>::min();
		size_t maxKeyframeTimeIdx = 0;
		for (size_t i = 0; i < keyframeTimes.size(); ++i)
		{
			const float curAbsDelta = glm::abs(currentTimeSec - keyframeTimes[i]);
			if (curAbsDelta < minAbsDelta)
			{
				minAbsDelta = curAbsDelta;
				minAbsDeltaIdx = i;
			}

			// Cache the min/max values we encounter:
			if (keyframeTimes[i] < minKeyframeTimeSec)
			{
				minKeyframeTimeSec = keyframeTimes[i];
				minKeyframeTimeIdx = i;
			}
			if (keyframeTimes[i] > maxKeyframeTimeSec)
			{
				maxKeyframeTimeSec = keyframeTimes[i];
				maxKeyframeTimeIdx = i;
			}
		}

		prevKeyframeIdxOut = 0;
		nextKeyframeIdxOut = 0;

		if (currentTimeSec < minKeyframeTimeSec) // Clamp to the min
		{
			prevKeyframeIdxOut = minKeyframeTimeIdx;
			nextKeyframeIdxOut = minKeyframeTimeIdx;
		}
		else if (currentTimeSec > maxKeyframeTimeSec) // Clamp to the max
		{
			prevKeyframeIdxOut = maxKeyframeTimeIdx;
			nextKeyframeIdxOut = maxKeyframeTimeIdx;
		}
		else
		{
			if (keyframeTimes[minAbsDeltaIdx] < currentTimeSec)
			{
				prevKeyframeIdxOut = minAbsDeltaIdx;
				nextKeyframeIdxOut = (prevKeyframeIdxOut + 1) % keyframeTimes.size();
			}
			else // closest keyframe time >= currentTime
			{
				nextKeyframeIdxOut = minAbsDeltaIdx;
				prevKeyframeIdxOut = nextKeyframeIdxOut == 0 ? keyframeTimes.size() - 1 : nextKeyframeIdxOut - 1;
			}
		}
	}


	void AnimationComponent::ApplyAnimation(
		fr::AnimationComponent const& animCmpt, fr::TransformComponent& transformCmpt)
	{
		if (animCmpt.m_animationController->GetAnimationState() != AnimationState::Playing)
		{
			return;
		}

		AnimationData const* animationData = 
			animCmpt.GetAnimationData(animCmpt.m_animationController->GetActiveAnimationIdx());
		if (!animationData)
		{
			return; // Node is not animated by the given keyframe times index
		}

		fr::Transform& transform = transformCmpt.GetTransform();

		for (auto const& channel : animationData->m_channels)
		{
			// Find the next smallest/next largest keyframe time value about our current animation time:
			size_t prevKeyframeIdx = 0;
			size_t nextKeyframeIdx = 0;
			GetPrevNextKeyframeIdx(animCmpt.m_animationController, channel, prevKeyframeIdx, nextKeyframeIdx);

			// Select the appropriate channelData values:
			const float currentTimeSec = animCmpt.m_animationController->GetActiveClampedAnimationTimeSec();

			std::vector<float> const& keyframeTimes =
				animCmpt.m_animationController->GetKeyframeTimes(channel.m_keyframeTimesIdx);

			std::vector<float> const& channelData = animCmpt.m_animationController->GetChannelData(channel.m_dataIdx);

			switch (channel.m_targetPath)
			{
			case AnimationPath::Translation:
			{
				glm::vec3 const& interpolatedValue = GetInterpolatedValue<glm::vec3>(
					channel.m_interpolationMode,
					channelData.data(),
					channelData.size(),
					prevKeyframeIdx,
					nextKeyframeIdx,
					keyframeTimes[prevKeyframeIdx],
					keyframeTimes[nextKeyframeIdx],
					currentTimeSec);

				transform.SetGlobalPosition(interpolatedValue);
			}
			break;
			case AnimationPath::Rotation:
			{
				glm::quat interpolatedValue;
				if (channel.m_interpolationMode == fr::InterpolationMode::SphericalLinearInterpolation)
				{
					interpolatedValue = GetSphericalLinearInterpolatedValue(
						channel.m_interpolationMode,
						channelData.data(),
						channelData.size(),
						prevKeyframeIdx,
						nextKeyframeIdx,
						keyframeTimes[prevKeyframeIdx],
						keyframeTimes[nextKeyframeIdx],
						currentTimeSec);
				}
				else
				{
					interpolatedValue = GetInterpolatedValue<glm::quat>(
						channel.m_interpolationMode,
						channelData.data(),
						channelData.size(),
						prevKeyframeIdx,
						nextKeyframeIdx,
						keyframeTimes[prevKeyframeIdx],
						keyframeTimes[nextKeyframeIdx],
						currentTimeSec);
				}

				transform.SetGlobalRotation(glm::normalize(interpolatedValue));
			}
			break;
			case AnimationPath::Scale:
			{
				glm::vec3 const& interpolatedValue = GetInterpolatedValue<glm::vec3>(
					channel.m_interpolationMode,
					channelData.data(),
					channelData.size(),
					prevKeyframeIdx,
					nextKeyframeIdx,
					keyframeTimes[prevKeyframeIdx],
					keyframeTimes[nextKeyframeIdx],
					currentTimeSec);

				transform.SetGlobalScale(interpolatedValue);
			}
			break;
			case AnimationPath::Weights:
			{
				// Do nothing: MeshAnimationComponent handles AnimationPath::Weights
			}
			break;
			default: SEAssertF("Invalid animation target");
			}
		}
	}


	AnimationComponent::AnimationComponent(AnimationController const* animController, PrivateCTORTag)
		: m_animationController(animController)
	{
		SEAssert(m_animationController, "Animation controller cannot be null");
	}


	void AnimationComponent::SetAnimationData(AnimationData const& animationData)
	{
		m_animationsData.emplace_back(animationData);

		std::sort(m_animationsData.begin(), m_animationsData.end(), AnimationsDataComparator());
	}


	AnimationData const* AnimationComponent::GetAnimationData(size_t animationIdx) const
	{
		auto animDataItr = std::lower_bound( // Get an iterator to the 1st element with >= the keyframeTimesIdx
			m_animationsData.begin(),
			m_animationsData.end(),
			animationIdx,
			AnimationsDataComparator());

		// Return null if node is not animated by the given keyframe times index
		return animDataItr == m_animationsData.end() ? nullptr : &(*animDataItr);
	}


	// -----------------------------------------------------------------------------------------------------------------


	MeshAnimationComponent::MeshAnimationComponent(PrivateCTORTag)
	{
		m_morphTargetWeights.reserve(gr::VertexStream::k_maxVertexStreams); // Best guess
	}


	MeshAnimationComponent* MeshAnimationComponent::AttachMeshAnimationComponent(
		fr::EntityManager& em, entt::entity entity, float const* defaultWeights, uint32_t count)
	{
		SEAssert(em.HasComponent<fr::Mesh::MeshConceptMarker>(entity),
			"An MeshAnimationComponent can only be attached to nodes that have a Mesh::MeshConceptMarker");

		fr::MeshAnimationComponent* meshAnimCmpt =
			em.EmplaceComponent<fr::MeshAnimationComponent>(entity, PrivateCTORTag{});

		SEAssert(defaultWeights && count > 0, "Invalid default weights");
		for (uint32_t weightIdx = 0; weightIdx < count; ++weightIdx)
		{
			meshAnimCmpt->SetMorphWeight(weightIdx, defaultWeights[weightIdx]);
		}

		em.EmplaceComponent<DirtyMarker<fr::MeshAnimationComponent>>(entity);

		return meshAnimCmpt;
	}


	void MeshAnimationComponent::ApplyAnimation(
		entt::entity meshConcept,
		fr::AnimationComponent const& animCmpt,
		fr::MeshAnimationComponent& meshAnimCmpt)
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

				const float interpolatedValue = GetInterpolatedValue<float>(
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
			fr::EntityManager::Get()->TryEmplaceComponent<DirtyMarker<fr::MeshAnimationComponent>>(meshConcept);
		}
	}


	gr::MeshPrimitive::MeshRenderData MeshAnimationComponent::CreateRenderData(
		entt::entity entity, MeshAnimationComponent const& meshAnimCmpt)
	{
		gr::MeshPrimitive::MeshRenderData meshRenderData{};
	
		meshRenderData.m_morphTargetWeights = meshAnimCmpt.m_morphTargetWeights;

		return meshRenderData;
	}
}