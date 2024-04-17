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
		static std::unique_ptr<gr::GraphicsSystem> CreateByName(char const* scriptName, gr::GraphicsSystemManager*);
		static std::unique_ptr<gr::GraphicsSystem> CreateByName(std::string const& scriptName, gr::GraphicsSystemManager*);

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
		//	d) Provide an implementation of the pure virtual RegisterTextureInputs/Outputs() functions
		//		-> These are called before/after InitPipelineFn execution
	public:
		using TextureDependencies = std::map<std::string, std::shared_ptr<re::Texture>>;
		struct RuntimeBindings
		{
			using InitPipelineFn = std::function<void(re::StagePipeline&, TextureDependencies)>;
			using PreRenderFn = std::function<void()>;

			std::map<std::string, InitPipelineFn> m_initPipelineFunctions;
			std::map<std::string, PreRenderFn> m_preRenderFunctions;
		};
		virtual RuntimeBindings GetRuntimeBindings() = 0;


	public:
		enum TextureInputDefault : uint8_t // Optional generic fallbacks
		{
			OpaqueWhite,		// (1,1,1,1)
			TransparentWhite,	// (1,1,1,0)
			OpaqueBlack,		// (0,0,0,1)
			TransparentBlack,	// (0,0,0,0)
			
			None,				// Default
			TextureInputDefault_Count
		};

		TextureInputDefault GetTextureInputDefaultType(std::string const& inputScriptName) const;

		std::shared_ptr<re::Texture> GetTextureOutput(std::string const& outputScriptName) const;

		virtual void RegisterTextureInputs() = 0;
		virtual void RegisterTextureOutputs() = 0;

	protected:
		void RegisterTextureInput(char const*, TextureInputDefault = TextureInputDefault::None);
		void RegisterTextureOutput(char const*, std::shared_ptr<re::Texture>);
		

	private:
		// These maps must be populated during the call to ConfigureInput/OutputDependencies()
		std::map<std::string, TextureInputDefault> m_textureInputs;
		std::map<std::string, std::shared_ptr<re::Texture>> m_textureOutputs;


	public:
		GraphicsSystem(GraphicsSystem&&) = default;
		GraphicsSystem& operator=(GraphicsSystem&&) = default;

		virtual ~GraphicsSystem() = default;

		virtual void ShowImGuiWindow(); // Override this


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
		static std::unique_ptr<gr::GraphicsSystem> Create(gr::GraphicsSystemManager* gsm);

		
		using CreateFn = std::unique_ptr<gr::GraphicsSystem>(*)(gr::GraphicsSystemManager*);
		static bool RegisterGS(char const* scriptName, CreateFn);


	private:
		static std::unique_ptr<std::map<std::string, CreateFn>> s_gsCreateFunctions;
		static std::unique_ptr<std::mutex> s_gsCreateFunctionsMutex;
		

	private:
		template<typename T>
		friend class IScriptableGraphicsSystem; // Required to access GraphicsSystem::Create()
	};


	template<typename T>
	std::unique_ptr<gr::GraphicsSystem> GraphicsSystem::Create(gr::GraphicsSystemManager* gsm)
	{
		std::unique_ptr<gr::GraphicsSystem> newGS;
		newGS.reset(new T(gsm));
		return newGS;
	}


	inline GraphicsSystem::TextureInputDefault GraphicsSystem::GetTextureInputDefaultType(
		std::string const& inputScriptName) const
	{
		SEAssert(m_textureInputs.contains(inputScriptName), "Texture input with that name has not been registered");
		return m_textureInputs.at(inputScriptName);
	}


	inline std::shared_ptr<re::Texture> GraphicsSystem::GetTextureOutput(std::string const& scriptName) const
	{
		// Note: It's possible for GS's with multiple initialization steps to hit this if its first initialization step
		// runs before a GS it is dependent on has been initialized (because we (currently) just initialize in the order
		// the initialization steps are defined in)
		SEAssert(m_textureOutputs.contains(scriptName),
			"No texture output with the given script name was registered by the GS");
		return m_textureOutputs.at(scriptName);
	}


	inline void GraphicsSystem::RegisterTextureInput(
		char const* scriptName, TextureInputDefault fallbackDefault /*= TextureInputDefault::None*/)
	{
		// This function might be called multiple times by the same GS (e.g. if it has multiple initialization steps)
		SEAssert(!m_textureInputs.contains(scriptName) || m_textureInputs.at(scriptName) == fallbackDefault,
			"Texture dependency has already been registered and populated");
		m_textureInputs.emplace(scriptName, fallbackDefault);
	}


	inline void GraphicsSystem::RegisterTextureOutput(char const* scriptName, std::shared_ptr<re::Texture> outputTex)
	{
		// Note: It's possible for the texture to be null here (e.g. for GS's with multiple initialization steps). This
		// is fine long as everything exists the last time a GS calls this function.
		m_textureOutputs[scriptName] = outputTex;
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
	{util::ToLower(#memberFuncName), std::bind(&gsClassName::memberFuncName, this, std::placeholders::_1, std::placeholders::_2)}

#define PRE_RENDER_FN(gsClassName, memberFuncName) \
	{util::ToLower(#memberFuncName), std::bind(&gsClassName::memberFuncName, this)},