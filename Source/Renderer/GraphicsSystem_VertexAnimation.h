// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "Batch.h"
#include "GraphicsSystem.h"
#include "GraphicsSystemCommon.h"


namespace gr
{
	class VertexAnimationGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<VertexAnimationGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "VertexAnimation"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(VertexAnimationGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(VertexAnimationGraphicsSystem, PreRender))
			);
		}

		static constexpr util::CHashKey k_cullingDataInput = "ViewCullingResults";
		void RegisterInputs() override;

		static constexpr util::CHashKey k_animatedVertexStreamsOutput = "AnimatedVertexStreams";
		void RegisterOutputs() override;


	public:
		VertexAnimationGraphicsSystem(gr::GraphicsSystemManager*);
		~VertexAnimationGraphicsSystem() = default;

		void InitPipeline(re::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);
		void PreRender();


	private:
		void CreateMorphAnimationBatches();
		void CreateSkinningAnimationBatches();


	private:
		std::shared_ptr<re::Stage> m_morphAnimationStage;
		std::shared_ptr<re::Stage> m_skinAnimationStage;


	private:
		// Map from MeshConcept RenderDataID to mesh-specific morph weight Buffers. Set on each Batch generated from 
		// MeshPrimitives attached to the MeshConcept
		std::unordered_map<gr::RenderDataID, std::shared_ptr<re::Buffer>> m_meshIDToMorphWeights;

		// Map from MeshConcept RenderDataIDs with a gr::MeshPrimitive::SkinningRenderData, to our buffer of skin matrices
		std::unordered_map<gr::RenderDataID, std::shared_ptr<re::Buffer>> m_meshIDToSkinJoints;


	private:
		// We maintain the shared_ptr<re::Buffer> lifetime, and also prepare VertexBufferInputs that can be used for
		// Batch construction by other GS's
		struct AnimationBuffers
		{
			std::array<std::shared_ptr<re::Buffer>, gr::VertexStream::k_maxVertexStreams> m_destBuffers;
			std::shared_ptr<re::Buffer> m_morphMetadataBuffer;

			std::shared_ptr<re::Buffer> m_skinningDataBuffer;

			uint8_t m_numAnimatedStreams = 0;
		};
		std::unordered_map<gr::RenderDataID, AnimationBuffers> m_meshPrimIDToAnimBuffers;

		
		// Maintain this seperately: We share it with other GS's as a dependency output
		// Note: The contents here are always valid, even if the associated Batch did not pass culling. However,
		// the buffers are only actually updated/animated if the batch passed culling
		AnimatedVertexStreams m_outputs;

		void AddAnimationBuffers(gr::RenderDataID, gr::MeshPrimitive::RenderData const&);
		void RemoveAnimationBuffers(gr::RenderDataID);


	private:
		ViewCullingResults const* m_viewCullingResults; // From the Culling GS: We only animate things that pass culling
	};
}