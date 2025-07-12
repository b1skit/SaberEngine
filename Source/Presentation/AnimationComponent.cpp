// © 2024 Adam Badke. All rights reserved.
#include "AnimationComponent.h"
#include "EntityManager.h"
#include "NameComponent.h"
#include "TransformComponent.h"

#include "Core/Util/ImGuiUtils.h"


namespace pr
{
	AnimationController* AnimationController::CreateAnimationController(pr::EntityManager& em, char const* name)
	{
		entt::entity newEntity = em.CreateEntity(name);

		AnimationController* newAnimationController = 
			em.EmplaceComponent<AnimationController>(newEntity, AnimationController::PrivateCTORTag{});

		SEAssert(newAnimationController, "Failed to create newAnimationController");

		return newAnimationController;
	}


	std::unique_ptr<AnimationController> AnimationController::CreateAnimationControllerObject()
	{
		return std::make_unique<AnimationController>(AnimationController::PrivateCTORTag{});
	}


	AnimationController* AnimationController::CreateAnimationController(
		pr::EntityManager& em, char const* name, std::unique_ptr<AnimationController>&& initializedAnimationController)
	{
		SEAssert(initializedAnimationController != nullptr, "initializedAnimationController is null");

		entt::entity newEntity = em.CreateEntity(name);

		AnimationController* newAnimationController =
			em.EmplaceComponent<AnimationController>(newEntity, std::move(*initializedAnimationController));
		
		SEAssert(newAnimationController, "Failed to create newAnimationController");

		initializedAnimationController = nullptr;

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
		SEAssert(animationIdx < m_animChannelKeyframeTimesSec.size(), "OOB index");
		m_activeAnimationIdx = animationIdx;
	}


	size_t AnimationController::AddChannelKeyframeTimes(size_t animIdx, std::vector<float>&& keyframeTimes)
	{
		if (animIdx >= m_animChannelKeyframeTimesSec.size())
		{
			m_animChannelKeyframeTimesSec.emplace_back();
			SEAssert(animIdx == m_animChannelKeyframeTimesSec.size() - 1, 
				"Unexpected animation index: We currently expect new animations and their channels to be added in "
				"monotonically-increasing order");

			m_longestAnimChannelTimesSec.emplace_back(0.f);
			SEAssert(m_longestAnimChannelTimesSec.size() == m_animChannelKeyframeTimesSec.size(),
				"Animation times and longest channel times are out of sync");
		}

		const size_t channelKeyframeTimesIdx = m_animChannelKeyframeTimesSec[animIdx].size();

		std::vector<float> const& newKeyframeTimes = 
			m_animChannelKeyframeTimesSec[animIdx].emplace_back(std::move(keyframeTimes));
		
		// Update our longest channel timer
		for (float keyframeTime : newKeyframeTimes)
		{
			m_longestAnimChannelTimesSec[animIdx] =
				std::max(m_longestAnimChannelTimesSec[animIdx], keyframeTime);
		}		

		return channelKeyframeTimesIdx;
	}


	size_t AnimationController::AddChannelData(std::vector<float>&& channelData)
	{
		const size_t channelIdx = m_channelData.size();
		m_channelData.emplace_back(std::move(channelData));
		return channelIdx;
	}


	void AnimationController::ShowImGuiWindow(pr::EntityManager& em, entt::entity animControllerEntity)
	{
		pr::NameComponent const& nameComponent = em.GetComponent<pr::NameComponent>(animControllerEntity);

		if (ImGui::CollapsingHeader(
			std::format("Animation Controller: \"{}\"##{}", 
				nameComponent.GetName(), 
				nameComponent.GetUniqueID()).c_str(),
			ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			pr::AnimationController& animController = em.GetComponent<pr::AnimationController>(animControllerEntity);

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
					animController.SetAnimationState(pr::AnimationState::Stopped);
				}

				ImGui::SameLine();

				const pr::AnimationState currentAnimationState = animController.GetAnimationState();
				if (ImGui::Button(
					std::format("{}##{}", currentAnimationState != pr::AnimationState::Playing ? "Play" : "Pause",
						nameComponent.GetUniqueID()).c_str(),
					k_buttonDims))
				{
					if (currentAnimationState != pr::AnimationState::Playing)
					{
						animController.SetAnimationState(pr::AnimationState::Playing);
					}
					else
					{
						animController.SetAnimationState(pr::AnimationState::Paused);
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
					animController.GetActiveClampedAnimationTimeSec() / animController.GetActiveLongestChannelTimeSec();
				ImGui::ProgressBar(
					progress,
					ImVec2(0.f, 0.f),
					std::format("{:0.2f}%", progress * 100.f).c_str());
				ImGui::PopItemWidth();

				ImGui::SameLine();

				ImGui::Text(std::format("Time: {:0.2f} / {:0.2f} seconds", // Round to 2 decimal places
					animController.GetActiveClampedAnimationTimeSec(),
					animController.GetActiveLongestChannelTimeSec()).c_str());
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
				ImGui::Text(std::format("Longest animation: {} sec", animController.GetActiveLongestChannelTimeSec()).c_str());
				ImGui::Unindent();
			}

			ImGui::Unindent();
		}
	}


	// -----------------------------------------------------------------------------------------------------------------


	AnimationComponent* AnimationComponent::AttachAnimationComponent(
		pr::EntityManager& em, entt::entity entity, AnimationController const* animController)
	{
		SEAssert(animController != nullptr, "Animation controller cannot be null");

		pr::AnimationComponent* animationCmpt = 
			em.EmplaceComponent<pr::AnimationComponent>(entity, animController, PrivateCTORTag{});

		return animationCmpt;
	}


	void AnimationComponent::GetPrevNextKeyframeIdx(
		pr::AnimationController const* animController, 
		pr::AnimationData::Channel const& channel, 
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
		pr::AnimationComponent const& animCmpt, pr::TransformComponent& transformCmpt)
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

		pr::Transform& transform = transformCmpt.GetTransform();

		const float currentTimeSec = animCmpt.m_animationController->GetActiveClampedAnimationTimeSec();

		for (auto const& channel : animationData->m_channels)
		{
			// Find the next smallest/next largest keyframe time value about our current animation time:
			size_t prevKeyframeIdx = 0;
			size_t nextKeyframeIdx = 0;
			GetPrevNextKeyframeIdx(animCmpt.m_animationController, channel, prevKeyframeIdx, nextKeyframeIdx);

			// Select the appropriate channelData values:
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

				transform.SetLocalTranslation(interpolatedValue);
			}
			break;
			case AnimationPath::Rotation:
			{
				glm::quat interpolatedValue;
				if (channel.m_interpolationMode == pr::InterpolationMode::SphericalLinearInterpolation)
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

				transform.SetLocalRotation(glm::normalize(interpolatedValue));
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

				transform.SetLocalScale(interpolatedValue);
			}
			break;
			case AnimationPath::Weights:
			{
				// Do nothing: MeshMorphComponent handles AnimationPath::Weights
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
}