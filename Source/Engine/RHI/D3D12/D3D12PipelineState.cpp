#include "D3D12PipelineState.h"
#include "D3D12Device.h"
#include "D3D12RootSignature.h"
#include "ShaderCompiler.h"

#include <unordered_set>

namespace RHI
{
	D3D12_SHADER_BYTECODE RHITranslateD3D12(Shader* Shader)
	{
		if (Shader)
		{
			return { .pShaderBytecode = Shader->GetPointer(), .BytecodeLength = Shader->GetSize() };
		}
		return {};
	}

	static VOID NTAPI CompileFromCoroutineHandle(
		_Inout_ PTP_CALLBACK_INSTANCE Instance,
		_Inout_opt_ PVOID			  Context,
		_Inout_ PTP_WORK			  Work) noexcept
	{
		UNREFERENCED_PARAMETER(Instance);
		std::coroutine_handle<>::from_address(Context)();
		CloseThreadpoolWork(Work);
	}

	struct awaitable_CompilePsoThread
	{
		constexpr bool await_ready() const noexcept { return false; }

		constexpr void await_resume() const noexcept {}

		void await_suspend(std::coroutine_handle<> handle) const
		{
			Device->GetPsoCompilationThreadPool()->QueueThreadpoolWork(CompileFromCoroutineHandle, handle.address());
		}

		D3D12Device* Device;
	};

	D3D12PipelineState::D3D12PipelineState(
		D3D12Device*				   Parent,
		std::wstring				   Name,
		const PipelineStateStreamDesc& Desc)
		: D3D12DeviceChild(Parent)
	{
		D3D12PipelineParserCallbacks Parser;
		RHIParsePipelineStream(Desc, &Parser);
		CompilationWork = Create(Parent, Name, Parser.Type, Parser);
	}

	ID3D12PipelineState* D3D12PipelineState::GetApiHandle() const noexcept
	{
		if (CompilationWork)
		{
			CompilationWork.Wait();
			PipelineState	= CompilationWork.Get();
			CompilationWork = nullptr;
		}
		return PipelineState.Get();
	}

	AsyncTask<Arc<ID3D12PipelineState>> D3D12PipelineState::Create(
		D3D12Device*				 Device,
		std::wstring				 Name,
		RHI_PIPELINE_STATE_TYPE		 Type,
		D3D12PipelineParserCallbacks Parser)
	{
		if (Device->AllowAsyncPsoCompilation())
		{
			co_await awaitable_CompilePsoThread{ Device };
		}
		else
		{
			co_await std::suspend_never{};
		}

		if (Type == RHI_PIPELINE_STATE_TYPE::Graphics)
		{
			if (!Parser.MS)
			{
				co_return CompileGraphicsPipeline(Device, Name, Parser);
			}
			else
			{
				co_return CompileMeshShaderPipeline(Device, Name, Parser);
			}
		}
		else if (Type == RHI_PIPELINE_STATE_TYPE::Compute)
		{
			co_return CompileComputePipline(Device, Name, Parser);
		}
	}

