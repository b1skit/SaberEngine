// © 2025 Adam Badke. All rights reserved.
#pragma once

#include "HeapManager_DX12.h"
#include "Core/Assert.h"
#include <atomic>
#include <chrono>
#include <vector>
#include <memory>
#include <string>
#include <random>
#include <algorithm>

#if defined(_DEBUG)

namespace dx12
{
	/// <summary>
	/// Utility class for validating HeapManager correctness during development.
	/// Only enabled in debug builds to avoid performance impact in release builds.
	/// </summary>
	class HeapManagerValidator final
	{
	public:
		/// <summary>
		/// Validates that all resources created by the heap manager are properly cleaned up
		/// </summary>
		static void ValidateNoLeaks(HeapManager const& heapManager)
		{
			// These validations rely on the destructor assertions
			// The HeapManager destructor will assert if there are remaining resources
		}

		/// <summary>
		/// Stress test for creating and destroying many resources rapidly
		/// </summary>
		static void StressTestResourceLifetime(HeapManager& heapManager, uint32_t numIterations = 1000)
		{
			std::vector<std::unique_ptr<GPUResource>> resources;
			resources.reserve(numIterations);

			// Create a basic buffer resource descriptor for testing
			ResourceDesc testBufferDesc{};
			testBufferDesc.m_resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			testBufferDesc.m_resourceDesc.Alignment = 0;
			testBufferDesc.m_resourceDesc.Width = 1024; // 1KB buffer
			testBufferDesc.m_resourceDesc.Height = 1;
			testBufferDesc.m_resourceDesc.DepthOrArraySize = 1;
			testBufferDesc.m_resourceDesc.MipLevels = 1;
			testBufferDesc.m_resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			testBufferDesc.m_resourceDesc.SampleDesc.Count = 1;
			testBufferDesc.m_resourceDesc.SampleDesc.Quality = 0;
			testBufferDesc.m_resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			testBufferDesc.m_resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			testBufferDesc.m_heapType = D3D12_HEAP_TYPE_DEFAULT;
			testBufferDesc.m_initialState = D3D12_RESOURCE_STATE_COMMON;
			testBufferDesc.m_isMSAATexture = false;
			testBufferDesc.m_createAsComitted = false;

			// Create resources
			for (uint32_t i = 0; i < numIterations; ++i)
			{
				std::wstring resourceName = L"StressTest_Resource_" + std::to_wstring(i);
				auto resource = heapManager.CreateResource(testBufferDesc, resourceName.c_str());
				
				SEAssert(resource && resource->IsValid(), "Failed to create resource during stress test");
				resources.push_back(std::move(resource));
			}

			// Validate all resources are valid
			for (auto const& resource : resources)
			{
				SEAssert(resource && resource->IsValid(), "Resource became invalid unexpectedly");
			}

			// Free resources in random order to test various deallocation patterns
			std::random_device rd;
			std::mt19937 rng(rd());
			std::shuffle(resources.begin(), resources.end(), rng);
			
			// Free half the resources
			size_t halfPoint = resources.size() / 2;
			for (size_t i = 0; i < halfPoint; ++i)
			{
				resources[i].reset();
			}

			// Simulate frame progression to allow deferred deletions
			static uint64_t frameCounter = 0;
			heapManager.BeginFrame(++frameCounter);
			heapManager.EndFrame();

			// Free remaining resources
			resources.clear();

			// Final frame to clean up remaining resources
			heapManager.BeginFrame(++frameCounter);
			heapManager.EndFrame();
		}

		/// <summary>
		/// Test move semantics to ensure no double-free or leaks occur
		/// </summary>
		static void TestMoveSemantics(HeapManager& heapManager)
		{
			ResourceDesc testBufferDesc{};
			testBufferDesc.m_resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			testBufferDesc.m_resourceDesc.Alignment = 0;
			testBufferDesc.m_resourceDesc.Width = 2048;
			testBufferDesc.m_resourceDesc.Height = 1;
			testBufferDesc.m_resourceDesc.DepthOrArraySize = 1;
			testBufferDesc.m_resourceDesc.MipLevels = 1;
			testBufferDesc.m_resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			testBufferDesc.m_resourceDesc.SampleDesc.Count = 1;
			testBufferDesc.m_resourceDesc.SampleDesc.Quality = 0;
			testBufferDesc.m_resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			testBufferDesc.m_resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			testBufferDesc.m_heapType = D3D12_HEAP_TYPE_DEFAULT;
			testBufferDesc.m_initialState = D3D12_RESOURCE_STATE_COMMON;
			testBufferDesc.m_isMSAATexture = false;
			testBufferDesc.m_createAsComitted = false;

			// Test GPUResource move construction
			{
				auto resource1 = heapManager.CreateResource(testBufferDesc, L"MoveTest1");
				SEAssert(resource1 && resource1->IsValid(), "Failed to create resource");

				auto resource2 = std::move(resource1);
				SEAssert(resource2 && resource2->IsValid(), "Move construction failed");
				SEAssert(!resource1 || !resource1->IsValid(), "Source resource should be invalid after move");
			}

			// Test GPUResource move assignment
			{
				auto resource1 = heapManager.CreateResource(testBufferDesc, L"MoveTest2");
				auto resource2 = heapManager.CreateResource(testBufferDesc, L"MoveTest3");
				
				SEAssert(resource1 && resource1->IsValid(), "Failed to create resource1");
				SEAssert(resource2 && resource2->IsValid(), "Failed to create resource2");

				resource2 = std::move(resource1);
				SEAssert(resource2 && resource2->IsValid(), "Move assignment failed");
				SEAssert(!resource1 || !resource1->IsValid(), "Source resource should be invalid after move");
			}
		}

