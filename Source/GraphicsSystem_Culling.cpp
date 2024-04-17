// © 2023 Adam Badke. All rights reserved.
#include "BoundsRenderData.h"
#include "CameraRenderData.h"
#include "CoreEngine.h"
#include "GraphicsSystem_Culling.h"
#include "GraphicsSystemManager.h"
#include "LightRenderData.h"
#include "RenderDataManager.h"
#include "ThreadSafeVector.h"


namespace
{
	// Returns true if a bounds is visible, or false otherwise
	bool TestBoundsVisibility(
		gr::Bounds::RenderData const& bounds,
		gr::Transform::RenderData const& transform,
		gr::Camera::Frustum const& frustum,
		float* camToMeshBoundsDistOut = nullptr) // Optional, will be populated if not null
	{
		// Transform our Bounds into world space:
		constexpr uint8_t k_numBoundsPoints = 8;
		const std::array<glm::vec3, k_numBoundsPoints> boundsPoints = {
			transform.g_model * glm::vec4(bounds.m_minXYZ.x, bounds.m_maxXYZ.y, bounds.m_maxXYZ.z, 1.f), // farTL
			transform.g_model * glm::vec4(bounds.m_minXYZ.x, bounds.m_minXYZ.y, bounds.m_maxXYZ.z, 1.f), // farBL
			transform.g_model * glm::vec4(bounds.m_maxXYZ.x, bounds.m_maxXYZ.y, bounds.m_maxXYZ.z, 1.f), // farTR
			transform.g_model * glm::vec4(bounds.m_maxXYZ.x, bounds.m_minXYZ.y, bounds.m_maxXYZ.z, 1.f), // farBR
			transform.g_model * glm::vec4(bounds.m_minXYZ.x, bounds.m_maxXYZ.y, bounds.m_minXYZ.z, 1.f), // nearTL
			transform.g_model * glm::vec4(bounds.m_minXYZ.x, bounds.m_minXYZ.y, bounds.m_minXYZ.z, 1.f), // nearBL
			transform.g_model * glm::vec4(bounds.m_maxXYZ.x, bounds.m_maxXYZ.y, bounds.m_minXYZ.z, 1.f), // nearTR
			transform.g_model * glm::vec4(bounds.m_maxXYZ.x, bounds.m_minXYZ.y, bounds.m_minXYZ.z, 1.f), // nearBR
		};

		// We sort our results based on the distance to the bounds center:
		const glm::vec3 boundsCenter = (bounds.m_minXYZ + bounds.m_maxXYZ) * 0.5f;

		// Note: Frustum normals point outward

		// Detect Bounds completely outside of any plane:
		for (gr::Camera::FrustumPlane const& plane : frustum.m_planes)
		{
			bool isCompletelyOutsideOfPlane = true;
			for (uint8_t pointIdx = 0; pointIdx < k_numBoundsPoints; pointIdx++)
			{
				glm::vec3 const& planeToBoundsDir = boundsPoints[pointIdx] - plane.m_point;

				const bool isOutsideOfPlane = glm::dot(planeToBoundsDir, plane.m_normal) > 0.f;
				if (!isOutsideOfPlane)
				{
					isCompletelyOutsideOfPlane = false;
					break;
				}
			}
			if (isCompletelyOutsideOfPlane)
			{
				return false; // Any Bounds totally outside of any plane is not visible
			}
		}

		// If we've made it this far, the object is visible
		if (camToMeshBoundsDistOut)
		{
			*camToMeshBoundsDistOut = glm::length(frustum.m_camWorldPos - boundsCenter);
		}
		return true;
	}


