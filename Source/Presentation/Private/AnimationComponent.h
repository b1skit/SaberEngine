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


	// -----------------------------------------------------------------------------------------------------------------


	inline float ComputeSegmentNormalizedInterpolationFactor(float prevSec, float nextSec, float requestedSec)
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


	inline glm::quat GetSphericalLinearInterpolatedValue(
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


	// -----------------------------------------------------------------------------------------------------------------


	class AnimationController final
	{
	public:
		static constexpr auto in_place_delete = true; // Required for pointer stability


	public:
		// Create an empty AnimationController entity directly:
		static AnimationController* CreateAnimationController(fr::EntityManager&, char const* name);

		// 2-step/deferred AnimationController construction:
		// 1) Create an animation controller object
		// 2) Populate it
		// 3) Move it to initialize an entity/component with it
		static std::unique_ptr<AnimationController> CreateAnimationControllerObject();
		static AnimationController* CreateAnimationController(
			fr::EntityManager&, char const* name, std::unique_ptr<AnimationController>&&);

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

		float GetActiveLongestChannelTimeSec() const;


	public:
		void AddNewAnimation(char const* animName); // Called once per animation, during construction

		size_t AddChannelKeyframeTimes(size_t animIdx, std::vector<float>&&); // Returns keyframeTimesIdx for channel
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

		std::vector<std::string> m_animationNames;
		std::vector<double> m_currentTimeSec;

		std::vector<std::vector<std::vector<float>>> m_animChannelKeyframeTimesSec; // [animation][channel] == vector<float> keyframe times

		std::vector<float> m_longestAnimChannelTimesSec; // Indexed per animation

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
		SEAssert(m_activeAnimationIdx < m_currentTimeSec.size(), "m_activeAnimationIdx is out of sync");

		return glm::fmod(
			static_cast<float>(m_currentTimeSec[m_activeAnimationIdx]),
			m_longestAnimChannelTimesSec[m_activeAnimationIdx]);
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


	inline float AnimationController::GetActiveLongestChannelTimeSec() const
	{
		return m_longestAnimChannelTimesSec.empty() ? 0 : m_longestAnimChannelTimesSec[m_activeAnimationIdx];
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
		SEAssert(keyframeTimesIdx < m_animChannelKeyframeTimesSec[m_activeAnimationIdx].size(), "Invalid index");
		return m_animChannelKeyframeTimesSec[m_activeAnimationIdx][keyframeTimesIdx];
	}


	inline size_t AnimationController::GetNumKeyframeTimes() const
	{
		return m_animChannelKeyframeTimesSec.empty() ? 0 : m_animChannelKeyframeTimesSec[m_activeAnimationIdx].size();
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


	struct AnimationData final
	{
		static constexpr size_t k_invalidIdx = std::numeric_limits<size_t>::max();
		static constexpr size_t k_invalidFloatsPerKeyframe = std::numeric_limits<uint8_t>::max();

		size_t m_animationIdx;

		struct Channel final
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


	class AnimationComponent final
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
		AnimationComponent(AnimationComponent&&) noexcept = default;

		AnimationComponent& operator=(AnimationComponent const&) = default;
		AnimationComponent& operator=(AnimationComponent&&) noexcept = default;


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
		struct AnimationsDataComparator final
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
}