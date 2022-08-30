#pragma once

#include <string>

#include "RenderStage.h"
#include "TextureTarget.h"
#include "RenderPipeline.h"


namespace gr
{
	class GraphicsSystem
	{
	public:
		GraphicsSystem(std::string name);

		GraphicsSystem() = delete;
		GraphicsSystem(GraphicsSystem const&) = delete;
		GraphicsSystem(GraphicsSystem&&) = delete;

		// Virtual members:
		virtual ~GraphicsSystem() = 0;
		virtual void Create(gr::RenderPipeline& pipeline) = 0; // Initial Graphics System setup
		virtual void PreRender() = 0; // Called every frame

		virtual gr::TextureTargetSet& GetTextureTargetSet() = 0;
		virtual gr::TextureTargetSet const& GetTextureTargetSet() const = 0;

		// Getters/setters:
		inline std::string const& GetName() { return m_name; }
		

	private:
		std::string m_name;
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline GraphicsSystem::~GraphicsSystem() {}
}