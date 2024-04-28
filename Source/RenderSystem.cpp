// © 2023 Adam Badke. All rights reserved.
#include "GraphicsSystem.h"
#include "ProfilingMarkers.h"
#include "RenderSystem.h"
#include "RenderSystemDesc.h"
#include "SceneManager.h"
#include "Core\ThreadPool.h"

#include "Core\Util\ImGuiUtils.h"

using GSName = re::RenderSystemDescription::GSName;
using SrcDstNamePairs = re::RenderSystemDescription::SrcDstNamePairs;


namespace
{
	gr::GraphicsSystem::TextureDependencies ResolveTextureDependencies(
		std::string const& dstGSScriptName,
		re::RenderSystemDescription const& renderSysDesc,
		gr::GraphicsSystemManager const& gsm)
	{
		gr::GraphicsSystem::TextureDependencies texDependencies;

		gr::GraphicsSystem* dstGS = gsm.GetGraphicsSystemByScriptName(dstGSScriptName);

		// Initialize everything with a nullptr, incase no input is described
		for (auto const& input : dstGS->GetTextureInputs())
		{
			texDependencies.emplace(input.first, nullptr);
		}

		// It's possible our GS doesn't have any input dependencies
		if (renderSysDesc.m_textureInputs.contains(dstGSScriptName))
		{
			auto const& gsTexDependencies = renderSysDesc.m_textureInputs.at(dstGSScriptName);

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
						texDependencies[dstName] = srcGS->GetTextureOutput(srcName);
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
							texDependencies[dstName] =
								sceneData->GetTexture(en::DefaultResourceNames::k_opaqueWhiteDefaultTexName);
						}
						break;
						case gr::GraphicsSystem::TextureInputDefault::TransparentWhite:
						{
							texDependencies[dstName] =
								sceneData->GetTexture(en::DefaultResourceNames::k_transparentWhiteDefaultTexName);
						}
						break;
						case gr::GraphicsSystem::TextureInputDefault::OpaqueBlack:
						{
							texDependencies[dstName] =
								sceneData->GetTexture(en::DefaultResourceNames::k_opaqueBlackDefaultTexName);
						}
						break;
						case gr::GraphicsSystem::TextureInputDefault::TransparentBlack:
						{
							texDependencies[dstName] =
								sceneData->GetTexture(en::DefaultResourceNames::k_transparentBlackDefaultTexName);
						}
						break;
						case gr::GraphicsSystem::TextureInputDefault::None:
						{
							continue; // We've already initialized the entry as a nullptr
						}
						break;
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
		re::RenderSystemDescription const& renderSysDesc,
		gr::GraphicsSystemManager const& gsm)
	{
		std::unordered_map<std::string, void const*> resolvedDependencies;

		gr::GraphicsSystem const* dstGS = gsm.GetGraphicsSystemByScriptName(dstGSScriptName);

		// Initialize everything with a nullptr, incase no input is described
		for (auto const& input : dstGS->GetDataInputs())
		{
			resolvedDependencies.emplace(input, nullptr);
		}

		// Process any data inputs assigned to the current destination GraphicsSystem:
		if (renderSysDesc.m_dataInputs.contains(dstGSScriptName))
		{
			std::vector<std::pair<GSName, SrcDstNamePairs>> const& gsDependencies =
				renderSysDesc.m_dataInputs.at(dstGSScriptName);

			for (auto const& curDependency : gsDependencies)
			{
				std::string const& srcGSName = curDependency.first;

				gr::GraphicsSystem const* srcGS = gsm.GetGraphicsSystemByScriptName(srcGSName);
				SEAssert(srcGS, "Source GraphicsSystem could not be found");

				for (auto const& srcDstNames : curDependency.second)
				{
					std::string const& dependencyDstName = srcDstNames.second;
					SEAssert(dstGS->HasDataInput(dependencyDstName), "No input with the given name has been registered");

					std::string const& dependencySrcName = srcDstNames.first;
					resolvedDependencies[dependencyDstName] = srcGS->GetDataOutput(dependencySrcName);
				}
			}
		}

		return resolvedDependencies;
	}
	
	
	void ComputeExecutionGroups(
		re::RenderSystemDescription const& renderSysDesc,
		std::vector<std::vector<std::string>>& updateExecutionGroups,
		bool singleThreadGSExecution)
	{
		// Note: Creation order doesn't matter, only initialization and updates are order-dependent

		if (singleThreadGSExecution)
		{
			// Output the exact ordering received in the pipeline description. It's up to the user to ensure these
			// orderings are valid
			for (auto const& pipelineStep : renderSysDesc.m_pipelineOrder)
			{
				// Add each step in sequence so we get serial ordering with no overlap:
				std::vector<std::string>& currentStep = updateExecutionGroups.emplace_back();
				currentStep.emplace_back(pipelineStep);
			}
		}
		else
		{
			// Create a list of GS's and their dependencies
			struct GSDependencies
			{
				std::string const* m_gsName;
				std::unordered_set<std::string> m_dependencies; // Script names of GS's we're dependent on

				bool operator>(GSDependencies const& rhs) const { return m_dependencies.size() > rhs.m_dependencies.size(); }
				bool operator<(GSDependencies const& rhs) const { return m_dependencies.size() < rhs.m_dependencies.size(); }
			};
			std::vector<GSDependencies> gsDependencies;
		
			// Build a list of dependencies for each GS:
			for (auto const& currentGSName : renderSysDesc.m_pipelineOrder)
			{
				GSDependencies curGSDependencies;
				curGSDependencies.m_gsName = &currentGSName;

				auto PopulateDependencies = [&currentGSName, &curGSDependencies, &renderSysDesc](
					std::unordered_map<GSName, std::vector<std::pair<GSName, SrcDstNamePairs>>> const& inputs)
					{
						if (inputs.contains(currentGSName))
						{
							for (auto const& srcGS : inputs.at(currentGSName))
							{
								std::string const& srcGSName = srcGS.first;

								// Only add the dependency if it's one of the active graphics systems. It's possible
								// we'll have an input (e.g. texture dependency) for a GS that doesn't exist/is excluded
								if (renderSysDesc.m_graphicsSystemNames.contains(srcGSName))
								{
									curGSDependencies.m_dependencies.emplace(srcGSName);
								}
							}
						}
					};
				// If enabled, this will consider texture inputs as a dependency for computing the GraphicsSystem update
				// order. This should never be necessary (as textures are updated on the GPU), but is useful for debugging
//#define CONSIDER_TEX_INPUTS_AS_UPDATE_DEPENDENCIES
#if defined(CONSIDER_TEX_INPUTS_AS_UPDATE_DEPENDENCIES)
				PopulateDependencies(renderSysDesc.m_textureInputs);
#endif
				PopulateDependencies(renderSysDesc.m_dataInputs);

				gsDependencies.emplace_back(std::move(curGSDependencies));
			}

			// Compute neighboring groups of GS's that can be executed in parallel:
			std::vector<std::vector<std::string const*>> executionGroups;
			size_t startIdx = 0;
			while (startIdx < gsDependencies.size())
			{
				// If enabled, a GraphicsSystem's update functionality can be executed before other GS's in the pipeline
				// description when their dependencies allow it. This is desireable, but we can toggle it for debugging
#define ALLOW_UPDATE_EXECUTION_REORDERING
#if defined ALLOW_UPDATE_EXECUTION_REORDERING
				std::sort(gsDependencies.begin(), gsDependencies.end(), std::less<GSDependencies>());
#endif
				std::vector<std::string const*>& curExecutionGroupGSNames = executionGroups.emplace_back();

				// All sequentially declared GS's with 0 dependencies can be executed in parallel:
				size_t curIdx = startIdx;
				while (curIdx < gsDependencies.size() && gsDependencies[curIdx].m_dependencies.size() == 0)
				{
					curExecutionGroupGSNames.emplace_back(gsDependencies[curIdx].m_gsName);
					curIdx++;
				}
				SEAssert(curIdx > startIdx,
					"Failed to find a GS with 0 dependencies. This suggests the declared GS ordering is invalid");

				// Prune the current execution group from the remaining dependencies, and rebuild the priority queue:
				size_t updateIdx = curIdx;
				while (updateIdx < gsDependencies.size())
				{
					for (auto const& curExecutionGrpGS : curExecutionGroupGSNames)
					{
						gsDependencies[updateIdx].m_dependencies.erase(*curExecutionGrpGS); // No-op if key doesn't exist
					}
					updateIdx++;
				}

				// Prepare for the next iteration:
				startIdx = curIdx;
			}

			// Finally, populate the output
			for (auto const& executionGrp : executionGroups)
			{
				std::vector<std::string>& updateStepGSNameFnName = updateExecutionGroups.emplace_back();

				for (auto const& gsName : executionGrp) // Add all update steps in the order they're declared
				{
					updateStepGSNameFnName.emplace_back(*gsName);
				}
			}
		}
	}
}


