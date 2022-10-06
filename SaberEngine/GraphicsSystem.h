#pragma once

#include <string>

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

		GraphicsSystem() = delete;
		GraphicsSystem(GraphicsSystem const&) = delete;
		GraphicsSystem(GraphicsSystem&&) = delete;

		// GraphicsSystem interface:
		virtual void Create(re::StagePipeline& pipeline) = 0; // Initial Graphics System setup
		virtual void PreRender(re::StagePipeline& pipeline) = 0; // Called every frame

		virtual gr::TextureTargetSet& GetFinalTextureTargetSet() = 0;
		virtual gr::TextureTargetSet const& GetFinalTextureTargetSet() const = 0;
	};
}