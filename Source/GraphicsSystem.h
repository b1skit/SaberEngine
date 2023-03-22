// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "RenderStage.h"
#include "TextureTarget.h"
#include "RenderPipeline.h"
#include "NamedObject.h"


namespace gr
{
	class GraphicsSystem : public virtual en::NamedObject
	{
	public:
		explicit GraphicsSystem(std::string const& name);

		// GraphicsSystem interface:
		virtual void Create(re::StagePipeline& pipeline) = 0; // Initial Graphics System setup
		virtual void PreRender(re::StagePipeline& pipeline) = 0; // Called every frame

		virtual std::shared_ptr<re::TextureTargetSet> GetFinalTextureTargetSet() const = 0;


	private:
		virtual void CreateBatches() = 0;


	private: // No copying allowed
		GraphicsSystem() = delete;
		GraphicsSystem(GraphicsSystem const&) = delete;
		GraphicsSystem(GraphicsSystem&&) = delete;
		GraphicsSystem& operator=(GraphicsSystem const&) = delete;
	};
}