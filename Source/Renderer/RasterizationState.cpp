// © 2023 Adam Badke. All rights reserved.
#include "RasterizationState.h"

#include "Core/Util/TextUtils.h"


namespace re
{
	RasterizationState::RasterizationState()
		: m_isDirty(true)
		, m_primitiveTopologyType(PrimitiveTopologyType::Triangle)

		// Rasterizer state. Note: Defaults as per D3D12: 
		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_rasterizer_desc#remarks
		, m_fillMode(FillMode::Solid)
		, m_faceCullingMode(FaceCullingMode::Back)
		, m_windingOrder(WindingOrder::CCW)
		, m_depthBias(0)
		, m_depthBiasClamp(0.f)
		, m_slopeScaledDepthBias(0.f)
		, m_depthClipEnable(true)
		, m_multisampleEnable(false)
		, m_antialiasedLineEnable(false)
		, m_forcedSampleCount(0)
		, m_conservativeRaster(false)
		
		// Depth stencil state: Note: Defaults as per D3D12:
		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_depth_stencil_desc#remarks
		, m_depthTestEnable(true)
		, m_depthWriteMask(DepthWriteMask::All)
		, m_depthFunc(ComparisonFunc::Less)
		, m_stencilEnabled(false)
		, m_stencilReadMask(k_defaultStencilReadMask)
		, m_stencilWriteMask(k_defaultStencilWriteMask)
		, m_frontFace{}
		, m_backFace{}

		// Blend state. Note: Defaults as per D3D12: 
		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_blend_desc#remarks
		, m_alphaToCoverageEnable(false)
		, m_independentBlendEnable(false)
		, m_renderTargetBlendDescs{}
	{
		ComputeDataHash();
	}


	void RasterizationState::ComputeDataHash()
	{
		SEAssert(m_isDirty, "RasterizationState data is not dirty");
		m_isDirty = false;

		ResetDataHash();

		AddDataBytesToHash(m_primitiveTopologyType);

		// Rasterizer state:
		AddDataBytesToHash(m_fillMode);
		AddDataBytesToHash(m_faceCullingMode);
		AddDataBytesToHash(m_windingOrder);
		AddDataBytesToHash(m_depthBias);
		AddDataBytesToHash(m_depthBiasClamp);
		AddDataBytesToHash(m_slopeScaledDepthBias);
		AddDataBytesToHash(m_depthClipEnable);
		AddDataBytesToHash(m_multisampleEnable);
		AddDataBytesToHash(m_antialiasedLineEnable);
		AddDataBytesToHash(m_forcedSampleCount);
		AddDataBytesToHash(m_conservativeRaster);

		// Depth stencil state:
		AddDataBytesToHash(m_depthTestEnable);
		AddDataBytesToHash(m_depthWriteMask);
		AddDataBytesToHash(m_depthFunc);
		AddDataBytesToHash(m_stencilEnabled);
		AddDataBytesToHash(m_stencilReadMask);
		AddDataBytesToHash(m_stencilWriteMask);
		AddDataBytesToHash(m_frontFace);
		AddDataBytesToHash(m_backFace);

		// Blend state:
		AddDataBytesToHash(m_alphaToCoverageEnable);
		AddDataBytesToHash(m_independentBlendEnable);
		for (auto const& renderTargetBlendDesc : m_renderTargetBlendDescs)
		{
			AddDataBytesToHash(renderTargetBlendDesc);
		}
	}


	util::HashKey RasterizationState::GetDataHash() const
	{
		SEAssert(!m_isDirty, "Trying to get the data hash from a dirty pipeline state");
		return core::IHashedDataObject::GetDataHash();
	}


	RasterizationState::PrimitiveTopologyType RasterizationState::GetPrimitiveTopologyType() const
	{
		SEAssert(!m_isDirty, "RasterizationState is dirty");
		return m_primitiveTopologyType;
	}


	void RasterizationState::SetPrimitiveTopologyType(RasterizationState::PrimitiveTopologyType topologyType)
	{
		m_primitiveTopologyType = topologyType;
		m_isDirty = true;
		ComputeDataHash();
	}


	RasterizationState::PrimitiveTopologyType RasterizationState::CStrToPrimitiveTopologyType(char const* name)
	{
		static const std::map<std::string, RasterizationState::PrimitiveTopologyType> s_nameToType =
		{
			{"triangle", RasterizationState::PrimitiveTopologyType::Triangle},
			{"point", RasterizationState::PrimitiveTopologyType::Point},
			{"line", RasterizationState::PrimitiveTopologyType::Line},
			{"patch", RasterizationState::PrimitiveTopologyType::Patch},
		};
		
		std::string const& lowerCaseName = util::ToLower(name);
		SEAssert(s_nameToType.contains(lowerCaseName), "Invalid type name string");

		return s_nameToType.at(lowerCaseName);
	}


