// © 2023 Adam Badke. All rights reserved.
#include "ImGuiUtils.h"
#include "GraphicsSystem.h"
#include "ProfilingMarkers.h"
#include "RenderSystem.h"
#include "SceneManager.h"


namespace
{
	gr::GraphicsSystem::TextureDependencies BuildTextureDependencies(
		std::string const& dstGSScriptName,
		re::RenderPipelineDesc::RenderSystemDescription const& renderSysDesc,
		gr::GraphicsSystemManager const& gsm)
	{
		gr::GraphicsSystem::TextureDependencies texDependencies;

		// It's possible our GS doesn't have any input dependencies
		if (renderSysDesc.m_textureInputs.contains(dstGSScriptName))
		{
			auto const& gsTexDependencies = renderSysDesc.m_textureInputs.at(dstGSScriptName);

			// Iterate over each GS in our dependency list:
			for (auto const& srcEntry : gsTexDependencies)
			{
				std::string const& srcGSScriptName = srcEntry.first;
				gr::GraphicsSystem* srcGS = gsm.GetGraphicsSystemByScriptName(srcGSScriptName);

				if (srcGS)
				{
					// Get each of the texture dependencies from the source GS:
					for (auto const& dependencySrcDstNameMapping : srcEntry.second)
					{
						std::string const& srcName = dependencySrcDstNameMapping.first;
						std::string const& dstName = dependencySrcDstNameMapping.second;
						SEAssert(srcGS->GetTextureOutput(srcName),
							"Source GS hasn't created the required texture");
						texDependencies.emplace(dstName, srcGS->GetTextureOutput(srcName));
					}
				}
				else
				{
					// Source GS doesn't exist. Attempt to use a default texture as a fallback
					gr::GraphicsSystem* dstGS = gsm.GetGraphicsSystemByScriptName(dstGSScriptName);

					fr::SceneData* sceneData = fr::SceneManager::GetSceneData();
					
					for (auto const& dependencySrcDstNameMapping : srcEntry.second)
					{
						std::string const& srcName = dependencySrcDstNameMapping.first;
						std::string const& dstName = dependencySrcDstNameMapping.second;
						
						gr::GraphicsSystem::TextureInputDefault inputDefault = 
							dstGS->GetTextureInputDefaultType(dstName);

						switch (inputDefault)
						{
						case gr::GraphicsSystem::TextureInputDefault::OpaqueWhite:
						{
							texDependencies.emplace(
								dstName, 
								sceneData->GetTexture(en::DefaultResourceNames::k_opaqueWhiteDefaultTexName));
						}
						break;
						case gr::GraphicsSystem::TextureInputDefault::TransparentWhite:
						{
							texDependencies.emplace(
								dstName,
								sceneData->GetTexture(en::DefaultResourceNames::k_transparentWhiteDefaultTexName));
						}
						break;
						case gr::GraphicsSystem::TextureInputDefault::OpaqueBlack:
						{
							texDependencies.emplace(
								dstName,
								sceneData->GetTexture(en::DefaultResourceNames::k_opaqueBlackDefaultTexName));
						}
						break;
						case gr::GraphicsSystem::TextureInputDefault::TransparentBlack:
						{
							texDependencies.emplace(
								dstName,
								sceneData->GetTexture(en::DefaultResourceNames::k_transparentBlackDefaultTexName));
						}
						break;
						case gr::GraphicsSystem::TextureInputDefault::None:
						{
							SEAssertF("Couldn't find a source GS, and no default input has been specified");
							continue;
						}
						default: SEAssertF("Invalid TextureInputDefault");
						}
					}
				}
			}
		}

		return texDependencies;
	}
}

namespace re
{
	std::unique_ptr<RenderSystem> RenderSystem::Create(std::string const& name)
	{
		std::unique_ptr<RenderSystem> newRenderSystem = nullptr;

		newRenderSystem.reset(new RenderSystem(name));

		return std::move(newRenderSystem);
	}