	Arc<ID3D12PipelineState> D3D12PipelineState::CompileGraphicsPipeline(
		D3D12Device*						Device,
		const std::wstring&					Name,
		const D3D12PipelineParserCallbacks& Parser)
	{
		ScopedTimer Timer(
			[&](i64 Milliseconds)
			{
				LUNA_LOG(D3D12RHI, Info, L"Thread: {} has finished compiling PSO: {} in {}ms", GetCurrentThreadId(), Name, Milliseconds);
			});

		Arc<ID3D12PipelineState>		   PipelineState;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC Desc = {
			.pRootSignature		   = Parser.RootSignature->GetApiHandle(),
			.VS					   = RHITranslateD3D12(Parser.VS),
			.PS					   = RHITranslateD3D12(Parser.PS),
			.DS					   = RHITranslateD3D12(Parser.DS),
			.HS					   = RHITranslateD3D12(Parser.HS),
			.GS					   = RHITranslateD3D12(Parser.GS),
			.StreamOutput		   = D3D12_STREAM_OUTPUT_DESC(),
			.BlendState			   = RHITranslateD3D12(Parser.BlendState),
			.SampleMask			   = DefaultSampleMask(),
			.RasterizerState	   = RHITranslateD3D12(Parser.RasterizerState),
			.DepthStencilState	   = RHITranslateD3D12(Parser.DepthStencilState),
			.InputLayout		   = { Parser.InputElements.data(), static_cast<UINT>(Parser.InputElements.size()) },
			.IBStripCutValue	   = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
			.PrimitiveTopologyType = RHITranslateD3D12(Parser.PrimitiveTopology),
			.NumRenderTargets	   = Parser.RenderTargetState.NumRenderTargets,
			.DSVFormat			   = Parser.RenderTargetState.DSFormat,
			.SampleDesc			   = DefaultSampleDesc(),
			.NodeMask			   = Device->GetAllNodeMask(),
			.CachedPSO			   = D3D12_CACHED_PIPELINE_STATE(),
			.Flags				   = D3D12_PIPELINE_STATE_FLAG_NONE
		};
		memcpy(Desc.RTVFormats, Parser.RenderTargetState.RTFormats, sizeof(Desc.RTVFormats));

		if (Device->GetPipelineLibrary())
		{
			ID3D12PipelineLibrary1* PipelineLibrary1 = Device->GetPipelineLibrary()->GetLibrary1();
			HRESULT					Result			 = PipelineLibrary1->LoadGraphicsPipeline(Name.data(), &Desc, IID_PPV_ARGS(&PipelineState));
			if (Result == E_INVALIDARG)
			{
				VERIFY_D3D12_API(Device->GetD3D12Device5()->CreateGraphicsPipelineState(&Desc, IID_PPV_ARGS(&PipelineState)));
				StorePipeline(Device, Name, PipelineState.Get());
			}
		}
		else
		{
			VERIFY_D3D12_API(Device->GetD3D12Device5()->CreateGraphicsPipelineState(&Desc, IID_PPV_ARGS(&PipelineState)));
		}
		return PipelineState;
	}

	Arc<ID3D12PipelineState> D3D12PipelineState::CompileMeshShaderPipeline(
		D3D12Device*						Device,
		const std::wstring&					Name,
		const D3D12PipelineParserCallbacks& Parser)
	{
		ScopedTimer Timer(
			[&](i64 Milliseconds)
			{
				LUNA_LOG(D3D12RHI, Info, L"Thread: {} has finished compiling PSO: {} in {}ms", GetCurrentThreadId(), Name, Milliseconds);
			});

		Arc<ID3D12PipelineState>			   PipelineState;
		D3DX12_MESH_SHADER_PIPELINE_STATE_DESC Desc = {
			.pRootSignature		   = Parser.RootSignature->GetApiHandle(),
			.MS					   = RHITranslateD3D12(Parser.MS),
			.PS					   = RHITranslateD3D12(Parser.PS),
			.BlendState			   = RHITranslateD3D12(Parser.BlendState),
			.SampleMask			   = DefaultSampleMask(),
			.RasterizerState	   = RHITranslateD3D12(Parser.RasterizerState),
			.DepthStencilState	   = RHITranslateD3D12(Parser.DepthStencilState),
			.PrimitiveTopologyType = RHITranslateD3D12(Parser.PrimitiveTopology),
			.NumRenderTargets	   = Parser.RenderTargetState.NumRenderTargets,
			.DSVFormat			   = Parser.RenderTargetState.DSFormat,
			.SampleDesc			   = DefaultSampleDesc(),
			.NodeMask			   = Device->GetAllNodeMask(),
			.CachedPSO			   = D3D12_CACHED_PIPELINE_STATE(),
			.Flags				   = D3D12_PIPELINE_STATE_FLAG_NONE
		};
		memcpy(Desc.RTVFormats, Parser.RenderTargetState.RTFormats, sizeof(Desc.RTVFormats));

		CD3DX12_PIPELINE_STATE_STREAM2	 Stream(Desc);
		D3D12_PIPELINE_STATE_STREAM_DESC StreamDesc = {
			.SizeInBytes				   = sizeof(Stream),
			.pPipelineStateSubobjectStream = &Stream
		};

		if (Device->GetPipelineLibrary())
		{
			ID3D12PipelineLibrary1* PipelineLibrary1 = Device->GetPipelineLibrary()->GetLibrary1();
			HRESULT					Result			 = PipelineLibrary1->LoadPipeline(Name.data(), &StreamDesc, IID_PPV_ARGS(&PipelineState));
			if (Result == E_INVALIDARG)
			{
				VERIFY_D3D12_API(Device->GetD3D12Device5()->CreatePipelineState(&StreamDesc, IID_PPV_ARGS(&PipelineState)));
				StorePipeline(Device, Name, PipelineState.Get());
			}
		}
		else
		{
			VERIFY_D3D12_API(Device->GetD3D12Device5()->CreatePipelineState(&StreamDesc, IID_PPV_ARGS(&PipelineState)));
		}
		return PipelineState;
	}

