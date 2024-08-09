// © 2024 Adam Badke. All rights reserved.
#include "AnimationComponent.h"
#include "EntityManager.h"
#include "NameComponent.h"
#include "TransformComponent.h"

#include "Core/Util/ImGuiUtils.h"


namespace
{
	float ComputeSegmentNormalizedInterpolationFactor(float prevSec, float nextSec, float requestedSec)
	{
		SEAssert(nextSec >= prevSec, "Invalid time values");
		SEAssert(prevSec <= requestedSec && requestedSec <= nextSec, "Requested time step is OOB");

		const float stepDuration = nextSec - prevSec; // td
		return (requestedSec - prevSec) / stepDuration;
	}


	template<typename T>
	T GetInterpolatedValue(
		fr::InterpolationMode mode,
		T const& prevValue,
		T const& nextValue,
		float prevSec, 
		float nextSec, 
		float requestedSec)
	{
		if (prevSec == nextSec || prevValue == nextValue)
		{
			return prevValue;
		}

		const float t = ComputeSegmentNormalizedInterpolationFactor(prevSec, nextSec, requestedSec);

		switch (mode)
		{
		case fr::InterpolationMode::Linear:
		{
			return (1.f - t) * prevValue + t * nextValue;
		}
		break;
		case fr::InterpolationMode::Step:
		{
			return prevValue;
		}
		break;
		case fr::InterpolationMode::CubicSpline:
		{
			SEAssertF("TODO: Support this. We'll need to overload this function to take in/out tangents");
			// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#interpolation-cubic
		}
		break;
		case fr::InterpolationMode::SphericalLinearInterpolation:
		default: SEAssertF("Invalid interpolation mode");
		}
		return prevValue; // This should never happen
	}


