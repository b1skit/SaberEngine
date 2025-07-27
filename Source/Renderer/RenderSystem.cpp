// © 2023 Adam Badke. All rights reserved.
#include "Context.h"
#include "GraphicsSystem.h"
#include "GraphicsSystemCommon.h"
#include "GraphicsSystemManager.h"
#include "IndexedBuffer.h"
#include "RenderDataManager.h"
#include "RenderSystem.h"
#include "RenderPipelineDesc.h"

#include "Core/Config.h"
#include "Core/Logger.h"
#include "Core/ProfilingMarkers.h"
#include "Core/ThreadPool.h"

#include "Core/Definitions/ConfigKeys.h"


using GSName = gr::RenderPipelineDescription::GSName;
using SrcDstNamePairs = gr::RenderPipelineDescription::SrcDstNamePairs;


namespace
{
	gr::TextureDependencies ResolveTextureDependencies(
		std::string const& dstGSScriptName,
		gr::RenderPipelineDescription const& renderSysDesc,
		gr::GraphicsSystemManager const& gsm)
	{
		gr::TextureDependencies texDependencies;
		
		gr::GraphicsSystem* dstGS = gsm.GetGraphicsSystemByScriptName(dstGSScriptName);

		// Initialize everything with a default in case the input doesn't exist for some reason
		for (auto const& input : dstGS->GetTextureInputs())
		{
			gr::GraphicsSystem::TextureInputDefault inputDefault = dstGS->GetTextureInputDefaultType(input.first);

			switch (inputDefault)
			{
			case gr::GraphicsSystem::TextureInputDefault::OpaqueWhite:
			{
				texDependencies[input.first] =
					&gsm.GetContext()->GetDefaultTexture(en::DefaultResourceNames::k_opaqueWhiteDefaultTexName);
			}
			break;
			case gr::GraphicsSystem::TextureInputDefault::TransparentWhite:
			{
				texDependencies[input.first] =
					&gsm.GetContext()->GetDefaultTexture(en::DefaultResourceNames::k_transparentWhiteDefaultTexName);
			}
			break;
			case gr::GraphicsSystem::TextureInputDefault::OpaqueBlack:
			{
				texDependencies[input.first] =
					&gsm.GetContext()->GetDefaultTexture(en::DefaultResourceNames::k_opaqueBlackDefaultTexName);
			}
			break;
			case gr::GraphicsSystem::TextureInputDefault::TransparentBlack:
			{
				texDependencies[input.first] =
					&gsm.GetContext()->GetDefaultTexture(en::DefaultResourceNames::k_transparentBlackDefaultTexName);
			}
			break;
			case gr::GraphicsSystem::TextureInputDefault::CubeMap_OpaqueWhite:
			{
				texDependencies[input.first] =
					&gsm.GetContext()->GetDefaultTexture(en::DefaultResourceNames::k_cubeMapOpaqueWhiteDefaultTexName);
			}
			break;
			case gr::GraphicsSystem::TextureInputDefault::CubeMap_TransparentWhite:
			{
				texDependencies[input.first] =
					&gsm.GetContext()->GetDefaultTexture(en::DefaultResourceNames::k_cubeMapTransparentWhiteDefaultTexName);
			}
			break;
			case gr::GraphicsSystem::TextureInputDefault::CubeMap_OpaqueBlack:
			{
				texDependencies[input.first] =
					&gsm.GetContext()->GetDefaultTexture(en::DefaultResourceNames::k_cubeMapOpaqueBlackDefaultTexName);
			}
			break;
			case gr::GraphicsSystem::TextureInputDefault::CubeMap_TransparentBlack:
			{
				texDependencies[input.first] =
					&gsm.GetContext()->GetDefaultTexture(en::DefaultResourceNames::k_cubeMapTransparentBlackDefaultTexName);
			}
			break;
			case gr::GraphicsSystem::TextureInputDefault::None:
			{
				texDependencies[input.first] = nullptr;
			}
			break;
			default: SEAssertF("Invalid TextureInputDefault");
			}
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

				if (srcGS)
				{
					for (auto const& dependencySrcDstNameMapping : srcEntry.second)
					{
						std::string const& srcName = dependencySrcDstNameMapping.first;

						util::CHashKey const& dstName = util::CHashKey::Create(dependencySrcDstNameMapping.second);
						SEAssert(dstGS->HasRegisteredTextureInput(dstName),
							"Destination GS hasn't registered this input name");

						texDependencies[dstName] = srcGS->GetTextureOutput(srcName);
					}
				}
			}
		}