	Arc<ID3D12PipelineState> D3D12PipelineState::CompileComputePipline(
		D3D12Device*						Device,
		const std::wstring&					Name,
		const D3D12PipelineParserCallbacks& Parser)
	{
		ScopedTimer Timer(
			[&](i64 Milliseconds)
			{
				LUNA_LOG(D3D12RHI, Info, L"Thread: {} has finished compiling PSO: {} in {}ms", GetCurrentThreadId(), Name, Milliseconds);
			});

		Arc<ID3D12PipelineState>		  PipelineState;
		D3D12_COMPUTE_PIPELINE_STATE_DESC Desc = {
			.pRootSignature = Parser.RootSignature->GetApiHandle(),
			.CS				= RHITranslateD3D12(Parser.CS),
			.NodeMask		= Device->GetAllNodeMask(),
			.CachedPSO		= D3D12_CACHED_PIPELINE_STATE(),
			.Flags			= D3D12_PIPELINE_STATE_FLAG_NONE
		};

		if (Device->GetPipelineLibrary())
		{
			ID3D12PipelineLibrary1* PipelineLibrary1 = Device->GetPipelineLibrary()->GetLibrary1();
			HRESULT					Result			 = PipelineLibrary1->LoadComputePipeline(Name.data(), &Desc, IID_PPV_ARGS(&PipelineState));
			if (Result == E_INVALIDARG)
			{
				VERIFY_D3D12_API(Device->GetD3D12Device5()->CreateComputePipelineState(&Desc, IID_PPV_ARGS(&PipelineState)));
				StorePipeline(Device, Name, PipelineState.Get());
			}
		}
		else
		{
			VERIFY_D3D12_API(Device->GetD3D12Device5()->CreateComputePipelineState(&Desc, IID_PPV_ARGS(&PipelineState)));
		}
		return PipelineState;
	}

	void D3D12PipelineState::StorePipeline(
		D3D12Device*		 Device,
		const std::wstring&	 Name,
		ID3D12PipelineState* PipelineState)
	{
		HRESULT Result = Device->GetPipelineLibrary()->GetLibrary1()->StorePipeline(Name.data(), PipelineState);
		if (Result == E_INVALIDARG)
		{
			// A PSO with the specified name already exists in the library.
			// If that is the case, we invalidate disk cache, so when next time app
			// starts, pipeline library will be renewed with the updated PSOs.
			Device->GetPipelineLibrary()->InvalidateDiskCache();
		}
		else
		{
			VERIFY_D3D12_API(Result);
		}
	}

	RaytracingPipelineStateDesc::RaytracingPipelineStateDesc() noexcept
		: Desc(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE)
		, GlobalRootSignature(nullptr)
		, ShaderConfig()
		, PipelineConfig()
	{
	}

	RaytracingPipelineStateDesc& RaytracingPipelineStateDesc::AddLibrary(
		const D3D12_SHADER_BYTECODE&		  Library,
		const std::vector<std::wstring_view>& Symbols)
	{
		// Add a DXIL library to the pipeline. The exported symbols must correspond exactly to the
		// names of the shaders declared in the library, although unused ones can be omitted.
		Libraries.emplace_back(DxilLibrary(Library, Symbols));
		return *this;
	}

	RaytracingPipelineStateDesc& RaytracingPipelineStateDesc::AddHitGroup(
		std::wstring_view				 HitGroupName,
		std::optional<std::wstring_view> AnyHitSymbol,
		std::optional<std::wstring_view> ClosestHitSymbol,
		std::optional<std::wstring_view> IntersectionSymbol)
	{
		// In DXR the hit-related shaders are grouped into hit groups. Such shaders are:
		// - The intersection shader, which can be used to intersect custom geometry, and is called upon
		//   hitting the bounding box the the object. A default one exists to intersect triangles
		// - The any hit shader, called on each intersection, which can be used to perform early
		//   alpha-testing and allow the ray to continue if needed. Default is a pass-through.
		// - The closest hit shader, invoked on the hit point closest to the ray start.
		// The shaders in a hit group share the same root signature, and are only referred to by the
		// hit group name in other places of the program.
		HitGroups.emplace_back(HitGroup(HitGroupName, AnyHitSymbol, ClosestHitSymbol, IntersectionSymbol));

		// 3 different shaders can be invoked to obtain an intersection: an
		// intersection shader is called when hitting the bounding box of non-triangular geometry.
		// An any-hit shader is called on potential intersections.
		// This shader can, for example, perform alpha-testing and
		// discard some intersections. Finally, the closest-hit program is invoked on
		// the intersection point closest to the ray origin. Those 3 shaders are bound
		// together into a hit group.

		// Note that for triangular geometry the intersection shader is built-in. An
		// empty any-hit shader is also defined by default, so in our simple case each
		// hit group contains only the closest hit shader. Note that since the
		// exported symbols are defined above the shaders can be simply referred to by
		// name.
		return *this;
	}

