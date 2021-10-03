#include "RHICommon.h"

D3D12InputLayout::operator D3D12_INPUT_LAYOUT_DESC() const noexcept
{
	for (size_t i = 0; i < InputElements.size(); ++i)
	{
		InputElements[i].SemanticName = SemanticNames[i].data();
	}

	D3D12_INPUT_LAYOUT_DESC Desc = {};
	Desc.pInputElementDescs		 = InputElements.data();
	Desc.NumElements			 = static_cast<UINT>(InputElements.size());
	return Desc;
}

void D3D12InputLayout::AddVertexLayoutElement(
	std::string_view SemanticName,
	UINT			 SemanticIndex,
	DXGI_FORMAT		 Format,
	UINT			 InputSlot)
{
	SemanticNames.emplace_back(SemanticName);

	D3D12_INPUT_ELEMENT_DESC& Desc = InputElements.emplace_back();
	Desc.SemanticName			   = nullptr; // Will be resolved later
	Desc.SemanticIndex			   = SemanticIndex;
	Desc.Format					   = Format;
	Desc.InputSlot				   = InputSlot;
	Desc.AlignedByteOffset		   = D3D12_APPEND_ALIGNED_ELEMENT;
	Desc.InputSlotClass			   = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	Desc.InstanceDataStepRate	   = 0;
}

void RHIParsePipelineStream(const PipelineStateStreamDesc& Desc, IPipelineParserCallbacks* Callbacks)
{
	if (Desc.SizeInBytes == 0 || Desc.pPipelineStateSubobjectStream == nullptr)
	{
		Callbacks->ErrorBadInputParameter(1); // first parameter issue
		return;
	}

	bool SubobjectSeen[static_cast<UINT>(PipelineStateSubobjectType::NumTypes)] = {};
	for (SIZE_T CurOffset = 0, SizeOfSubobject = 0; CurOffset < Desc.SizeInBytes; CurOffset += SizeOfSubobject)
	{
		BYTE* Stream		= static_cast<BYTE*>(Desc.pPipelineStateSubobjectStream) + CurOffset;
		auto  SubobjectType = *reinterpret_cast<PipelineStateSubobjectType*>(Stream);
		UINT  Index			= static_cast<UINT>(SubobjectType);

		if (Index < 0 || Index >= static_cast<UINT>(PipelineStateSubobjectType::NumTypes))
		{
			Callbacks->ErrorUnknownSubobject(Index);
			return;
		}
		if (SubobjectSeen[Index])
		{
			Callbacks->ErrorDuplicateSubobject(SubobjectType);
			return; // disallow subobject duplicates in a stream
		}
		SubobjectSeen[Index] = true;

		switch (SubobjectType)
		{
		case PipelineStateSubobjectType::RootSignature:
			Callbacks->RootSignatureCb(*reinterpret_cast<PipelineStateStreamRootSignature*>(Stream));
			SizeOfSubobject = sizeof(PipelineStateStreamRootSignature);
			break;
		case PipelineStateSubobjectType::VS:
			Callbacks->VSCb(*reinterpret_cast<PipelineStateStreamVS*>(Stream));
			SizeOfSubobject = sizeof(PipelineStateStreamVS);
			break;
		case PipelineStateSubobjectType::PS:
			Callbacks->PSCb(*reinterpret_cast<PipelineStateStreamPS*>(Stream));
			SizeOfSubobject = sizeof(PipelineStateStreamPS);
			break;
		case PipelineStateSubobjectType::DS:
			Callbacks->DSCb(*reinterpret_cast<PipelineStateStreamDS*>(Stream));
			SizeOfSubobject = sizeof(PipelineStateStreamPS);
			break;
		case PipelineStateSubobjectType::HS:
			Callbacks->HSCb(*reinterpret_cast<PipelineStateStreamHS*>(Stream));
			SizeOfSubobject = sizeof(PipelineStateStreamHS);
			break;
		case PipelineStateSubobjectType::GS:
			Callbacks->GSCb(*reinterpret_cast<PipelineStateStreamGS*>(Stream));
			SizeOfSubobject = sizeof(PipelineStateStreamGS);
			break;
		case PipelineStateSubobjectType::CS:
			Callbacks->CSCb(*reinterpret_cast<PipelineStateStreamCS*>(Stream));
			SizeOfSubobject = sizeof(PipelineStateStreamCS);
			break;
		case PipelineStateSubobjectType::BlendState:
			Callbacks->BlendStateCb(*reinterpret_cast<PipelineStateStreamBlendState*>(Stream));
			SizeOfSubobject = sizeof(PipelineStateStreamBlendState);
			break;
		case PipelineStateSubobjectType::RasterizerState:
			Callbacks->RasterizerStateCb(*reinterpret_cast<PipelineStateStreamRasterizerState*>(Stream));
			SizeOfSubobject = sizeof(PipelineStateStreamRasterizerState);
			break;
		case PipelineStateSubobjectType::DepthStencilState:
			Callbacks->DepthStencilStateCb(*reinterpret_cast<PipelineStateStreamDepthStencilState*>(Stream));
			SizeOfSubobject = sizeof(PipelineStateStreamDepthStencilState);
			break;
		case PipelineStateSubobjectType::InputLayout:
			Callbacks->InputLayoutCb(*reinterpret_cast<PipelineStateStreamInputLayout*>(Stream));
			SizeOfSubobject = sizeof(PipelineStateStreamInputLayout);
			break;
		case PipelineStateSubobjectType::PrimitiveTopology:
			Callbacks->PrimitiveTopologyTypeCb(*reinterpret_cast<PipelineStateStreamPrimitiveTopology*>(Stream));
			SizeOfSubobject = sizeof(PipelineStateStreamPrimitiveTopology);
			break;
		case PipelineStateSubobjectType::RenderPass:
			Callbacks->RenderPassCb(*reinterpret_cast<PipelineStateStreamRenderPass*>(Stream));
			SizeOfSubobject = sizeof(PipelineStateStreamRenderPass);
			break;
		default:
			Callbacks->ErrorUnknownSubobject(Index);
			return;
		}
	}
}