namespace re
{
	std::unique_ptr<RenderSystem> RenderSystem::Create(std::string const& name, std::string const& pipelineFileName)
	{
		// Load the render system description:
		std::string const& scriptPath = std::format("{}{}", core::configkeys::k_pipelineDirName, pipelineFileName);

		RenderSystemDescription const& renderSystemDesc = LoadRenderSystemDescription(scriptPath.c_str());

		LOG("Render pipeline description \"%s\" loaded!", pipelineFileName.c_str());

		// Create the render system, and build its various pipeline stages:
		std::unique_ptr<RenderSystem> newRenderSystem = nullptr;

		newRenderSystem.reset(new RenderSystem(name));

		newRenderSystem->BuildPipeline(renderSystemDesc); // Builds initialization/update functions

		return std::move(newRenderSystem);
	}


	RenderSystem::RenderSystem(std::string const& name)
		: NamedObject(name)
		, m_graphicsSystemManager(this)
		, m_renderPipeline(name + " render pipeline")
		, m_initPipeline(nullptr)
	{
		m_graphicsSystemManager.Create();
	}


	void RenderSystem::Destroy()
	{
		m_graphicsSystemManager.Destroy();
		m_renderPipeline.Destroy();
		m_initPipeline = nullptr;
		m_updatePipeline.clear();
	}