		return texDependencies;
	}


	gr::BufferDependencies ResolveBufferDependencies(
		std::string const& dstGSScriptName,
		gr::RenderPipelineDescription const& renderSysDesc,
		gr::GraphicsSystemManager const& gsm)
	{
		gr::BufferDependencies bufferDependencies;

		gr::GraphicsSystem const* dstGS = gsm.GetGraphicsSystemByScriptName(dstGSScriptName);

		// Initialize everything with a nullptr, in case no input is described
		for (auto const& input : dstGS->GetBufferInputs())
		{
			bufferDependencies.emplace(input, nullptr);
		}

		// Process any buffer inputs assigned to the current destination GraphicsSystem:
		if (renderSysDesc.m_bufferInputs.contains(dstGSScriptName))
		{
			std::vector<std::pair<GSName, SrcDstNamePairs>> const& gsDependencies =
				renderSysDesc.m_bufferInputs.at(dstGSScriptName);

			for (auto const& curDependency : gsDependencies)
			{
				std::string const& srcGSName = curDependency.first;

				gr::GraphicsSystem const* srcGS = gsm.GetGraphicsSystemByScriptName(srcGSName);
				SEAssert(srcGS, "Source GraphicsSystem could not be found");

				for (auto const& srcDstNames : curDependency.second)
				{
					util::CHashKey const& dependencyDstName = util::CHashKey::Create(srcDstNames.second);
					SEAssert(dstGS->HasRegisteredBufferInput(dependencyDstName),
						"No Buffer input with the given name has been registered");

					std::string const& dependencySrcName = srcDstNames.first;
					bufferDependencies[dependencyDstName] = srcGS->GetBufferOutput(dependencySrcName);
				}
			}
		}

		return bufferDependencies;
	}


	gr::DataDependencies ResolveDataDependencies(
		std::string const& dstGSScriptName,
		gr::RenderPipelineDescription const& renderSysDesc,
		gr::GraphicsSystemManager const& gsm)
	{
		gr::DataDependencies resolvedDependencies;

		gr::GraphicsSystem const* dstGS = gsm.GetGraphicsSystemByScriptName(dstGSScriptName);

		// Initialize everything with a nullptr, in case no input is described
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
					util::CHashKey const& dependencyDstName = util::CHashKey::Create(srcDstNames.second);
					SEAssert(dstGS->HasRegisteredDataInput(dependencyDstName),
						"No data input with the given name has been registered");

					std::string const& dependencySrcName = srcDstNames.first;
					resolvedDependencies[dependencyDstName] = srcGS->GetDataOutput(dependencySrcName);
				}
			}
		}

		return resolvedDependencies;
	}
	
	
	void ComputeExecutionGroups(
		gr::RenderPipelineDescription const& renderSysDesc,
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
				std::string const* m_gsName = nullptr;
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
				// Consider inputs as a dependency for computing the CPU-side GraphicsSystem update order. Even for
				// resources that are exclusively modified on the GPU, this is necessary in case an owning GS
				// destroys/modifies a resource used by another GS as a dependency
#define CONSIDER_TEX_INPUTS_AS_UPDATE_DEPENDENCIES
#if defined(CONSIDER_TEX_INPUTS_AS_UPDATE_DEPENDENCIES)
				PopulateDependencies(renderSysDesc.m_textureInputs);
#endif
#define CONSIDER_BUFFER_INPUTS_AS_UPDATE_DEPENDENCIES
#if defined(CONSIDER_BUFFER_INPUTS_AS_UPDATE_DEPENDENCIES)
				PopulateDependencies(renderSysDesc.m_bufferInputs);
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


	bool DisableThreadedGraphicsSystemUpdates()
	{
		// Note: Only a single thread can access an OpenGL context, and we don't (currently) support multiple OpenGL
		// contexts. Some GraphicsSystems indirectly make platform-level calls (e.g. for Buffer CPU readbacks), thus
		// we disable threaded GS updates in all cases for this API

		const bool singleThreadGSExecutionCmdReceived = 
			core::Config::KeyExists(core::configkeys::k_singleThreadGSExecution);

		const platform::RenderingAPI api =
			core::Config::GetValue<platform::RenderingAPI>(core::configkeys::k_renderingAPIKey);
		switch (api)
		{
		case platform::RenderingAPI::DX12: return singleThreadGSExecutionCmdReceived;
		case platform::RenderingAPI::OpenGL: return true;
		default: SEAssertF("Invalid rendering API");
		}
		return singleThreadGSExecutionCmdReceived;
	}
}


