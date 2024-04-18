// © 2023 Adam Badke. All rights reserved.
#include "ImGuiUtils.h"
#include "GraphicsSystem.h"
#include "ProfilingMarkers.h"
#include "RenderSystem.h"
#include "SceneManager.h"


namespace
{
	gr::GraphicsSystem::TextureDependencies ResolveTextureDependencies(
		std::string const& dstGSScriptName,
		re::RenderPipelineDesc::RenderSystemDescription const& renderSysDesc,
		gr::GraphicsSystemManager const& gsm)
	{
		gr::GraphicsSystem::TextureDependencies texDependencies;

		// It's possible our GS doesn't have any input dependencies
		if (renderSysDesc.m_textureInputs.contains(dstGSScriptName))
		{
			auto const& gsTexDependencies = renderSysDesc.m_textureInputs.at(dstGSScriptName);

			gr::GraphicsSystem* dstGS = gsm.GetGraphicsSystemByScriptName(dstGSScriptName);

			// Iterate over each GS in our dependency list:
			for (auto const& srcEntry : gsTexDependencies)
			{
				std::string const& srcGSScriptName = srcEntry.first;
				gr::GraphicsSystem* srcGS = gsm.GetGraphicsSystemByScriptName(srcGSScriptName);

				for (auto const& dependencySrcDstNameMapping : srcEntry.second)
				{
					std::string const& srcName = dependencySrcDstNameMapping.first;
					
					std::string const& dstName = dependencySrcDstNameMapping.second;
					SEAssert(dstGS->HasTextureInput(dstName), "Destination GS hasn't registered this input name");

					if (srcGS)
					{
						texDependencies.emplace(dstName, srcGS->GetTextureOutput(srcName));
					}
					else
					{
						// Source GS doesn't exist. Attempt to use a default texture as a fallback
						fr::SceneData* sceneData = fr::SceneManager::GetSceneData();

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


	std::unordered_map<std::string, void const*> ResolveDataDependencies(
		std::string const& dstGSScriptName,
		re::RenderPipelineDesc::RenderSystemDescription const& renderSysDesc,
		gr::GraphicsSystemManager const& gsm)
	{
		using GSName = re::RenderPipelineDesc::RenderSystemDescription::GSName;
		using SrcDstNamePairs = re::RenderPipelineDesc::RenderSystemDescription::SrcDstNamePairs;

		std::unordered_map<std::string, void const*> resolvedDependencies;

		if (renderSysDesc.m_dataInputs.contains(dstGSScriptName))
		{
			std::vector<std::pair<GSName, SrcDstNamePairs>> const& gsDependencies =
				renderSysDesc.m_dataInputs.at(dstGSScriptName);

			gr::GraphicsSystem const* dstGS = gsm.GetGraphicsSystemByScriptName(dstGSScriptName);

			for (auto const& curDependency : gsDependencies)
			{
				std::string const& srcGSName = curDependency.first;

				gr::GraphicsSystem const* srcGS = gsm.GetGraphicsSystemByScriptName(srcGSName);

				for (auto const& srcDstNames : curDependency.second)
				{
					std::string const& dependencyDstName = srcDstNames.second;
					SEAssert(dstGS->HasDataInput(dependencyDstName), "No input with the given name has been registered");

					std::string const& dependencySrcName = srcDstNames.first;
					resolvedDependencies.emplace(dependencyDstName, srcGS->GetDataOutput(dependencySrcName));
				}
			}
		}

		return resolvedDependencies;
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
		// Construct the GS creation pipeline:
		m_creationPipeline = [this, renderSysDesc](re::RenderSystem* renderSystem)
		{
			gr::GraphicsSystemManager& gsm = renderSystem->GetGraphicsSystemManager();

			for (std::string const& gsName : renderSysDesc.m_graphicsSystemNames)
			{
				gsm.CreateAddGraphicsSystemByScriptName(gsName);
			}
		};

		m_initPipeline = [this, renderSysDesc](re::RenderSystem* renderSystem)
		{
			gr::GraphicsSystemManager& gsm = renderSystem->GetGraphicsSystemManager();
			gsm.Create();

			re::RenderPipeline& renderPipeline = renderSystem->GetRenderPipeline();

			for (auto const& entry : renderSysDesc.m_initSteps)
			{
				std::string const& currentGSScriptName = entry.first;
				gr::GraphicsSystem* currentGS = gsm.GetGraphicsSystemByScriptName(currentGSScriptName);

				currentGS->RegisterInputs();

				std::string const& lowercaseScriptFnName(util::ToLower(entry.second));
				std::string const& stagePipelineName = std::format("{} stages", currentGS->GetName());

				std::map<std::string, std::shared_ptr<re::Texture>> const& textureInputs = 
					ResolveTextureDependencies(currentGSScriptName, renderSysDesc, gsm);

				currentGS->GetRuntimeBindings().m_initPipelineFunctions.at(lowercaseScriptFnName)(
					renderPipeline.AddNewStagePipeline(stagePipelineName),
					textureInputs);

				// Now the GS is initialized, it can populate its resource dependencies for other GS's
				currentGS->RegisterOutputs();
			}

			// The update pipeline caches member function and data pointers; We can only populate it once our GS's are
			// created & initialized
			for (auto const& entry : renderSysDesc.m_updateSteps)
			{
				std::string const& currentGSName = entry.first;

				gr::GraphicsSystem* currentGS = gsm.GetGraphicsSystemByScriptName(currentGSName);

				std::string const& lowercaseScriptFnName = util::ToLower(entry.second);

				UpdateStep& updateStep = m_updatePipeline.emplace_back(UpdateStep{
					.m_preRenderFunc = currentGS->GetRuntimeBindings().m_preRenderFunctions.at(lowercaseScriptFnName),
					.m_resolvedDependencies = ResolveDataDependencies(currentGSName, renderSysDesc, gsm)});
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

		for (auto& updateStep : m_updatePipeline)
		{
			updateStep.m_preRenderFunc(updateStep.m_resolvedDependencies);
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