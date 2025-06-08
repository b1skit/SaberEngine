// © 2023 Adam Badke. All rights reserved.
#include "Private/RasterizationState.h"

#include "Core/Util/CHashKey.h"
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


	RasterizationState::PrimitiveTopologyType RasterizationState::CStrToPrimitiveTopologyType(char const* name)
	{
		util::CHashKey const& nameHash = util::CHashKey::Create(util::ToLower(name));
		switch (nameHash)
		{
		case util::CHashKey("triangle"): return RasterizationState::PrimitiveTopologyType::Triangle;
		case util::CHashKey("point"): return RasterizationState::PrimitiveTopologyType::Point;
		case util::CHashKey("line"): return RasterizationState::PrimitiveTopologyType::Line;
		case util::CHashKey("patch"): return RasterizationState::PrimitiveTopologyType::Patch;
		default: SEAssertF("Invalid type name string");
		}
		return RasterizationState::PrimitiveTopologyType::Triangle; // This should never happen
	}


	RasterizationState::FillMode RasterizationState::GetFillModeByName(char const* name)
	{
		util::CHashKey const& nameHash = util::CHashKey::Create(util::ToLower(name));
		switch (nameHash)
		{
		case util::CHashKey("solid"): return RasterizationState::FillMode::Solid;
		case util::CHashKey("wireframe"): return RasterizationState::FillMode::Wireframe;
		default: SEAssertF("Invalid type name string");
		}
		return RasterizationState::FillMode::Solid; // This should never happen
	}


	RasterizationState::FaceCullingMode RasterizationState::GetFaceCullingModeByName(char const* name)
	{
		util::CHashKey const& nameHash = util::CHashKey::Create(util::ToLower(name));
		switch (nameHash)
		{
		case util::CHashKey("back"): return RasterizationState::FaceCullingMode::Back;
		case util::CHashKey("front"): return RasterizationState::FaceCullingMode::Front;
		case util::CHashKey("disabled"): return RasterizationState::FaceCullingMode::Disabled;
		default: SEAssertF("Invalid type name string");
		}
		return RasterizationState::FaceCullingMode::Back; // This should never happen
	}


	RasterizationState::WindingOrder RasterizationState::GetWindingOrderByName(char const* name)
	{
		util::CHashKey const& nameHash = util::CHashKey::Create(util::ToLower(name));
		switch (nameHash)
		{
		case util::CHashKey("ccw"): return RasterizationState::WindingOrder::CCW;
		case util::CHashKey("cw"): return RasterizationState::WindingOrder::CW;
		default: SEAssertF("Invalid type name string");
		}
		return RasterizationState::WindingOrder::CCW; // This should never happen
	}


	RasterizationState::ComparisonFunc RasterizationState::GetComparisonByName(char const* name)
	{
		util::CHashKey const& nameHash = util::CHashKey::Create(util::ToLower(name));
		switch (nameHash)
		{
		case util::CHashKey("less"): return RasterizationState::ComparisonFunc::Less;
		case util::CHashKey("never"): return RasterizationState::ComparisonFunc::Never;
		case util::CHashKey("equal"): return RasterizationState::ComparisonFunc::Equal;
		case util::CHashKey("lequal"): return RasterizationState::ComparisonFunc::LEqual;
		case util::CHashKey("greater"): return RasterizationState::ComparisonFunc::Greater;
		case util::CHashKey("notequal"): return RasterizationState::ComparisonFunc::NotEqual;
		case util::CHashKey("gequal"): return RasterizationState::ComparisonFunc::GEqual;
		case util::CHashKey("always"): return RasterizationState::ComparisonFunc::Always;
		default: SEAssertF("Invalid type name string");
		}
		return RasterizationState::ComparisonFunc::Less; // This should never happen
	}


	RasterizationState::DepthWriteMask RasterizationState::GetDepthWriteMaskByName(char const* name)
	{
		util::CHashKey const& nameHash = util::CHashKey::Create(util::ToLower(name));
		switch (nameHash)
		{
		case util::CHashKey("zero"): return RasterizationState::DepthWriteMask::Zero;
		case util::CHashKey("all"): return RasterizationState::DepthWriteMask::All;
		default: SEAssertF("Invalid type name string");
		}
		return RasterizationState::DepthWriteMask::All; // This should never happen
	}


	RasterizationState::StencilOp RasterizationState::GetStencilOpByName(char const* name)
	{
		util::CHashKey const& nameHash = util::CHashKey::Create(util::ToLower(name));
		switch (nameHash)
		{
		case util::CHashKey("keep"): return RasterizationState::StencilOp::Keep;
		case util::CHashKey("zero"): return RasterizationState::StencilOp::Zero;
		case util::CHashKey("replace"): return RasterizationState::StencilOp::Replace;
		case util::CHashKey("incrementsaturate"): return RasterizationState::StencilOp::IncrementSaturate;
		case util::CHashKey("decrementsaturate"): return RasterizationState::StencilOp::DecrementSaturate;
		case util::CHashKey("invert"): return RasterizationState::StencilOp::Invert;
		case util::CHashKey("increment"): return RasterizationState::StencilOp::Increment;
		case util::CHashKey("decrement"): return RasterizationState::StencilOp::Decrement;
		default: SEAssertF("Invalid type name string");
		}
		return RasterizationState::StencilOp::Keep; // This should never happen
	}


	RasterizationState::BlendMode RasterizationState::GetBlendModeByName(char const* name)
	{
		util::CHashKey const& nameHash = util::CHashKey::Create(util::ToLower(name));
		switch (nameHash)
		{
		case util::CHashKey("zero"): return RasterizationState::BlendMode::Zero;
		case util::CHashKey("one"): return RasterizationState::BlendMode::One;
		case util::CHashKey("srccolor"): return RasterizationState::BlendMode::SrcColor;
		case util::CHashKey("invsrccolor"): return RasterizationState::BlendMode::InvSrcColor;
		case util::CHashKey("srcalpha"): return RasterizationState::BlendMode::SrcAlpha;
		case util::CHashKey("invsrcalpha"): return RasterizationState::BlendMode::InvSrcAlpha;
		case util::CHashKey("dstalpha"): return RasterizationState::BlendMode::DstAlpha;
		case util::CHashKey("invdstalpha"): return RasterizationState::BlendMode::InvDstAlpha;
		case util::CHashKey("dstcolor"): return RasterizationState::BlendMode::DstColor;
		case util::CHashKey("invdstcolor"): return RasterizationState::BlendMode::InvDstColor;
		case util::CHashKey("srcalphasat"): return RasterizationState::BlendMode::SrcAlphaSat;
		case util::CHashKey("blendfactor"): return RasterizationState::BlendMode::BlendFactor;
		case util::CHashKey("invblendfactor"): return RasterizationState::BlendMode::InvBlendFactor;
		case util::CHashKey("srconecolor"): return RasterizationState::BlendMode::SrcOneColor;
		case util::CHashKey("invsrconecolor"): return RasterizationState::BlendMode::InvSrcOneColor;
		case util::CHashKey("srconealpha"): return RasterizationState::BlendMode::SrcOneAlpha;
		case util::CHashKey("invsrconealpha"): return RasterizationState::BlendMode::InvSrcOneAlpha;
		case util::CHashKey("alphafactor"): return RasterizationState::BlendMode::AlphaFactor;
		case util::CHashKey("invalphafactor"): return RasterizationState::BlendMode::InvAlphaFactor;
		default: SEAssertF("Invalid type name string");
		};
		return RasterizationState::BlendMode::Zero; // This should never happen
	}


	RasterizationState::BlendOp RasterizationState::GetBlendOpByName(char const* name)
	{
		util::CHashKey const& nameHash = util::CHashKey::Create(util::ToLower(name));
		switch (nameHash)
		{
			case util::CHashKey("add"): return RasterizationState::BlendOp::Add;
			case util::CHashKey("subtract"): return RasterizationState::BlendOp::Subtract;
			case util::CHashKey("revsubtract"): return RasterizationState::BlendOp::RevSubtract;
			case util::CHashKey("min"): return RasterizationState::BlendOp::Min;
			case util::CHashKey("max"): return RasterizationState::BlendOp::Max;
			default: SEAssertF("Invalid type name string");
		};
		return RasterizationState::BlendOp::Add; // This should never happen
	}


	RasterizationState::LogicOp RasterizationState::GetLogicOpByName(char const* name)
	{
		util::CHashKey const& nameHash = util::CHashKey::Create(util::ToLower(name));
		switch (nameHash)
		{
			case util::CHashKey("clear"): return RasterizationState::LogicOp::Clear;
			case util::CHashKey("set"): return RasterizationState::LogicOp::Set;
			case util::CHashKey("copy"): return RasterizationState::LogicOp::Copy;
			case util::CHashKey("copyinverted"): return RasterizationState::LogicOp::CopyInverted;
			case util::CHashKey("noop"): return RasterizationState::LogicOp::NoOp;
			case util::CHashKey("invert"): return RasterizationState::LogicOp::Invert;
			case util::CHashKey("and"): return RasterizationState::LogicOp::AND;
			case util::CHashKey("nand"): return RasterizationState::LogicOp::NAND;
			case util::CHashKey("or"): return RasterizationState::LogicOp::OR;
			case util::CHashKey("nor"): return RasterizationState::LogicOp::NOR;
			case util::CHashKey("xor"): return RasterizationState::LogicOp::XOR;
			case util::CHashKey("equiv"): return RasterizationState::LogicOp::EQUIV;
			case util::CHashKey("andreverse"): return RasterizationState::LogicOp::ANDReverse;
			case util::CHashKey("andinverted"): return RasterizationState::LogicOp::AndInverted;
			case util::CHashKey("orreverse"): return RasterizationState::LogicOp::ORReverse;
			case util::CHashKey("orinverted"): return RasterizationState::LogicOp::ORInverted;
			default: SEAssertF("Invalid type name string");
		};
		return RasterizationState::LogicOp::Clear; // This should never happen
	}
}