	void CullLights(
		gr::RenderDataManager const& renderData, 
		gr::Camera::Frustum const& frustum,
		std::vector<gr::RenderDataID>& pointLightIDsOut,
		std::vector<gr::RenderDataID>& spotLightIDsOut,
		bool cullingEnabled)
	{
		pointLightIDsOut.clear();
		spotLightIDsOut.clear();

		pointLightIDsOut.reserve(renderData.GetNumElementsOfType<gr::Light::RenderDataPoint>());
		spotLightIDsOut.reserve(renderData.GetNumElementsOfType<gr::Light::RenderDataSpot>());

		auto DoCulling = [&renderData, &frustum, &cullingEnabled]<typename T>(
			gr::RenderDataManager::LinearIterator<T> lightItr,
			gr::RenderDataManager::LinearIterator<T> const& lightItrEnd,
			std::vector<gr::RenderDataID>& lightIDs)
		{
			while (lightItr != lightItrEnd)
			{
				gr::Bounds::RenderData const& lightBounds =
					renderData.GetObjectData<gr::Bounds::RenderData>(lightItr->m_renderDataID);
				gr::Transform::RenderData const& lightTransform =
					renderData.GetTransformDataFromTransformID(lightItr->m_transformID);

				const bool lightIsVisible = TestBoundsVisibility(lightBounds, lightTransform, frustum);
				if (lightIsVisible || !cullingEnabled)
				{
					lightIDs.emplace_back(lightItr->m_renderDataID);
				}

				++lightItr;
			}
		};
		DoCulling(
			renderData.Begin<gr::Light::RenderDataPoint>(), renderData.End<gr::Light::RenderDataPoint>(), pointLightIDsOut);
		DoCulling(
			renderData.Begin<gr::Light::RenderDataSpot>(), renderData.End<gr::Light::RenderDataSpot>(), spotLightIDsOut);
	}


	void CullGeometry(
		gr::RenderDataManager const& renderData, 
		std::unordered_map<gr::RenderDataID, std::vector<gr::RenderDataID>> const& meshesToMeshPrimitiveBounds,
		gr::Camera::Frustum const& frustum,
		std::vector<gr::RenderDataID>& visibleIDsOut,
		bool cullingEnabled)
	{
		struct IDAndDistance
		{
			gr::RenderDataID m_visibleID;
			float m_distance;
		};
		std::vector<IDAndDistance> idsAndDistances;
		idsAndDistances.reserve(visibleIDsOut.capacity());

		for (auto const& encapsulatingBounds : meshesToMeshPrimitiveBounds)
		{
			const gr::RenderDataID meshID = encapsulatingBounds.first;

			// Hierarchical culling: Only test the MeshPrimitive Bounds if the Mesh Bounds is visible
			gr::Bounds::RenderData const& meshBounds = renderData.GetObjectData<gr::Bounds::RenderData>(meshID);
			gr::Transform::RenderData const& meshTransform = renderData.GetTransformDataFromRenderDataID(meshID);

			float camToMeshBoundsDist = 0.f;
			const bool meshIsVisible = TestBoundsVisibility(meshBounds, meshTransform, frustum, &camToMeshBoundsDist);

			if (meshIsVisible || !cullingEnabled)
			{
				std::vector<gr::RenderDataID> const& meshPrimitiveIDs = encapsulatingBounds.second;
				for (auto const& meshPrimID : meshPrimitiveIDs)
				{
					gr::Bounds::RenderData const& primBounds = 
						renderData.GetObjectData<gr::Bounds::RenderData>(meshPrimID);
					gr::Transform::RenderData const& primTransform = 
						renderData.GetTransformDataFromRenderDataID(meshPrimID);

					float camToMeshPrimBoundsDist = 0.f;
					const bool meshPrimIsVisible = 
						TestBoundsVisibility(primBounds, primTransform, frustum, &camToMeshPrimBoundsDist);

					if (meshPrimIsVisible || !cullingEnabled)
					{
						idsAndDistances.emplace_back(IDAndDistance{
							.m_visibleID = meshPrimID,
							.m_distance = camToMeshPrimBoundsDist
							});
					}
				}
			}
		}

		// Sort our IDs so they're ordered closest to the camera, to furthest away
		std::sort(idsAndDistances.begin(), idsAndDistances.end(), 
			[](IDAndDistance const& a, IDAndDistance const& b)
			{
				return a.m_distance < b.m_distance;
			});

		// Finally, copy our sorted results into the outgoing vector:
		for (IDAndDistance const& idAndDist : idsAndDistances)
		{
			visibleIDsOut.emplace_back(idAndDist.m_visibleID);			
		}
	}
}

namespace gr
{
	constexpr char const* k_gsName = "Culling Graphics System";


	CullingGraphicsSystem::CullingGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(k_gsName, owningGSM)
		, NamedObject(k_gsName)
		, m_cullingEnabled(true)
	{
	}