	RasterizationState::FillMode RasterizationState::GetFillMode() const
	{
		SEAssert(!m_isDirty, "RasterizationState is dirty");
		return m_fillMode;
	}

	void RasterizationState::SetFillMode(FillMode fillMode)
	{
		m_fillMode = fillMode;
		m_isDirty = true;
		ComputeDataHash();
	}


	RasterizationState::FillMode RasterizationState::GetFillModeByName(char const* name)
	{
		static const std::map<std::string, RasterizationState::FillMode> s_nameToType =
		{
			{"solid", RasterizationState::FillMode::Solid},
			{"wireframe", RasterizationState::FillMode::Wireframe},
		};

		std::string const& lowerCaseName = util::ToLower(name);
		SEAssert(s_nameToType.contains(lowerCaseName), "Invalid type name string");

		return s_nameToType.at(lowerCaseName);
	}


	RasterizationState::FaceCullingMode RasterizationState::GetFaceCullingMode() const
	{
		SEAssert(!m_isDirty, "RasterizationState is dirty");
		return m_faceCullingMode;
	}


	void RasterizationState::SetFaceCullingMode(RasterizationState::FaceCullingMode faceCullingMode)
	{
		m_faceCullingMode = faceCullingMode;
		m_isDirty = true;
		ComputeDataHash();
	}


	RasterizationState::FaceCullingMode RasterizationState::GetFaceCullingModeByName(char const* name)
	{
		static const std::map<std::string, RasterizationState::FaceCullingMode> s_nameToType =
		{
			{"back", RasterizationState::FaceCullingMode::Back},
			{"front", RasterizationState::FaceCullingMode::Front},
			{"disabled", RasterizationState::FaceCullingMode::Disabled},
		};

		std::string const& lowerCaseName = util::ToLower(name);
		SEAssert(s_nameToType.contains(lowerCaseName), "Invalid type name string");

		return s_nameToType.at(lowerCaseName);
	}


	RasterizationState::WindingOrder RasterizationState::GetWindingOrder() const
	{
		SEAssert(!m_isDirty, "RasterizationState is dirty");
		return m_windingOrder;
	}


	void RasterizationState::SetWindingOrder(RasterizationState::WindingOrder windingOrder)
	{
		m_windingOrder = windingOrder;
		m_isDirty = true;
		ComputeDataHash();
	}


	RasterizationState::WindingOrder RasterizationState::GetWindingOrderByName(char const* name)
	{
		static const std::map<std::string, RasterizationState::WindingOrder> s_nameToType =
		{
			{"ccw", RasterizationState::WindingOrder::CCW},
			{"cw", RasterizationState::WindingOrder::CW},
		};

		std::string const& lowerCaseName = util::ToLower(name);
		SEAssert(s_nameToType.contains(lowerCaseName), "Invalid type name string");

		return s_nameToType.at(lowerCaseName);
	}


	RasterizationState::ComparisonFunc RasterizationState::GetDepthComparison() const
	{
		SEAssert(!m_isDirty, "RasterizationState is dirty");
		return m_depthFunc;
	}


	void RasterizationState::SetDepthComparison(RasterizationState::ComparisonFunc depthTestMode)
	{
		m_depthFunc = depthTestMode;
		m_isDirty = true;
		ComputeDataHash();
	}


	RasterizationState::ComparisonFunc RasterizationState::GetComparisonByName(char const* name)
	{
		static const std::map<std::string, RasterizationState::ComparisonFunc> s_nameToType =
		{
			{"less", RasterizationState::ComparisonFunc::Less},
			{"never", RasterizationState::ComparisonFunc::Never},
			{"equal", RasterizationState::ComparisonFunc::Equal},
			{"lequal", RasterizationState::ComparisonFunc::LEqual},
			{"greater", RasterizationState::ComparisonFunc::Greater},
			{"notequal", RasterizationState::ComparisonFunc::NotEqual},
			{"gequal", RasterizationState::ComparisonFunc::GEqual},
			{"always", RasterizationState::ComparisonFunc::Always},
		};

		std::string const& lowerCaseName = util::ToLower(name);
		SEAssert(s_nameToType.contains(lowerCaseName), "Invalid type name string");

		return s_nameToType.at(lowerCaseName);
	}


	RasterizationState::DepthWriteMask RasterizationState::GetDepthWriteMaskByName(char const* name)
	{
		static const std::map<std::string, RasterizationState::DepthWriteMask> s_nameToType =
		{
			{"zero", RasterizationState::DepthWriteMask::Zero},
			{"all", RasterizationState::DepthWriteMask::All},
		};

		std::string const& lowerCaseName = util::ToLower(name);
		SEAssert(s_nameToType.contains(lowerCaseName), "Invalid type name string");

		return s_nameToType.at(lowerCaseName);
	}