	RaytracingPipelineStateDesc& RaytracingPipelineStateDesc::AddRootSignatureAssociation(
		ID3D12RootSignature*				  RootSignature,
		const std::vector<std::wstring_view>& Symbols)
	{
		RootSignatureAssociations.emplace_back(RootSignatureAssociation(RootSignature, Symbols));
		return *this;
	}

	RaytracingPipelineStateDesc& RaytracingPipelineStateDesc::SetGlobalRootSignature(
		ID3D12RootSignature* GlobalRootSignature)
	{
		this->GlobalRootSignature = GlobalRootSignature;
		return *this;
	}

	RaytracingPipelineStateDesc& RaytracingPipelineStateDesc::SetRaytracingShaderConfig(
		UINT MaxPayloadSizeInBytes,
		UINT MaxAttributeSizeInBytes)
	{
		ShaderConfig.MaxPayloadSizeInBytes	 = MaxPayloadSizeInBytes;
		ShaderConfig.MaxAttributeSizeInBytes = MaxAttributeSizeInBytes;
		return *this;
	}

	RaytracingPipelineStateDesc& RaytracingPipelineStateDesc::SetRaytracingPipelineConfig(
		UINT MaxTraceRecursionDepth)
	{
		PipelineConfig.MaxTraceRecursionDepth = MaxTraceRecursionDepth;
		return *this;
	}

	D3D12_STATE_OBJECT_DESC RaytracingPipelineStateDesc::Build()
	{
		// Add all the DXIL libraries
		for (const auto& [Library, Symbols] : Libraries)
		{
			auto DxilLibrarySubobject = Desc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
			DxilLibrarySubobject->SetDXILLibrary(&Library);
			for (const auto& Symbol : Symbols)
			{
				DxilLibrarySubobject->DefineExport(Symbol.data());
			}
		}

		// Add all the hit group declarations
		for (const auto& [HitGroupName, AnyHit, ClosestHit, Intersection] : HitGroups)
		{
			auto HitGroupSubobject = Desc.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
			HitGroupSubobject->SetHitGroupExport(HitGroupName.data());
			HitGroupSubobject->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

			HitGroupSubobject->SetAnyHitShaderImport(!AnyHit.empty() ? AnyHit.data() : nullptr);
			HitGroupSubobject->SetClosestHitShaderImport(!ClosestHit.empty() ? ClosestHit.data() : nullptr);
			HitGroupSubobject->SetIntersectionShaderImport(!Intersection.empty() ? Intersection.data() : nullptr);
		}

		// Add 2 subobjects for every root signature association
		for (const auto& [RootSignature, Symbols] : RootSignatureAssociations)
		{
			// The root signature association requires two objects for each: one to declare the root
			// signature, and another to associate that root signature to a set of symbols

			// Add a subobject to declare the local root signature
			auto LocalRootSignatureSubobject = Desc.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
			LocalRootSignatureSubobject->SetRootSignature(RootSignature);

			// Add a subobject for the association between the exported shader symbols and the local root signature
			auto SubobjectToExportsAssociationSubobject = Desc.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
			for (const auto& Symbol : Symbols)
			{
				SubobjectToExportsAssociationSubobject->AddExport(Symbol.data());
			}
			SubobjectToExportsAssociationSubobject->SetSubobjectToAssociate(*LocalRootSignatureSubobject);
		}

		// Add a subobject for global root signature
		auto GlobalRootSignatureSubobject = Desc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
		GlobalRootSignatureSubobject->SetRootSignature(GlobalRootSignature);

		// Add a subobject for the raytracing shader config
		auto RaytracingShaderConfigSubobject = Desc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
		RaytracingShaderConfigSubobject->Config(ShaderConfig.MaxPayloadSizeInBytes, ShaderConfig.MaxAttributeSizeInBytes);

		// Add a subobject for the association between shaders and the payload
		std::vector<std::wstring_view> ExportedSymbols						  = BuildShaderExportList();
		auto						   SubobjectToExportsAssociationSubobject = Desc.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
		for (const auto& Symbol : ExportedSymbols)
		{
			SubobjectToExportsAssociationSubobject->AddExport(Symbol.data());
		}
		SubobjectToExportsAssociationSubobject->SetSubobjectToAssociate(*RaytracingShaderConfigSubobject);

		// Add a subobject for the raytracing pipeline configuration
		auto RaytracingPipelineConfigSubobject = Desc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
		RaytracingPipelineConfigSubobject->Config(PipelineConfig.MaxTraceRecursionDepth);

		return Desc;
	}