	void CullingGraphicsSystem::InitPipeline()
	{
		//
	}


	void CullingGraphicsSystem::PreRender()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		// Add any new bounds to our tracking tables:
		if (renderData.HasIDsWithNewData<gr::Bounds::RenderData>())
		{
			std::vector<gr::RenderDataID> newBoundsIDS = renderData.GetIDsWithNewData<gr::Bounds::RenderData>();

			auto newBoundsItr = renderData.IDBegin(newBoundsIDS);
			auto const& newBoundsItrEnd = renderData.IDEnd(newBoundsIDS);
			while (newBoundsItr != newBoundsItrEnd)
			{
				gr::Bounds::RenderData const& boundsData = newBoundsItr.Get<gr::Bounds::RenderData>();

				const gr::RenderDataID newBoundsID = newBoundsItr.GetRenderDataID();

				const gr::RenderDataID encapsulatingBounds = boundsData.m_encapsulatingBounds;

				// If we've never seen the encapsulating bounds before, add a new (empty) vector of IDs
				if (encapsulatingBounds != k_invalidRenderDataID &&
					!m_meshesToMeshPrimitiveBounds.contains(encapsulatingBounds))
				{
					m_meshesToMeshPrimitiveBounds.emplace(encapsulatingBounds, std::vector<gr::RenderDataID>());
				}
				else if (gr::HasFeature(gr::RenderObjectFeature::IsMeshBounds, renderData.GetFeatureBits(newBoundsID)) &&
					!m_meshesToMeshPrimitiveBounds.contains(newBoundsID))
				{
					SEAssert(encapsulatingBounds == gr::k_invalidRenderDataID,
						"Mesh Bounds should not have an encapsulating bounds");

					m_meshesToMeshPrimitiveBounds.emplace(newBoundsID, std::vector<gr::RenderDataID>());
				}
				
				if (gr::HasFeature(
					gr::RenderObjectFeature::IsMeshPrimitiveBounds, renderData.GetFeatureBits(newBoundsID)))
				{
					SEAssert(encapsulatingBounds != gr::k_invalidRenderDataID,
						"MeshPrimitive Bounds must have an encapsulating bounds");
					SEAssert(m_meshesToMeshPrimitiveBounds.contains(encapsulatingBounds),
						"Encapsulating bounds should have already been recorded");

					// Store the MeshPrimitive's ID under its encapsulating Mesh:
					m_meshesToMeshPrimitiveBounds.at(encapsulatingBounds).emplace_back(newBoundsID);
					
					// Map the MeshPrimitive back to its encapsulating Mesh:
					m_meshPrimitivesToEncapsulatingMesh.emplace(newBoundsID, boundsData.m_encapsulatingBounds);
				}

				++newBoundsItr;
			}
		}
		
		// Remove any deleted bounds from our tracking tables:
		if (renderData.HasIDsWithDeletedData<gr::Bounds::RenderData>())
		{
			std::vector<gr::RenderDataID> deletedBoundsIDS = renderData.GetIDsWithDeletedData<gr::Bounds::RenderData>();

			auto deletedBoundsItr = renderData.IDBegin(deletedBoundsIDS);
			auto const& deletedBoundsItrEnd = renderData.IDEnd(deletedBoundsIDS);
			while (deletedBoundsItr != deletedBoundsItrEnd)
			{
				// Note: We don't have access to the filterbits of the deleted IDs anymore; It's possible the bounds
				// were not associated with a Mesh/MeshPrimitive (e.g. scene bounds, light mesh bounds)
				gr::RenderDataID deletedBoundsID = deletedBoundsItr.GetRenderDataID();

				// Handle deleted Mesh bounds:
				if (m_meshesToMeshPrimitiveBounds.contains(deletedBoundsID))
				{
					SEAssert(m_meshesToMeshPrimitiveBounds.at(deletedBoundsID).empty(),
						"There are still bounds registered under the current Mesh. This suggests an ordering issue"
						" with delete commands");

					m_meshesToMeshPrimitiveBounds.erase(deletedBoundsID);
				}
				else if (m_meshPrimitivesToEncapsulatingMesh.contains(deletedBoundsID)) // Deleted MeshPrimitive bounds
				{
					gr::RenderDataID encapsulatingBoundsID = m_meshPrimitivesToEncapsulatingMesh.at(deletedBoundsID);

					auto primitiveIDItr = std::find(m_meshesToMeshPrimitiveBounds[encapsulatingBoundsID].begin(),
						m_meshesToMeshPrimitiveBounds[encapsulatingBoundsID].end(), 
						deletedBoundsID);

					m_meshesToMeshPrimitiveBounds[encapsulatingBoundsID].erase(primitiveIDItr);
				}

				++deletedBoundsItr;
			}
		}

