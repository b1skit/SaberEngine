// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "NamedObject.h"
#include "RenderStage.h"
#include "RenderPipeline.h"
#include "TextUtils.h"


namespace re
{
	class RenderSystem;
	class TextureTargetSet;
}

namespace gr
{
	class GraphicsSystemManager;


	class GraphicsSystem : public virtual en::NamedObject
	{
	public:
		// Scriptable pipeline: Create a graphics system by the (case insensitive) name provided in a script.
		// Returns null if no GS with that name exists
		static std::shared_ptr<gr::GraphicsSystem> CreateByName(char const* scriptName, gr::GraphicsSystemManager*);
		static std::shared_ptr<gr::GraphicsSystem> CreateByName(std::string const& scriptName, gr::GraphicsSystemManager*);

		// GraphicsSystem interface:
		// -------------------------
		// GraphicsSystems intentionally have a flexible interface with minimal required virtual functionality.
		// Typically, a Graphics System will require 1 or more of the following functions:
		// - InitPipeline(re::StagePipeline&) method(s): Used to attach a sequence of RenderStages to a StagePipeline
		// - PreRender() method(s): Called every frame to update the GraphicsSystem before platform-level rendering
		// 
		// ------------------------
		// 
		// To participate in the self-enrolling scriptable rendering pipeline, a GraphicsSystem must:
		//	a) Inherit from the IScriptableGraphicsSystem interface
		//	b) Implement a "static constexpr char const* GetScriptName()" member function
		//	c) Provide an implementation of the pure virtual GetRuntimeBindings() function.
		//		-> Use the macros at the end of this file to reduce boilerplate
	public:
		struct RuntimeBindings
		{
			using InitPipelineFn = std::function<void(re::StagePipeline&)>;
			using PreRenderFn = std::function<void()>;

			std::map<std::string, InitPipelineFn> m_initPipelineFunctions;
			std::map<std::string, PreRenderFn> m_preRenderFunctions;
		};
		virtual RuntimeBindings GetRuntimeBindings() = 0;


	public:
		GraphicsSystem(GraphicsSystem&&) = default;
		GraphicsSystem& operator=(GraphicsSystem&&) = default;


		// TODO: We should have inputs and outputs, to allow data flow between GraphicsSystems to be configured externally
		virtual std::shared_ptr<re::TextureTargetSet const> GetFinalTextureTargetSet() const = 0;

		virtual void ShowImGuiWindow(); // Override this

	private:
		virtual void CreateBatches() = 0;


	protected:
		gr::GraphicsSystemManager* const m_graphicsSystemManager;


	protected:
		explicit GraphicsSystem(char const* name, gr::GraphicsSystemManager*);


	private: // No copying allowed
		GraphicsSystem() = delete;
		GraphicsSystem(GraphicsSystem const&) = delete;
		GraphicsSystem& operator=(GraphicsSystem const&) = delete;


	protected:
		// Automatic factory registration mechanism:
		template<typename T>
		static std::shared_ptr<gr::GraphicsSystem> Create(gr::GraphicsSystemManager* gsm);

		
		using CreateFn = std::shared_ptr<gr::GraphicsSystem>(*)(gr::GraphicsSystemManager*);
		static bool RegisterGS(char const* scriptName, CreateFn);


	private:
		static std::unique_ptr<std::map<std::string, CreateFn>> s_gsCreateFunctions;
		static std::unique_ptr<std::mutex> s_gsCreateFunctionsMutex;
		

	private:
		template<typename T>
		friend class IScriptableGraphicsSystem;
	};


	template<typename T>
	std::shared_ptr<gr::GraphicsSystem> gr::GraphicsSystem::Create(gr::GraphicsSystemManager* gsm)
	{
		std::shared_ptr<gr::GraphicsSystem> newGS;
		newGS.reset(new T(gsm));
		return newGS;
	}


	// Scriptable rendering pipeline self-registration interface
	// By inheriting from this interface, a GraphicsSystem will automatically be able to be created by name
	template<typename T>
	class IScriptableGraphicsSystem
	{
	public:
		// Use a static_cast to prevent the compiler from optimizing our static registration variable away
		IScriptableGraphicsSystem() { static_cast<void*>(&s_isRegistered); }

	private:
		static bool s_isRegistered;
	};


	template<typename T>
	bool IScriptableGraphicsSystem<T>::s_isRegistered =
		GraphicsSystem::RegisterGS(T::GetScriptName(), gr::GraphicsSystem::Create<T>);
}


// Helper macros: Cut down on the boilerplate required to build maps of runtime functions callable by name

#define RETURN_RUNTIME_BINDINGS(...) \
	return gr::GraphicsSystem::RuntimeBindings{__VA_ARGS__};

#define INIT_PIPELINE(...) \
	.m_initPipelineFunctions = {__VA_ARGS__},

#define PRE_RENDER(...) \
	.m_preRenderFunctions = {__VA_ARGS__},

#define INIT_PIPELINE_FN(gsClassName, memberFuncName) \
	{util::ToLower(#memberFuncName), std::bind(&gsClassName::memberFuncName, this, std::placeholders::_1)}

#define PRE_RENDER_FN(gsClassName, memberFuncName) \
	{util::ToLower(#memberFuncName), std::bind(&gsClassName::memberFuncName, this)},