	RasterizationState::StencilOp RasterizationState::GetStencilOpByName(char const* name)
	{
		static const std::map<std::string, RasterizationState::StencilOp> s_nameToType =
		{
			{"keep", RasterizationState::StencilOp::Keep},
			{"zero", RasterizationState::StencilOp::Zero},
			{"replace", RasterizationState::StencilOp::Replace},
			{"incrementsaturate", RasterizationState::StencilOp::IncrementSaturate},
			{"decrementsaturate", RasterizationState::StencilOp::DecrementSaturate},
			{"invert", RasterizationState::StencilOp::Invert},
			{"increment", RasterizationState::StencilOp::Increment},
			{"decrement", RasterizationState::StencilOp::Decrement},
		};

		std::string const& lowerCaseName = util::ToLower(name);
		SEAssert(s_nameToType.contains(lowerCaseName), "Invalid type name string");

		return s_nameToType.at(lowerCaseName);
	}


	RasterizationState::BlendMode RasterizationState::GetBlendModeByName(char const* name)
	{
		static const std::map<std::string, RasterizationState::BlendMode> s_nameToType =
		{
			{"zero", RasterizationState::BlendMode::Zero},
			{"one", RasterizationState::BlendMode::One},
			{"srccolor", RasterizationState::BlendMode::SrcColor},
			{"invsrccolor", RasterizationState::BlendMode::InvSrcColor},
			{"srcalpha", RasterizationState::BlendMode::SrcAlpha},
			{"invsrcalpha", RasterizationState::BlendMode::InvSrcAlpha},
			{"dstalpha", RasterizationState::BlendMode::DstAlpha},
			{"invdstalpha", RasterizationState::BlendMode::InvDstAlpha},
			{"dstcolor", RasterizationState::BlendMode::DstColor},
			{"invdstcolor", RasterizationState::BlendMode::InvDstColor},
			{"srcalphasat", RasterizationState::BlendMode::SrcAlphaSat},
			{"blendfactor", RasterizationState::BlendMode::BlendFactor},
			{"invblendfactor", RasterizationState::BlendMode::InvBlendFactor},
			{"srconecolor", RasterizationState::BlendMode::SrcOneColor},
			{"invsrconecolor", RasterizationState::BlendMode::InvSrcOneColor},
			{"srconealpha", RasterizationState::BlendMode::SrcOneAlpha},
			{"invsrconealpha", RasterizationState::BlendMode::InvSrcOneAlpha},
			{"alphafactor", RasterizationState::BlendMode::AlphaFactor},
			{"invalphafactor", RasterizationState::BlendMode::InvAlphaFactor},
		};

		std::string const& lowerCaseName = util::ToLower(name);
		SEAssert(s_nameToType.contains(lowerCaseName), "Invalid type name string");

		return s_nameToType.at(lowerCaseName);
	}


	RasterizationState::BlendOp RasterizationState::GetBlendOpByName(char const* name)
	{
		static const std::map<std::string, RasterizationState::BlendOp> s_nameToType =
		{
			{"add", RasterizationState::BlendOp::Add},
			{"subtract", RasterizationState::BlendOp::Subtract},
			{"revsubtract", RasterizationState::BlendOp::RevSubtract},
			{"min", RasterizationState::BlendOp::Min},
			{"max", RasterizationState::BlendOp::Max},
		};

		std::string const& lowerCaseName = util::ToLower(name);
		SEAssert(s_nameToType.contains(lowerCaseName), "Invalid type name string");

		return s_nameToType.at(lowerCaseName);
	}


	RasterizationState::LogicOp RasterizationState::GetLogicOpByName(char const* name)
	{
		static const std::map<std::string, RasterizationState::LogicOp> s_nameToType =
		{
			{"clear", RasterizationState::LogicOp::Clear},
			{"set", RasterizationState::LogicOp::Set},
			{"copy", RasterizationState::LogicOp::Copy},
			{"copyinverted", RasterizationState::LogicOp::CopyInverted},
			{"noop", RasterizationState::LogicOp::NoOp},
			{"invert", RasterizationState::LogicOp::Invert},
			{"and", RasterizationState::LogicOp::AND},
			{"nand", RasterizationState::LogicOp::NAND},
			{"or", RasterizationState::LogicOp::OR},
			{"nor", RasterizationState::LogicOp::NOR},
			{"xor", RasterizationState::LogicOp::XOR},
			{"equiv", RasterizationState::LogicOp::EQUIV},
			{"andreverse", RasterizationState::LogicOp::ANDReverse},
			{"andinverted", RasterizationState::LogicOp::AndInverted},
			{"orreverse", RasterizationState::LogicOp::ORReverse},
			{"orinverted", RasterizationState::LogicOp::ORInverted},
		};

		std::string const& lowerCaseName = util::ToLower(name);
		SEAssert(s_nameToType.contains(lowerCaseName), "Invalid type name string");

		return s_nameToType.at(lowerCaseName);
	}
}