	glm::quat GetInterpolatedValue(
		fr::InterpolationMode mode,
		glm::quat const& prevValue,
		glm::quat const& nextValue,
		float prevSec,
		float nextSec,
		float requestedSec)
	{
		SEAssert(mode == fr::InterpolationMode::SphericalLinearInterpolation, "Invalid mode for this implementation");

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
		, m_longestChannelTimeSec(0.f)
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
		const size_t keyframeTimesIdx = m_keyframeTimesSec.size();
		m_keyframeTimesSec.emplace_back(std::move(keyframeTimes));

		// Update our longest channel timer
		for (auto const& keyframeTime : m_keyframeTimesSec.back())
		{
			m_longestChannelTimeSec = std::max(m_longestChannelTimeSec, keyframeTime);
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
				animController.GetActiveClampedAnimationTimeSec() / animController.GetLongestAnimationTimeSec();
			ImGui::ProgressBar(
				progress,
				ImVec2(0.f, 0.f),
				std::format("{:0.2f}%", progress * 100.f).c_str());
			ImGui::PopItemWidth();

			ImGui::SameLine();

			ImGui::Text(std::format("Time: {:0.2f} / {:0.2f} seconds", // Round to 2 decimal places
				animController.GetActiveClampedAnimationTimeSec(),
				animController.GetLongestAnimationTimeSec()).c_str());

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
				ImGui::Text(std::format("Longest animation: {} sec", animController.GetLongestAnimationTimeSec()).c_str());
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
			return;
		}

		fr::Transform& transform = transformCmpt.GetTransform();

		for (auto const& channel : animationData->m_channels)
		{
			// Find the next smallest/next largest keyframe time value about our current animation time:
			const float currentTimeSec = animCmpt.m_animationController->GetActiveClampedAnimationTimeSec();

			std::vector<float> const& keyframeTimes =
				animCmpt.m_animationController->GetKeyframeTimes(channel.m_keyframeTimesIdx);

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

			size_t prevKeyframeIdx = 0;
			size_t nextKeyframeIdx = 0;

			if (currentTimeSec < minKeyframeTimeSec) // Clamp to the min
			{
				prevKeyframeIdx = minKeyframeTimeIdx;
				nextKeyframeIdx = minKeyframeTimeIdx;
			}
			else if (currentTimeSec > maxKeyframeTimeSec) // Clamp to the max
			{
				prevKeyframeIdx = maxKeyframeTimeIdx;
				nextKeyframeIdx = maxKeyframeTimeIdx;
			}
			else
			{
				if (keyframeTimes[minAbsDeltaIdx] < currentTimeSec)
				{
					prevKeyframeIdx = minAbsDeltaIdx;
					nextKeyframeIdx = (prevKeyframeIdx + 1) % keyframeTimes.size();
				}
				else // closest keyframe time >= currentTime
				{
					nextKeyframeIdx = minAbsDeltaIdx;
					prevKeyframeIdx = nextKeyframeIdx == 0 ? keyframeTimes.size() - 1 : nextKeyframeIdx - 1;
				}

				// Wrap the indexes if necessary:
				if (prevKeyframeIdx > nextKeyframeIdx)
				{
					prevKeyframeIdx = (prevKeyframeIdx + 1) % keyframeTimes.size();
					nextKeyframeIdx = (nextKeyframeIdx + 1) % keyframeTimes.size();

					SEAssert(currentTimeSec >= keyframeTimes[prevKeyframeIdx], "Indexes have been wrapped incorrectly");
				}
			}

			// Select the appropriate data values:
			std::vector<float> const& data = animCmpt.m_animationController->GetChannelData(channel.m_dataIdx);

			switch (channel.m_targetPath)
			{
			case AnimationPath::Translation:
			{
				SEAssert(keyframeTimes.size() == (data.size() / 3), "Keyframe/data size mismatch");

				glm::vec3 const& prevValue = reinterpret_cast<std::vector<glm::vec3> const&>(data)[prevKeyframeIdx];
				glm::vec3 const& nextValue = reinterpret_cast<std::vector<glm::vec3> const&>(data)[nextKeyframeIdx];

				glm::vec3 const& interpolatedValue = GetInterpolatedValue(
					channel.m_interpolationMode,
					prevValue,
					nextValue,
					keyframeTimes[prevKeyframeIdx],
					keyframeTimes[nextKeyframeIdx],
					currentTimeSec);

				transform.SetGlobalPosition(interpolatedValue);
			}
			break;
			case AnimationPath::Rotation:
			{
				SEAssert(keyframeTimes.size() == (data.size() / 4), "Keyframe/data size mismatch");

				glm::quat const& prevValue = reinterpret_cast<std::vector<glm::quat> const&>(data)[prevKeyframeIdx];
				glm::quat const& nextValue = reinterpret_cast<std::vector<glm::quat> const&>(data)[nextKeyframeIdx];

				glm::quat const& interpolatedValue = GetInterpolatedValue(
					channel.m_interpolationMode,
					prevValue,
					nextValue,
					keyframeTimes[prevKeyframeIdx],
					keyframeTimes[nextKeyframeIdx],
					currentTimeSec);

				transform.SetGlobalRotation(interpolatedValue);
			}
			break;
			case AnimationPath::Scale:
			{
				SEAssert(keyframeTimes.size() == (data.size() / 3), "Keyframe/data size mismatch");

				glm::vec3 const& prevValue = reinterpret_cast<std::vector<glm::vec3> const&>(data)[prevKeyframeIdx];
				glm::vec3 const& nextValue = reinterpret_cast<std::vector<glm::vec3> const&>(data)[nextKeyframeIdx];

				glm::vec3 const& interpolatedValue = GetInterpolatedValue(
					channel.m_interpolationMode,
					prevValue,
					nextValue,
					keyframeTimes[prevKeyframeIdx],
					keyframeTimes[nextKeyframeIdx],
					currentTimeSec);

				transform.SetGlobalScale(interpolatedValue);
			}
			break;
			case AnimationPath::Weights:
			{
				SEAssertF("TODO: Implement this");
				SEAssert(keyframeTimes.size() == data.size(), "Keyframe/data size mismatch");

				const float prevValue = data[prevKeyframeIdx];
				const float nextValue = data[nextKeyframeIdx];

				float interpolatedValue = GetInterpolatedValue(
					channel.m_interpolationMode,
					prevValue,
					nextValue,
					keyframeTimes[prevKeyframeIdx],
					keyframeTimes[nextKeyframeIdx],
					currentTimeSec);

				SEAssertF("TODO: Apply this interpolatedValue");
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

		return animDataItr == m_animationsData.end() ? nullptr : &(*animDataItr);
	}
}