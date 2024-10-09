// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Core/Assert.h"

#include "Renderer/MeshPrimitive.h"
#include "Renderer/VertexStream.h"


namespace fr
{
	class EntityManager;
	class MeshPrimitiveComponent;
	class NameComponent;
	class TransformComponent;


	enum AnimationPath : uint8_t
	{
		Translation,
		Rotation,
		Scale,
		Weights, // For morph targets
		
		AnimationPath_Invalid
	};


	enum InterpolationMode
	{
		Linear,
		SphericalLinearInterpolation,
		Step,
		CubicSpline,

		InterpolationMode_Invalid
	};


	enum AnimationState
	{
		Playing,
		Stopped,
		Paused,
	};


	class AnimationController
	{
	public:
		static constexpr auto in_place_delete = true; // Required for pointer stability


	public:
		static AnimationController* CreateAnimationController(fr::EntityManager&, char const* name);

		static void UpdateAnimationController(AnimationController&, double stepTimeMs);


	public:
		bool HasAnimations() const;

		void UpdateCurrentAnimationTime(double timeStepMS);
		float GetActiveClampedAnimationTimeSec() const;

		void SetAnimationState(AnimationState);
		AnimationState GetAnimationState() const;

		void SetActiveAnimationIdx(size_t animationIdx);
		size_t GetActiveAnimationIdx() const;
		size_t GetAnimationCount() const;

		float GetAnimationSpeed() const;
		void SetAnimationSpeed(float);

		float GetLongestAnimationTimeSec() const;

		float GetLongestChannelLength() const;
		


	public:
		void AddNewAnimation(char const* animName); // Called once per animation, during construction

		size_t AddKeyframeTimes(std::vector<float>&&); // Returns keyframeTimesIdx
		std::vector<float> const& GetKeyframeTimes(size_t keyframeTimesIdx) const;
		size_t GetNumKeyframeTimes() const;

		size_t AddChannelData(std::vector<float>&&); // Returns channelIdx
		std::vector<float> const& GetChannelData(size_t channelIdx) const;
		size_t GetNumChannels() const;

	public:
		static void ShowImGuiWindow(fr::EntityManager&, entt::entity);


	private:
		AnimationState m_animationState;

		size_t m_activeAnimationIdx;
		float m_animationSpeed;
		float m_longestChannelTimeSec;

		std::vector<std::string> m_animationNames;
		std::vector<double> m_currentTimeSec;
		std::vector<std::vector<float>> m_keyframeTimesSec;

		std::vector<std::vector<float>> m_channelData; // ALL data for all animations


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
		AnimationController() : AnimationController(PrivateCTORTag{}) {}


	public:
		AnimationController(PrivateCTORTag);

		~AnimationController() = default;
		AnimationController(AnimationController&&) noexcept = default;
		AnimationController& operator=(AnimationController&&) noexcept = default;


	private: // No copying allowed
		AnimationController(AnimationController const&) = delete;
		AnimationController& operator=(AnimationController const&) = delete;
	};


	inline bool AnimationController::HasAnimations() const
	{
		return GetAnimationCount() > 0;
	}


	inline float AnimationController::GetActiveClampedAnimationTimeSec() const
	{
		return glm::fmod(static_cast<float>(m_currentTimeSec[m_activeAnimationIdx]), m_longestChannelTimeSec);
	}


	inline size_t AnimationController::GetActiveAnimationIdx() const
	{
		return m_activeAnimationIdx;
	}


	inline size_t AnimationController::GetAnimationCount() const
	{
		return m_animationNames.size();
	}


	inline float AnimationController::GetAnimationSpeed() const
	{
		return m_animationSpeed;
	}


	inline void AnimationController::SetAnimationSpeed(float newSpeed)
	{
		m_animationSpeed = newSpeed;
	}


	inline float AnimationController::GetLongestAnimationTimeSec() const
	{
		return m_longestChannelTimeSec;
	}


	inline float AnimationController::GetLongestChannelLength() const
	{
		return m_longestChannelTimeSec;
	}


	inline void AnimationController::AddNewAnimation(char const* animName)
	{
		SEAssert(animName, "Animation name cannot be null");
		m_currentTimeSec.emplace_back(0.0);
		m_animationNames.emplace_back(animName);
		SEAssert(m_currentTimeSec.size() == m_animationNames.size(), "Animation names and timers are out of sync");
	}


	inline AnimationState AnimationController::GetAnimationState() const
	{
		return m_animationState;
	}


