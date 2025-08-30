// © 2025 Adam Badke. All rights reserved.
#include "GraphicsUtils.h"

#include "Core/Assert.h"
#include "Core/Logger.h"
#include "Core/ThreadPool.h"

#include "Core/Host/PerformanceTimer.h"


namespace grutil
{
	std::unique_ptr<AliasTableData> CreateAliasTableData(
		re::Texture::TextureParams const& texParams, re::Texture::IInitialData const* texData)
	{
		host::PerformanceTimer aliasTableTimer;
		aliasTableTimer.Start();

		SEAssert(texData, "Trying to create alias table data from null texData");
		SEAssert(texData->ArrayDepth() == 1 && texData->NumFaces() == 1, 
			"Unexpected dimensions for IBL texture");

		SEAssert(texParams.m_format == re::Texture::Format::RGBA32F,
			"Unexpected IBL texture format (we pad RGB -> RGBA)");

		SEAssert(texParams.m_width <= (1u << 24) &&
			texParams.m_height <= (1u << 24),
			"Width or height are too large to be held as integers in floats (first unrepresentable integer = 2^24 + 1)."
			" Consider changing the AliasTableData types");

		auto Compute1DTableData = [](
			double totalWeightedDimensionLuminance,
			std::vector<double> const& weightedDimensionLuminances,
			std::span<glm::vec2> aliasTableDataOut)
			{
				const uint32_t numElements = util::CheckedCast<uint32_t>(weightedDimensionLuminances.size());

				// Early out if there is ~0 luminance, just return a uniform distribution over all entries
				constexpr double k_epsilon = 1e-12;
				if (totalWeightedDimensionLuminance <= k_epsilon)
				{
					for (uint32_t i = 0; i < numElements; ++i)
					{
						aliasTableDataOut[i] = glm::vec2(1.f, static_cast<float>(i));
					}
					return;
				}

				struct Entry
				{
					uint32_t m_index;			// Note: Index of the actual element, NOT the alias index
					double m_scaledProbability;	// p(entry) * numElements, where p(entry) = [0,1]
				};

				// Note: We use thread_local here to avoid reallocation overheads when this function is called. The
				// downside is that this memory will never be freed until the thread exits, but this is ok for now...
				static thread_local std::vector<Entry> smaller;
				smaller.clear();
				smaller.reserve(numElements);

				static thread_local std::vector<Entry> larger;
				larger.clear();
				larger.reserve(numElements);

				// Classification: Divide elements based on whether they contain over/under the average luminance
				for (uint32_t rowIdx = 0; rowIdx < numElements; ++rowIdx)
				{
					const double entryProbability = weightedDimensionLuminances[rowIdx] / totalWeightedDimensionLuminance;
					const double scaledProbability = entryProbability * numElements;

					// Convert luminance to normalized probability [0,1]
					if (scaledProbability <= (1.0 + k_epsilon))
					{
						smaller.emplace_back(Entry{ rowIdx, scaledProbability });
					}
					else
					{
						larger.emplace_back(Entry{ rowIdx, scaledProbability });
					}
				}

				// Populate the row marginal alias table data:
				while (!smaller.empty() && !larger.empty())
				{
					Entry const& under = smaller.back();
					Entry over = larger.back();

					aliasTableDataOut[under.m_index] = glm::vec2(
						static_cast<float>(glm::clamp(under.m_scaledProbability, 0.0, 1.0)),// p(under) * numElements
						util::CheckedCast<float>(over.m_index)								// alias index
					);

					SEAssert(under.m_scaledProbability <= (1.0 + k_epsilon),
						"Probability is about to underflow. This should not be possible");
					SEAssert(over.m_scaledProbability > (1.0 - (under.m_scaledProbability + k_epsilon)),
						"Probability is about to underflow. This should not be possible");

					// Remove the remaining space allocated from the under entry
					over.m_scaledProbability -= (1.0 - under.m_scaledProbability);

					smaller.pop_back();
					larger.pop_back();

					// Add the remaining entry back to the appropriate queue:
					if (over.m_scaledProbability <= (1.0 + k_epsilon))
					{
						smaller.emplace_back(over);
					}
					else
					{
						larger.emplace_back(over);
					}
				}

				// Finalize any leftover elements:
				while (!larger.empty())
				{
					aliasTableDataOut[larger.back().m_index] = glm::vec2(
						1.f,												// p(this entry)
						util::CheckedCast<float>(larger.back().m_index));	// alias index
					larger.pop_back();
				}

				while (!smaller.empty())
				{
					aliasTableDataOut[smaller.back().m_index] = glm::vec2(
						1.f,												// p(this entry)
						util::CheckedCast<float>(smaller.back().m_index));	// alias index
					smaller.pop_back();
				}
			};


		// Create the alias table data:
		std::unique_ptr<AliasTableData> aliasTableData = std::make_unique<AliasTableData>();
		aliasTableData->m_rowData.resize(texParams.m_height, glm::vec2(0.f, 0.f));
		aliasTableData->m_columnData.resize(texParams.m_height * texParams.m_width, glm::vec2(0.f, 0.f));

		glm::vec4 const* const data = reinterpret_cast<glm::vec4 const*>(texData->GetDataBytes(0, 0));

		// Row marginal alias table
		std::vector<double> weightedRowLuminances;
		weightedRowLuminances.resize(texParams.m_height, 0.0);
		std::atomic<double> totalWeightedRowLuminance = 0.0;

		// Process rows in parallel tasks:
		std::vector<std::future<void>> rowTasks;
		rowTasks.reserve(texParams.m_height);

		constexpr uint32_t k_numRowsPerTask = 128;

		uint32_t taskBaseRow = 0;
		while (taskBaseRow < texParams.m_height)
		{
			rowTasks.emplace_back(core::ThreadPool::EnqueueJob(
				[&texParams, &data, &totalWeightedRowLuminance, &Compute1DTableData, taskBaseRow, k_numRowsPerTask,
				&weightedRowLuminances, &aliasTableData]() mutable
				{
					// Column conditional alias tables: We reuse this per row to minimize the working memory required
					std::vector<double> weightedColLuminances;
					weightedColLuminances.resize(texParams.m_width);

					double localTotalWeightedRowLuminance = 0.0; // Local sum to minimize atomic contention

					for (uint32_t row = taskBaseRow; (row < taskBaseRow + k_numRowsPerTask && row < texParams.m_height); row++)
					{
						// Compute the pixel center to polar angle:
						const double theta = ((row + 0.5) * glm::pi<double>()) / texParams.m_height;
						const double rowSinTheta = glm::sin(theta); // We weight by sin(theta) to account for lat/long distortion

						const uint32_t rowStartIndex = row * texParams.m_width;

						double currentRowLuminance = 0.0;
						for (uint32_t col = 0; col < texParams.m_width; col++)
						{
							const uint32_t dataIndex = rowStartIndex + col;

							// Compute the weighted texel luminance:
							glm::vec4 const& texel = data[dataIndex];
							const float luminance = LinearToLuminance(texel.rgb);
							const double weightedLuminance = luminance * rowSinTheta;

							// Record the weighted luminance:
							currentRowLuminance += weightedLuminance;
							localTotalWeightedRowLuminance += weightedLuminance;

							weightedColLuminances[col] = weightedLuminance;
						}

						// Record the row's total weighted luminance once, to minimize cache thrashing:
						weightedRowLuminances[row] = currentRowLuminance;

						// We've populated the column data, now compute the alias table for the row's columns:
						const size_t baseIdx = static_cast<size_t>(row) * texParams.m_width;

						Compute1DTableData(
							weightedRowLuminances[row],
							weightedColLuminances,
							std::span{ aliasTableData->m_columnData }.subspan(baseIdx, texParams.m_width));
					}

					// Update the atomic total once, now that we're done:
					totalWeightedRowLuminance.fetch_add(localTotalWeightedRowLuminance, std::memory_order_relaxed);
				}
			));
			taskBaseRow += k_numRowsPerTask;
		}

		// Wait for all tasks to complete:
		for (std::future<void>& task : rowTasks)
		{
			task.wait();
		}

		// Finally, compute the row marginal alias table:
		Compute1DTableData(
			totalWeightedRowLuminance.load(std::memory_order_relaxed), weightedRowLuminances, aliasTableData->m_rowData);

		LOG("Created environment map alias table (%dx%d) in %fs",
			texParams.m_width, texParams.m_height, aliasTableTimer.StopSec());

		return aliasTableData;
	}
}