	void RenderSystem::BuildPipeline(RenderSystemDescription const& renderSysDesc)
	{
		// Create the GrpahicsSystems:
		for (std::string const& gsName : renderSysDesc.m_graphicsSystemNames)
		{
			m_graphicsSystemManager.CreateAddGraphicsSystemByScriptName(gsName);
		}

		m_initPipeline = [this, renderSysDesc](re::RenderSystem* renderSystem)
		{
			gr::GraphicsSystemManager& gsm = renderSystem->GetGraphicsSystemManager();
			gsm.Create();

			re::RenderPipeline& renderPipeline = renderSystem->GetRenderPipeline();

			// Build up our log message so it's printed in a single block
			std::string initOrderLog = 
				std::format("Render system \"{}\" graphics system initialization order:", GetName());

			for (auto const& currentGSScriptName : renderSysDesc.m_pipelineOrder)
			{
				initOrderLog = std::format("{}\n\t- {}", initOrderLog, currentGSScriptName);

				gr::GraphicsSystem* currentGS = gsm.GetGraphicsSystemByScriptName(currentGSScriptName);

				std::map<std::string, std::shared_ptr<re::Texture>> const& textureInputs = 
					ResolveTextureDependencies(currentGSScriptName, renderSysDesc, gsm);

				for (auto const& initFn : currentGS->GetRuntimeBindings().m_initPipelineFunctions)
				{
					std::string const& initFnName = initFn.first;

					std::string const& stagePipelineName = std::format("{}::{} stages",
						currentGS->GetName(),
						initFnName);

					initFn.second(renderPipeline.AddNewStagePipeline(stagePipelineName), textureInputs);
				}

				// Now the GS is initialized, it can populate its resource dependencies for other GS's
				currentGS->RegisterOutputs();
			}
			LOG(initOrderLog.c_str());

			// Now our GS's exist and their input dependencies are registered, we can compute their execution ordering.
			// Note: The update pipeline caches member function and data pointers; We can only populate it once our GS's
			// are created & initialized
			const bool singleThreadGSExecution = en::Config::Get()->KeyExists(core::configkeys::k_singleThreadGSExecution);

			std::vector<std::vector<std::string>> updateExecutionGroups;
			ComputeExecutionGroups(renderSysDesc, updateExecutionGroups, singleThreadGSExecution);

			std::string updateOrderLog = std::format("Render system \"{}\" {} graphics system update execution grouping:",
				GetName().c_str(),
				singleThreadGSExecution ? "serial" : "threaded");

			for (auto const& executionGrp : updateExecutionGroups)
			{
				std::vector<UpdateStep>& currentStep = m_updatePipeline.emplace_back();

				for (auto const& currentGSName : executionGrp)
				{
					gr::GraphicsSystem* currentGS = gsm.GetGraphicsSystemByScriptName(currentGSName);
					SEAssert(currentGS, "Failed to find GraphicsSystem");

					for (auto const& updateFn : currentGS->GetRuntimeBindings().m_preRenderFunctions)
					{
						currentStep.emplace_back(UpdateStep{
						.m_preRenderFunc = updateFn.second,
						.m_resolvedDependencies = ResolveDataDependencies(currentGSName, renderSysDesc, gsm),
						.m_gs = currentGS,
						.m_scriptFunctionName = updateFn.first });

						updateOrderLog = std::format("{}\n\t- {}::{}", updateOrderLog, currentGSName, updateFn.first);
					}
				}
				
				updateOrderLog = std::format("{}\n\t\t---", updateOrderLog);
			}
			LOG(updateOrderLog.c_str());
		};
	}