namespace gr
{
	std::unique_ptr<RenderSystem> RenderSystem::Create(
		std::string const& pipelineFileName, RenderDataManager const* renderData, re::Context* context)
	{
		// Load the render system description:
		std::string const& scriptPath = std::format("{}{}", core::configkeys::k_pipelineDirName, pipelineFileName);

		gr::RenderPipelineDescription const& renderSystemDesc = gr::LoadPipelineDescription(scriptPath.c_str());

		LOG("Render pipeline description \"%s\" loaded!", pipelineFileName.c_str());

		// Create the render system, and build its various pipeline stages:
		std::unique_ptr<RenderSystem> newRenderSystem = nullptr;

		newRenderSystem.reset(new RenderSystem(renderSystemDesc.m_name, context));

		newRenderSystem->BuildPipeline(renderSystemDesc, renderData); // Builds initialization/update functions

		// Initialize the render system (which will in turn initialize each of its graphics systems & stage pipelines)
		newRenderSystem->ExecuteInitializationPipeline();

		return std::move(newRenderSystem);
	}


	RenderSystem::RenderSystem(std::string const& name, re::Context* context)
		: INamedObject(name)
		, m_graphicsSystemManager(context)
		, m_renderPipeline(name)
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


	void RenderSystem::PostUpdatePreRender(IndexedBufferManager& ibm, effect::EffectDB const& effectDB)
	{
		SEBeginCPUEvent(GetName().c_str());
		m_renderPipeline.PostUpdatePreRender(ibm, effectDB);
		SEEndCPUEvent();
	}


	void RenderSystem::EndOfFrame()
	{
		SEBeginCPUEvent(GetName().c_str());
		m_renderPipeline.EndOfFrame();
		m_graphicsSystemManager.EndOfFrame();
		SEEndCPUEvent();
	}


