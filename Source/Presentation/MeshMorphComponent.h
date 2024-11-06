// © 2024 Adam Badke. All rights reserved.
#pragma once


namespace fr
{
	class AnimationComponent;
	class EntityManager;


	class MeshMorphComponent
	{
	public:
		static MeshMorphComponent* AttachMeshMorphComponent(
			fr::EntityManager&, entt::entity, float const* defaultWeights, uint32_t count);

		// Returns true if any animation was applied
		static void ApplyAnimation(entt::entity meshConcept, fr::AnimationComponent const&, fr::MeshMorphComponent&);


		static gr::MeshPrimitive::MeshMorphRenderData CreateRenderData(entt::entity, MeshMorphComponent const&);


	private: // Use the static creation factories
		struct PrivateCTORTag { explicit PrivateCTORTag() = default; };
		MeshMorphComponent() = delete;


	public:
		MeshMorphComponent(PrivateCTORTag);

		void SetMorphWeight(uint8_t weightIdx, float weight);


	private:
		std::vector<float> m_morphTargetWeights;
	};


	inline void MeshMorphComponent::SetMorphWeight(uint8_t weightIdx, float weight)
	{
		SEAssert(weight >= 0.f && weight <= 1.f, "OOB weight");

		if (weightIdx >= m_morphTargetWeights.size())
		{
			m_morphTargetWeights.resize(weightIdx + 1, 0.f); // GLTF specs: Default weights are 0
		}

		m_morphTargetWeights[weightIdx] = weight;
	}
}