	void RenderSystem::ExecuteInitializationPipeline()
	{
		m_initPipeline(this);
	}


	void RenderSystem::ExecuteUpdatePipeline()
	{
		SEBeginCPUEvent(std::format("{} ExecuteUpdatePipeline", GetName()).c_str());

		static const bool s_singleThreadGSExecution = 
			en::Config::Get()->KeyExists(core::configkeys::k_singleThreadGSExecution);


		auto ExecuteUpdateStep = [](UpdateStep const& currentStep)
			{
				try
				{
					currentStep.m_preRenderFunc(currentStep.m_resolvedDependencies);
				}
				catch (std::exception e)
				{
					SEAssertF(std::format(
						"RenderSystem::ExecuteUpdatePipeline exception when executing \"{}::{}\"\n{}",
						currentStep.m_gs->GetName().c_str(),
						currentStep.m_scriptFunctionName.c_str(),
						e.what()).c_str());
				}
			};


		m_graphicsSystemManager.PreRender();

		for (auto& executionGroup : m_updatePipeline)
		{
			std::vector<std::future<void>> updateStepFutures;
			updateStepFutures.reserve(executionGroup.size());

			for (auto const& currentStep : executionGroup)
			{
				if (s_singleThreadGSExecution)
				{
					ExecuteUpdateStep(currentStep);
				}
				else
				{
					updateStepFutures.emplace_back(core::ThreadPool::Get()->EnqueueJob([&]()
						{
							ExecuteUpdateStep(currentStep);
						}));
				}
			}

			// Wait for all tasks within the current execution group to complete
			for (auto const& updateFuture : updateStepFutures)
			{
				updateFuture.wait();
			}
		}

		SEEndCPUEvent();
	}


	void RenderSystem::ShowImGuiWindow()
	{
		if (ImGui::CollapsingHeader(std::format("Graphics System Manager##{}", GetUniqueID()).c_str(),
			ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent();
			m_graphicsSystemManager.ShowImGuiWindow();
			ImGui::Unindent();
		}
	}
}