		// Erase any cached frustums for deleted cameras:
		std::vector<gr::RenderDataID> const& deletedCamIDs = renderData.GetIDsWithDeletedData<gr::Camera::RenderData>();
		for (gr::RenderDataID camID : deletedCamIDs)
		{
			for (uint8_t faceIdx = 0; faceIdx < 6; faceIdx++)
			{
				gr::Camera::View const& deletedView = gr::Camera::View(camID, faceIdx);
				if (!m_cachedFrustums.contains(deletedView))
				{
					SEAssert(faceIdx > 0, "Failed to find face 0. All cameras should have face 0");
					break;
				}
				m_cachedFrustums.erase(deletedView);
			}
		}

		// CPU-side frustum culling:
		// -------------------------

		const size_t numMeshPrimitives = m_meshPrimitivesToEncapsulatingMesh.size();

		util::ThreadSafeVector<std::future<void>> cullingFutures;
		cullingFutures.reserve(renderData.GetNumElementsOfType<gr::Camera::RenderData>());

		// We'll also cull lights against the currently active camera
		const gr::RenderDataID activeCamRenderDataID = m_graphicsSystemManager->GetActiveCameraRenderDataID();

		// Cull every registered camera:
		std::vector<gr::RenderDataID> const& cameraIDs = renderData.GetRegisteredRenderDataIDs<gr::Camera::RenderData>();
		auto cameraItr = renderData.IDBegin(cameraIDs);
		auto const& cameraItrEnd = renderData.IDEnd(cameraIDs);
		while (cameraItr != cameraItrEnd)
		{
			// Gather the data we'll pass by value:
			const gr::RenderDataID cameraID = cameraItr.GetRenderDataID();

			gr::Camera::RenderData const* camData = &cameraItr.Get<gr::Camera::RenderData>();
			gr::Transform::RenderData const* camTransformData = &cameraItr.GetTransformData();

			const bool cameraIsDirty = cameraItr.IsDirty<gr::Camera::RenderData>();

			// Enqueue the culling job:
			cullingFutures.emplace_back(en::CoreEngine::GetThreadPool()->EnqueueJob(
				[cameraID, camData, cameraIsDirty, camTransformData, numMeshPrimitives, activeCamRenderDataID,
				this, &renderData]()
			{
				// Create/update frustum planes for dirty cameras:
				// A Camera will be dirty if it has just been created, or if it has just been modified
				const uint8_t numViews = gr::Camera::NumViews(*camData);
				if (cameraIsDirty)
				{
					// Clear any existing FrustumPlanes:
					for (uint8_t faceIdx = 0; faceIdx < numViews; faceIdx++)
					{
						gr::Camera::View const& dirtyView = gr::Camera::View(cameraID, faceIdx);

						{
							std::lock_guard<std::mutex> lock(m_cachedFrustumsMutex);

							const bool hasCachedFrustumPlanes = m_cachedFrustums.contains(dirtyView);
							if (hasCachedFrustumPlanes)
							{
								m_cachedFrustums.erase(dirtyView);
							}
						}
					}

					// Build a new set of FrustumPlanes:
					switch (numViews)
					{
					case 1:
					{
						{
							std::lock_guard<std::mutex> lock(m_cachedFrustumsMutex);

							m_cachedFrustums.emplace(
								gr::Camera::View(cameraID, gr::Camera::View::Face::Default),
								gr::Camera::BuildWorldSpaceFrustumData(
									camTransformData->m_globalPosition, camData->m_cameraParams.g_invViewProjection));
						}
					}
					break;
					case 6:
					{
						std::vector<glm::mat4> invViewProjMats;
						invViewProjMats.reserve(6);

						std::vector<glm::mat4> const& viewMats = gr::Camera::BuildCubeViewMatrices(
							camTransformData->m_globalPosition,
							camTransformData->m_globalRight,
							camTransformData->m_globalUp,
							camTransformData->m_globalForward);

						std::vector<glm::mat4> const& viewProjMats =
							gr::Camera::BuildCubeViewProjectionMatrices(viewMats, camData->m_cameraParams.g_projection);

						invViewProjMats = gr::Camera::BuildCubeInvViewProjectionMatrices(viewProjMats);

						for (uint8_t faceIdx = 0; faceIdx < numViews; faceIdx++)
						{
							{
								std::lock_guard<std::mutex> lock(m_cachedFrustumsMutex);

								m_cachedFrustums.emplace(
									gr::Camera::View(cameraID, faceIdx),
									gr::Camera::BuildWorldSpaceFrustumData(
										camTransformData->m_globalPosition, invViewProjMats[faceIdx]));
							}
						}
					}
					break;
					default: SEAssertF("Invalid number of views");
					}
				} //cameraIsDirty

				// Clear any previous visibility results (Objects may have moved, we need to cull everything each frame)
				for (uint8_t faceIdx = 0; faceIdx < numViews; faceIdx++)
				{
					gr::Camera::View const& currentView = gr::Camera::View(cameraID, faceIdx);

					std::vector<gr::RenderDataID> renderIDsOut;
					renderIDsOut.reserve(numMeshPrimitives);

					// Cull our views and populate the set of visible IDs:
					CullGeometry(
						renderData,
						m_meshesToMeshPrimitiveBounds,
						m_cachedFrustums.at(currentView),
						renderIDsOut,
						m_cullingEnabled);

					// Finally, pass the results to the BatchManager:
					m_graphicsSystemManager->GetBatchManagerForModification().SetCullingResults(
						currentView,
						std::move(renderIDsOut));
				}

				// If we're the active camera, also cull the lights:
				if (cameraID == activeCamRenderDataID)
				{
					SEAssert(numViews == 1, "We're only expecting a single view for the main camera");
					{
						std::vector<gr::RenderDataID> visiblePointLightIDs;
						std::vector<gr::RenderDataID> visibleSpotLightIDs;

						CullLights(
							renderData, 
							m_cachedFrustums.at(gr::Camera::View(cameraID)),
							visiblePointLightIDs,
							visibleSpotLightIDs,
							m_cullingEnabled);

						m_graphicsSystemManager->GetBatchManagerForModification().SetPointLightCullingResults(
							std::move(visiblePointLightIDs));
						m_graphicsSystemManager->GetBatchManagerForModification().SetSpotLightCullingResults(
							std::move(visibleSpotLightIDs));
					}
				}
			}));
						
			++cameraItr;
		}