	RenderSystem::RenderSystem(std::string const& name)
		: NamedObject(name)
		, m_graphicsSystemManager(this)
		, m_renderPipeline(name + " render pipeline")
		, m_initPipeline(nullptr)
	{
	}


	void RenderSystem::Destroy()
	{
		m_graphicsSystemManager.Destroy();
		m_renderPipeline.Destroy();
		m_initPipeline = nullptr;
		m_updatePipeline.clear();
	}


	void RenderSystem::BuildPipeline(RenderPipelineDesc::RenderSystemDescription const& renderSysDesc)
	{
		// Create the GS creation pipeline:
		m_creationPipeline = [this, renderSysDesc](re::RenderSystem* renderSystem)
		{
			gr::GraphicsSystemManager& gsm = renderSystem->GetGraphicsSystemManager();

			for (std::string const& gsName : renderSysDesc.m_graphicsSystemNames)
			{
				gsm.CreateAddGraphicsSystemByScriptName(gsName);
			}

			// The update pipeline caches member function pointers; We can populate it now that our GS objects exist
			for (auto const& entry : renderSysDesc.m_updateSteps)
			{
				gr::GraphicsSystem* currentGS = gsm.GetGraphicsSystemByScriptName(entry.first);

				std::string const& lowercaseScriptFnName(util::ToLower(entry.second));

				m_updatePipeline.emplace_back(
					currentGS->GetRuntimeBindings().m_preRenderFunctions.at(lowercaseScriptFnName));
			}
		};

		m_initPipeline = [renderSysDesc](re::RenderSystem* renderSystem)
		{
			gr::GraphicsSystemManager& gsm = renderSystem->GetGraphicsSystemManager();
			gsm.Create();

			re::RenderPipeline& renderPipeline = renderSystem->GetRenderPipeline();

			for (auto const& entry : renderSysDesc.m_initSteps)
			{
				std::string const& currentGSScriptName = entry.first;
				gr::GraphicsSystem* currentGS = gsm.GetGraphicsSystemByScriptName(currentGSScriptName);

				currentGS->RegisterTextureInputs();

				std::string const& lowercaseScriptFnName(util::ToLower(entry.second));
				std::string const& stagePipelineName = std::format("{} stages", currentGS->GetName());

				std::map<std::string, std::shared_ptr<re::Texture>> const& textureInputs = 
					BuildTextureDependencies(currentGSScriptName, renderSysDesc, gsm);

				currentGS->GetRuntimeBindings().m_initPipelineFunctions.at(lowercaseScriptFnName)(
					renderPipeline.AddNewStagePipeline(stagePipelineName),
					textureInputs);

				// Now the GS is initialized, it can populate its resource dependencies for other GS's
				currentGS->RegisterTextureOutputs();
			}
		};

		// We can only build the update pipeline once the GS's have been created (as we need to access their runtime
		// bindings)
		m_updatePipeline.reserve(renderSysDesc.m_updateSteps.size());
	}


	void RenderSystem::ExecuteCreationPipeline()
	{
		m_creationPipeline(this);
	}


	void RenderSystem::ExecuteInitializationPipeline()
	{
		m_initPipeline(this);
	}


	void RenderSystem::ExecuteUpdatePipeline()
	{
		SEBeginCPUEvent(std::format("{} ExecuteUpdatePipeline", GetName()).c_str());

		m_graphicsSystemManager.PreRender();

		for (auto executeStep : m_updatePipeline)
		{
			executeStep();
		}

		SEEndCPUEvent();
	}


	void RenderSystem::ShowImGuiWindow()
	{
		if (ImGui::CollapsingHeader(std::format("Graphics System Manager##", util::PtrToID(this)).c_str(), 
			ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent();
			m_graphicsSystemManager.ShowImGuiWindow();
			ImGui::Unindent();
		}
	}
}