	std::vector<std::wstring_view> RaytracingPipelineStateDesc::BuildShaderExportList() const
	{
		// Get all names from libraries
		// Get names associated to hit groups
		// Return list of libraries + hit group names - shaders in hit groups
		std::unordered_set<std::wstring_view> Exports;

		// Add all the symbols exported by the libraries
		for (const auto& Library : Libraries)
		{
			for (const auto& Symbol : Library.Symbols)
			{
#ifdef _DEBUG
				// Sanity check in debug mode: check that no name is exported more than once
				if (Exports.contains(Symbol))
				{
					throw std::logic_error("Multiple definition of a symbol in the imported DXIL libraries");
				}
#endif
				Exports.insert(Symbol);
			}
		}

#ifdef _DEBUG
		// Sanity check in debug mode: verify that the hit groups do not reference an unknown shader name
		{
			std::unordered_set<std::wstring_view> AllExports = Exports;

			for (const auto& HitGroup : HitGroups)
			{
				if (!HitGroup.AnyHitSymbol.empty() && !Exports.contains(HitGroup.AnyHitSymbol))
				{
					throw std::logic_error("Any hit symbol not found in the imported DXIL libraries");
				}

				if (!HitGroup.ClosestHitSymbol.empty() && !Exports.contains(HitGroup.ClosestHitSymbol))
				{
					throw std::logic_error("Closest hit symbol not found in the imported DXIL libraries");
				}

				if (!HitGroup.IntersectionSymbol.empty() && !Exports.contains(HitGroup.IntersectionSymbol))
				{
					throw std::logic_error("Intersection symbol not found in the imported DXIL libraries");
				}

				AllExports.insert(HitGroup.HitGroupName);
			}

			// Sanity check in debug mode: verify that the root signature associations do not reference an
			// unknown shader or hit group name
			for (const auto& RootSignatureAssociation : RootSignatureAssociations)
			{
				for (const auto& Symbol : RootSignatureAssociation.Symbols)
				{
					if (!Symbol.empty() && !AllExports.contains(Symbol))
					{
						throw std::logic_error("Root association symbol not found in the imported DXIL libraries and hit group names");
					}
				}
			}
		}
#endif

		// Go through all hit groups and remove the symbols corresponding to intersection, any hit and
		// closest hit shaders from the symbol set
		for (const auto& HitGroup : HitGroups)
		{
			if (!HitGroup.ClosestHitSymbol.empty())
			{
				Exports.erase(HitGroup.ClosestHitSymbol);
			}

			if (!HitGroup.AnyHitSymbol.empty())
			{
				Exports.erase(HitGroup.AnyHitSymbol);
			}

			if (!HitGroup.IntersectionSymbol.empty())
			{
				Exports.erase(HitGroup.IntersectionSymbol);
			}

			Exports.insert(HitGroup.HitGroupName);
		}

		return { Exports.begin(), Exports.end() };
	}

	D3D12RaytracingPipelineState::D3D12RaytracingPipelineState(
		D3D12Device*				 Parent,
		RaytracingPipelineStateDesc& Desc)
		: D3D12DeviceChild(Parent)
	{
		D3D12_STATE_OBJECT_DESC ApiDesc = Desc.Build();
		VERIFY_D3D12_API(Parent->GetD3D12Device5()->CreateStateObject(&ApiDesc, IID_PPV_ARGS(&StateObject)));
		VERIFY_D3D12_API(StateObject->QueryInterface(IID_PPV_ARGS(&StateObjectProperties)));
	}

	void* D3D12RaytracingPipelineState::GetShaderIdentifier(std::wstring_view ExportName) const
	{
		void* ShaderIdentifier = StateObjectProperties->GetShaderIdentifier(ExportName.data());
		assert(ShaderIdentifier && "Invalid ExportName");
		return ShaderIdentifier;
	}
} // namespace RHI