		// Wait for our jobs to complete
		for (size_t cullingFutureIdx = 0; cullingFutureIdx < cullingFutures.size(); cullingFutureIdx++)
		{
			cullingFutures[cullingFutureIdx].wait();
		}
	}


	void CullingGraphicsSystem::ShowImGuiWindow()
	{
		auto FormatIDString = [](std::vector<gr::RenderDataID> const& renderDataIDs) -> std::string
			{
				std::string result;
				
				constexpr size_t k_idsPerLine = 16;
				size_t currentIDIds = 0;
				for (gr::RenderDataID renderDataID : renderDataIDs)
				{
					currentIDIds++;

					result += std::format("{}{}",
						static_cast<uint32_t>(renderDataID),
						(currentIDIds > 0 && (currentIDIds % k_idsPerLine == 0)) ? "\n" : ", ");
				}

				return result;
			};

		if (ImGui::Button(std::format("{} culling", m_cullingEnabled ? "Disable" : "Enable").c_str()))
		{
			m_cullingEnabled = !m_cullingEnabled;
		}

		if (ImGui::CollapsingHeader("Visible Light IDs"))
		{
			ImGui::Text(std::format("Active camera RenderDataID: {}",
				m_graphicsSystemManager->GetActiveCameraRenderDataID()).c_str());

			ImGui::Text("Point lights:");
			ImGui::Text(FormatIDString(m_graphicsSystemManager->GetBatchManager().GetPointLightCullingResults()).c_str());
			
			ImGui::Separator();

			ImGui::Text("Spot lights:");
			ImGui::Text(FormatIDString(m_graphicsSystemManager->GetBatchManager().GetSpotLightCullingResults()).c_str());
		}

		// Get the visible IDs we sent to the BatchManager:
		if (ImGui::CollapsingHeader("Visible IDs"))
		{
			for (auto const& frustum : m_cachedFrustums)
			{
				ImGui::Text(std::format("Camera RenderDataID: {}, Face: {}",
					frustum.first.m_cameraRenderDataID,
					gr::Camera::View::k_faceNames[frustum.first.m_face]).c_str());
				
				ImGui::Text(FormatIDString(
					m_graphicsSystemManager->GetBatchManager().GetCullingResults(frustum.first)).c_str());

				ImGui::Separator();
			}
		}


		if (ImGui::CollapsingHeader("Bounds RenderDataID tracking"))
		{
			constexpr ImGuiTableFlags flags = 
				ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;
			constexpr int numCols = 2;
			if (ImGui::BeginTable("m_IDToRenderObjectMetadata", numCols, flags))
			{
				ImGui::TableSetupColumn("Mesh RenderObjectID");
				ImGui::TableSetupColumn("MeshPrimitive RenderObjectIDs");
				ImGui::TableHeadersRow();

				for (auto const& meshToMeshPrimitiveIDs : m_meshesToMeshPrimitiveBounds)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();

					const gr::RenderDataID meshID = meshToMeshPrimitiveIDs.first;
					ImGui::Text(std::format("{}", static_cast<uint32_t>(meshID)).c_str());

					ImGui::TableNextColumn();

					std::string columnContents = FormatIDString(meshToMeshPrimitiveIDs.second);

					ImGui::Text(columnContents.c_str());
				}

				ImGui::EndTable();
			}
		}
		
		if (ImGui::CollapsingHeader("Camera culling frustums"))
		{
			for (auto const& viewFrustum : m_cachedFrustums)
			{
				ImGui::Text(std::format("Camera RenderObjectID {}", 
					static_cast<uint32_t>(viewFrustum.first.m_cameraRenderDataID)).c_str());

				ImGui::Text("Near:");
				ImGui::Text(std::format("Point: {}", 
					glm::to_string(viewFrustum.second.m_planes[0].m_point).c_str()).c_str());
				ImGui::Text(std::format("Normal: {}", 
					glm::to_string(viewFrustum.second.m_planes[0].m_normal).c_str()).c_str());

				ImGui::Text("Far:");
				ImGui::Text(std::format("Point: {}", 
					glm::to_string(viewFrustum.second.m_planes[1].m_point).c_str()).c_str());
				ImGui::Text(std::format("Normal: {}", 
					glm::to_string(viewFrustum.second.m_planes[1].m_normal).c_str()).c_str());

				ImGui::Text("Left:");
				ImGui::Text(std::format("Point: {}", 
					glm::to_string(viewFrustum.second.m_planes[2].m_point).c_str()).c_str());
				ImGui::Text(std::format("Normal: {}", 
					glm::to_string(viewFrustum.second.m_planes[2].m_normal).c_str()).c_str());

				ImGui::Text("Right:");
				ImGui::Text(std::format("Point: {}", 
					glm::to_string(viewFrustum.second.m_planes[3].m_point).c_str()).c_str());
				ImGui::Text(std::format("Normal: {}", 
					glm::to_string(viewFrustum.second.m_planes[3].m_normal).c_str()).c_str());

				ImGui::Text("Top:");
				ImGui::Text(std::format("Point: {}", 
					glm::to_string(viewFrustum.second.m_planes[4].m_point).c_str()).c_str());
				ImGui::Text(std::format("Normal: {}", 
					glm::to_string(viewFrustum.second.m_planes[4].m_normal).c_str()).c_str());

				ImGui::Text("Bottom:");
				ImGui::Text(std::format("Point: {}", 
					glm::to_string(viewFrustum.second.m_planes[5].m_point).c_str()).c_str());
				ImGui::Text(std::format("Normal: {}", 
					glm::to_string(viewFrustum.second.m_planes[5].m_normal).c_str()).c_str());

				ImGui::Separator();
			}
		}
	}
}