	void RenderSystem::BuildPipeline(
		gr::RenderPipelineDescription const& renderSysDesc, gr::RenderDataManager const* renderData)
	{
		SEBeginCPUEvent(GetName().c_str());
		// Create our GraphicsSystems:
		m_graphicsSystemManager.Create(renderData);

		for (std::string const& gsName : renderSysDesc.m_pipelineOrder)
		{
			auto flagsItr = renderSysDesc.m_flags.find(gsName);

			m_graphicsSystemManager.CreateAddGraphicsSystemByScriptName(
				gsName,
				(flagsItr != renderSysDesc.m_flags.end() ? 
					flagsItr->second : std::vector<std::pair<std::string, std::string>>{}));
		}
		

		m_initPipeline = [this, renderSysDesc](gr::RenderSystem* renderSystem)
		{
			gr::GraphicsSystemManager& gsm = renderSystem->GetGraphicsSystemManager();

			gr::RenderPipeline& renderPipeline = renderSystem->GetRenderPipeline();

			// Build up our log message so it's printed in a single block
			std::string initOrderLog = 
				std::format("Render system \"{}\" graphics system initialization order:", GetName());

			for (auto const& currentGSScriptName : renderSysDesc.m_pipelineOrder)
			{
				initOrderLog = std::format("{}\n\t- {}", initOrderLog, currentGSScriptName);

				gr::GraphicsSystem* currentGS = gsm.GetGraphicsSystemByScriptName(currentGSScriptName);			

				gr::TextureDependencies const& textureInputs =
					ResolveTextureDependencies(currentGSScriptName, renderSysDesc, gsm);

				gr::BufferDependencies const& bufferInputs = 
					ResolveBufferDependencies(currentGSScriptName, renderSysDesc, gsm);

				gr::DataDependencies const& dataInputs =
					ResolveDataDependencies(currentGSScriptName, renderSysDesc, gsm);

				for (auto const& initFn : currentGS->GetRuntimeBindings().m_initPipelineFunctions)
				{
					std::string const& initFnName = initFn.first;

					std::string const& stagePipelineName = std::format("{} stages", currentGS->GetName());

					initFn.second(
						renderPipeline.AddNewStagePipeline(stagePipelineName), textureInputs, bufferInputs, dataInputs);
				}

				// Now the GS is initialized, it can populate its resource dependencies for other GS's
				currentGS->RegisterOutputs();
			}
			LOG(initOrderLog.c_str());

			// Now our GS's exist and their input dependencies are registered, we can compute their execution ordering.
			// Note: The update pipeline caches member function and data pointers; We can only populate it once our GS's
			// are created & initialized
			const bool singleThreadGSExecution = DisableThreadedGraphicsSystemUpdates();

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
							.m_gs = currentGS,
							.m_scriptFunctionName = updateFn.first });

						updateOrderLog = std::format("{}\n\t- {}::{}", updateOrderLog, currentGSName, updateFn.first);
					}
				}
				
				updateOrderLog = std::format("{}\n\t\t---", updateOrderLog);
			}
			LOG(updateOrderLog.c_str());
		};
		SEEndCPUEvent();
	}


	void RenderSystem::ExecuteInitializationPipeline()
	{
		SEBeginCPUEvent(GetName().c_str());
		m_initPipeline(this);
		SEEndCPUEvent();
	}


	void RenderSystem::ExecuteUpdatePipeline(uint64_t currentFrameNum)
	{
		SEBeginCPUEvent(std::format("RenderSystem::ExecuteUpdatePipeline: {}", GetName()).c_str());

		static const bool s_singleThreadGSExecution = DisableThreadedGraphicsSystemUpdates();


		auto ExecuteUpdateStep = [this](UpdateStep const& currentStep)
			{
				SEBeginCPUEvent(std::format("Update GS: ", currentStep.m_gs->GetName()).c_str());
				try
				{
					currentStep.m_preRenderFunc();
				}
				catch (std::exception e)
				{
					SEAssertF(std::format(
						"RenderSystem::ExecuteUpdatePipeline exception when executing \"{}::{}\"\n{}",
						currentStep.m_gs->GetName().c_str(),
						currentStep.m_scriptFunctionName.c_str(),
						e.what()).c_str());
				}
				SEEndCPUEvent();
			};


		m_graphicsSystemManager.PreRender(currentFrameNum);

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
					updateStepFutures.emplace_back(core::ThreadPool::EnqueueJob([&]()
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


	// ---


	void CreateAddRenderSystem::Execute(void* cmdData)
	{
		CreateAddRenderSystem* cmdPtr = reinterpret_cast<CreateAddRenderSystem*>(cmdData);

		cmdPtr->GetRenderSystemsForModification().emplace_back(gr::RenderSystem::Create(
			cmdPtr->m_pipelineFileName,
			&cmdPtr->GetRenderData(),
			cmdPtr->GetContextForModification()));
	}
}