	inline std::vector<float> const& AnimationController::GetKeyframeTimes(size_t keyframeTimesIdx) const
	{
		SEAssert(keyframeTimesIdx < m_keyframeTimesSec.size(), "Invalid index");
		return m_keyframeTimesSec[keyframeTimesIdx];
	}


	inline size_t AnimationController::GetNumKeyframeTimes() const
	{
		return m_keyframeTimesSec.size();
	}


	inline std::vector<float> const& AnimationController::GetChannelData(size_t channelIdx) const
	{
		SEAssert(channelIdx < m_channelData.size(), "Invalid index");
		return m_channelData[channelIdx];
	}


	inline size_t AnimationController::GetNumChannels() const
	{
		return m_channelData.size();
	}
	

	// ----


	struct AnimationData
	{
		static constexpr size_t k_invalidIdx = std::numeric_limits<size_t>::max();
		static constexpr size_t k_invalidFloatsPerKeyframe = std::numeric_limits<uint8_t>::max();

		size_t m_animationIdx;

		struct Channel
		{
			InterpolationMode m_interpolationMode	= InterpolationMode::InterpolationMode_Invalid;
			AnimationPath m_targetPath				= AnimationPath::AnimationPath_Invalid;
			size_t m_keyframeTimesIdx				= k_invalidIdx;
			size_t m_dataIdx						= k_invalidIdx;
			uint8_t m_dataFloatsPerKeyframe			= k_invalidFloatsPerKeyframe;
		};

		std::vector<Channel> m_channels;
	};


	// ----


	class AnimationComponent
	{
	public:
		static AnimationComponent* AttachAnimationComponent(fr::EntityManager&, entt::entity, AnimationController const*);

		static void ApplyAnimation(fr::AnimationComponent const&, fr::TransformComponent&);


	public: // Helpers:
		static void GetPrevNextKeyframeIdx(
			fr::AnimationController const*, fr::AnimationData::Channel const&, size_t& prevIdxOut, size_t& nextIdxOut);


	private: // Use the static creation factories
		AnimationComponent() = delete;
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };


	public:
		AnimationComponent(AnimationController const*, PrivateCTORTag);

		~AnimationComponent() = default;

		AnimationComponent(AnimationComponent const&) = default;
		AnimationComponent(AnimationComponent&&) = default;

		AnimationComponent& operator=(AnimationComponent const&) = default;
		AnimationComponent& operator=(AnimationComponent&&) = default;


	public:
		void SetAnimationData(AnimationData const&);

		AnimationController const* GetAnimationController() const;
		
		// Returns null if node is not animated by the given keyframe times index
		AnimationData const* GetAnimationData(size_t animationIdx) const; 

		AnimationState GetAnimationState() const;
		bool IsPlaying() const;

	private:
		AnimationController const* m_animationController;
		std::vector<AnimationData> m_animationsData; // Maintained in sorted order, by animation index


	private:
		struct AnimationsDataComparator
		{
			// Return true if 1st element is ordered before the 2nd
			inline bool operator()(AnimationData const& animData, size_t animationIdx)
			{
				return animData.m_animationIdx < animationIdx;

			}

			inline bool operator()(AnimationData const& a, AnimationData const& b)
			{
				return a.m_animationIdx < b.m_animationIdx;
			}
		};
	};


	inline AnimationController const* AnimationComponent::GetAnimationController() const
	{
		return m_animationController;
	}


	inline AnimationState AnimationComponent::GetAnimationState() const
	{
		return m_animationController->GetAnimationState();
	}


	inline bool AnimationComponent::IsPlaying() const
	{
		return GetAnimationState() == fr::AnimationState::Playing;
	}


	// ---


	class MeshAnimationComponent
	{
	public:
		static MeshAnimationComponent* AttachMeshAnimationComponent(fr::EntityManager&, entt::entity);

		// Returns true if any animation was applied
		static bool ApplyAnimation(fr::AnimationComponent const&, fr::MeshAnimationComponent&, entt::entity meshConcept);


		static gr::MeshPrimitive::MeshRenderData CreateRenderData(entt::entity, MeshAnimationComponent const&);


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
		MeshAnimationComponent() = delete;


	public:
		MeshAnimationComponent(PrivateCTORTag);

		void SetMorphWeight(uint8_t weightIdx, float weight);


	private:
		std::array<float, gr::VertexStream::k_maxVertexStreams> m_morphWeights;
	};


	inline void MeshAnimationComponent::SetMorphWeight(uint8_t weightIdx, float weight)
	{
		SEAssert(weightIdx < m_morphWeights.size(), "OOB index");
		SEAssert(weight >= 0.f && weight <= 1.f, "OOB weight");

		m_morphWeights[weightIdx] = weight;
	}
}