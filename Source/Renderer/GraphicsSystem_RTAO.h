// © 2025 Adam Badke. All rights reserved.
#pragma once
#include "Effect.h"
#include "GraphicsSystem.h"

#include "Core/InvPtr.h"

#include "Core/Util/CHashKey.h"


namespace re
{
	class AccelerationStructure;
	class Texture;
}
namespace gr
{
	class Stage;


	class RTAOGraphicsSystem final
		: public virtual GraphicsSystem
		, public virtual IScriptableGraphicsSystem<RTAOGraphicsSystem>
	{
	public:
		static constexpr char const* GetScriptName() { return "RTAO"; }

		gr::GraphicsSystem::RuntimeBindings GetRuntimeBindings() override
		{
			RETURN_RUNTIME_BINDINGS
			(
				INIT_PIPELINE(INIT_PIPELINE_FN(RTAOGraphicsSystem, InitPipeline))
				PRE_RENDER(PRE_RENDER_FN(RTAOGraphicsSystem, PreRender))
			);
		}

		static constexpr util::CHashKey k_depthInput = "SceneDepth";
		static constexpr util::CHashKey k_wNormalInput = "SceneWNormal";		
		static constexpr util::CHashKey k_sceneTLASInput = "SceneTLAS";
		void RegisterInputs() override;

		static constexpr util::CHashKey k_aoOutput = "RTAOTex";
		void RegisterOutputs() override;


	public:
		RTAOGraphicsSystem(gr::GraphicsSystemManager*);

		~RTAOGraphicsSystem() = default;

		void InitPipeline(gr::StagePipeline&, TextureDependencies const&, BufferDependencies const&, DataDependencies const&);
		void PreRender();


	public:
		void ShowImGuiWindow() override;


	private:
		std::shared_ptr<gr::Stage> m_RTAOStage;
		core::InvPtr<re::Texture> m_workingAOTex;

		gr::StagePipeline* m_stagePipeline;

		core::InvPtr<re::Texture> const* m_depthInput;
		core::InvPtr<re::Texture> const* m_wNormalInput;		

		std::shared_ptr<re::AccelerationStructure> const* m_sceneTLAS;

		EffectID m_RTAOEffectID;

		uint8_t m_geometryInstanceMask;

		// RTAO parameters:
		bool m_isDirty;
		glm::vec2 m_tMinMax; // Min and max ray interval distance

		std::shared_ptr<re::Buffer> m_RTAOParams;
	};
}