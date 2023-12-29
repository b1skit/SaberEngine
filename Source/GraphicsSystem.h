// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "NamedObject.h"
#include "RenderStage.h"
#include "CommandQueue.h"
#include "RenderPipeline.h"
#include "TextureTarget.h"


namespace re
{
	class RenderSystem;
}

namespace gr
{
	class GraphicsSystemManager;


	class GraphicsSystem : public virtual en::NamedObject
	{
	public:
		explicit GraphicsSystem(std::string const& name, gr::GraphicsSystemManager*);

		GraphicsSystem(GraphicsSystem&&) = default;
		GraphicsSystem& operator=(GraphicsSystem&&) = default;


		// GraphicsSystem interface:
		// -------------------------
		// We don't enforce a strict virtual interface here to allow flexibility in graphics system creation & updates.
		// Typically, a Graphics System will require 1 or more of the following functions:
		// - Create(re::RenderSystem&, re::StagePipeline&) methods: Used to attach a sequence of RenderStages to a StagePipeline
		// - PreRender() methods: Called every frame to update the GraphicsSystem before platform-level rendering


		// TODO: We should have inputs and outputs, to allow data flow between GraphicsSystems to be configured externally
		virtual std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const = 0;

		virtual void ShowImGuiWindow(); // Override this

	private:
		virtual void CreateBatches() = 0;


	protected:
		gr::GraphicsSystemManager* const m_owningGraphicsSystemManager;


	private: // No copying allowed
		GraphicsSystem() = delete;
		GraphicsSystem(GraphicsSystem const&) = delete;
		GraphicsSystem& operator=(GraphicsSystem const&) = delete;
	};
}