		/// <summary>
		/// Validates heap page allocation and deallocation patterns
		/// </summary>
		static void ValidateHeapPageBehavior(HeapManager& heapManager)
		{
			std::vector<std::unique_ptr<GPUResource>> smallResources;
			std::vector<std::unique_ptr<GPUResource>> largeResources;

			// Create many small resources to fill pages
			ResourceDesc smallBufferDesc{};
			smallBufferDesc.m_resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			smallBufferDesc.m_resourceDesc.Alignment = 0;
			smallBufferDesc.m_resourceDesc.Width = 64; // 64 bytes
			smallBufferDesc.m_resourceDesc.Height = 1;
			smallBufferDesc.m_resourceDesc.DepthOrArraySize = 1;
			smallBufferDesc.m_resourceDesc.MipLevels = 1;
			smallBufferDesc.m_resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			smallBufferDesc.m_resourceDesc.SampleDesc.Count = 1;
			smallBufferDesc.m_resourceDesc.SampleDesc.Quality = 0;
			smallBufferDesc.m_resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			smallBufferDesc.m_resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			smallBufferDesc.m_heapType = D3D12_HEAP_TYPE_DEFAULT;
			smallBufferDesc.m_initialState = D3D12_RESOURCE_STATE_COMMON;
			smallBufferDesc.m_isMSAATexture = false;
			smallBufferDesc.m_createAsComitted = false;

			// Create large resource that should trigger new page allocation
			ResourceDesc largeBufferDesc{};
			largeBufferDesc.m_resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			largeBufferDesc.m_resourceDesc.Alignment = 0;
			largeBufferDesc.m_resourceDesc.Width = 32 * 1024 * 1024; // 32MB
			largeBufferDesc.m_resourceDesc.Height = 1;
			largeBufferDesc.m_resourceDesc.DepthOrArraySize = 1;
			largeBufferDesc.m_resourceDesc.MipLevels = 1;
			largeBufferDesc.m_resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			largeBufferDesc.m_resourceDesc.SampleDesc.Count = 1;
			largeBufferDesc.m_resourceDesc.SampleDesc.Quality = 0;
			largeBufferDesc.m_resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			largeBufferDesc.m_resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			largeBufferDesc.m_heapType = D3D12_HEAP_TYPE_DEFAULT;
			largeBufferDesc.m_initialState = D3D12_RESOURCE_STATE_COMMON;
			largeBufferDesc.m_isMSAATexture = false;
			largeBufferDesc.m_createAsComitted = false;

			// Allocate many small resources
			for (int i = 0; i < 100; ++i)
			{
				std::wstring resourceName = L"SmallResource_" + std::to_wstring(i);
				auto resource = heapManager.CreateResource(smallBufferDesc, resourceName.c_str());
				SEAssert(resource && resource->IsValid(), "Failed to create small resource");
				smallResources.push_back(std::move(resource));
			}

			// Allocate a few large resources
			for (int i = 0; i < 3; ++i)
			{
				std::wstring resourceName = L"LargeResource_" + std::to_wstring(i);
				auto resource = heapManager.CreateResource(largeBufferDesc, resourceName.c_str());
				SEAssert(resource && resource->IsValid(), "Failed to create large resource");
				largeResources.push_back(std::move(resource));
			}

			// Free all resources to test page cleanup
			smallResources.clear();
			largeResources.clear();

			// Simulate multiple frames to allow empty page cleanup
			static uint64_t frameCounter = 100;
			for (int i = 0; i < 15; ++i) // More than k_numEmptyFramesBeforePageRelease
			{
				heapManager.BeginFrame(++frameCounter);
				heapManager.EndFrame();
			}
		}

	private:
		HeapManagerValidator() = delete;
		~HeapManagerValidator() = delete;
		HeapManagerValidator(HeapManagerValidator const&) = delete;
		HeapManagerValidator& operator=(HeapManagerValidator const&) = delete;
	};

} // namespace dx12

#endif // _DEBUG