#include "DeferredRenderer.h"
#include <imgui.h>
#include "RHI/HlslResourceHandle.h"

using namespace RHI;

DeferredRenderer::DeferredRenderer(
	RHI::D3D12Device* Device,
	ShaderCompiler*	  Compiler)
	: Renderer(Device, Compiler)
{
	IndirectCullCS = Compiler->CompileCS(L"Shaders/IndirectCull.hlsl", ShaderCompileOptions(L"CSMain"));
	GBufferVS	   = Compiler->CompileVS(L"Shaders/GBuffer.hlsl", ShaderCompileOptions(L"VSMain"));
	GBufferPS	   = Compiler->CompilePS(L"Shaders/GBuffer.hlsl", ShaderCompileOptions(L"PSMain"));
	ShadingCS	   = Compiler->CompileCS(L"Shaders/Shading.hlsl", ShaderCompileOptions(L"CSMain"));

	IndirectCullRS = Device->CreateRootSignature(
		RootSignatureDesc()
			.AddConstantBufferView(0, 0)
			.AddShaderResourceView(0, 0)
			.AddUnorderedAccessViewWithCounter(0, 0));
	GBufferRS = Device->CreateRootSignature(
		RootSignatureDesc()
			.Add32BitConstants(0, 0, 1)
			.AddConstantBufferView(1, 0)
			.AddShaderResourceView(0, 0)
			.AddShaderResourceView(1, 0)
			.AllowInputLayout());
	ShadingRS = Device->CreateRootSignature(
		RootSignatureDesc()
			.AddConstantBufferView(0, 0)
			.AddShaderResourceView(0, 0));

	{
		struct PsoStream
		{
			PipelineStateStreamRootSignature RootSignature;
			PipelineStateStreamCS			 CS;
		} Stream;
		Stream.RootSignature = &IndirectCullRS;
		Stream.CS			 = &IndirectCullCS;
		IndirectCullPSO		 = Device->CreatePipelineState(L"Indirect Cull", Stream);
	}
	{
		RHI::D3D12InputLayout InputLayout(3);
		InputLayout.AddVertexLayoutElement("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0);
		InputLayout.AddVertexLayoutElement("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0);
		InputLayout.AddVertexLayoutElement("NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0);

		RHIDepthStencilState DepthStencilState;
		DepthStencilState.EnableDepthTest = true;

		RHIRenderTargetState RenderTargetState;
		RenderTargetState.RTFormats[0]	   = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		RenderTargetState.RTFormats[1]	   = DXGI_FORMAT_R32G32B32A32_FLOAT;
		RenderTargetState.RTFormats[2]	   = DXGI_FORMAT_R16G16_FLOAT;
		RenderTargetState.RTFormats[3]	   = DXGI_FORMAT_R16G16_FLOAT;
		RenderTargetState.NumRenderTargets = 4;
		RenderTargetState.DSFormat		   = DXGI_FORMAT_D32_FLOAT;

		struct PsoStream
		{
			PipelineStateStreamRootSignature	 RootSignature;
			PipelineStateStreamInputLayout		 InputLayout;
			PipelineStateStreamPrimitiveTopology PrimitiveTopologyType;
			PipelineStateStreamVS				 VS;
			PipelineStateStreamPS				 PS;
			PipelineStateStreamDepthStencilState DepthStencilState;
			PipelineStateStreamRenderTargetState RenderTargetState;
		} Stream;
		Stream.RootSignature		 = &GBufferRS;
		Stream.InputLayout			 = &InputLayout;
		Stream.PrimitiveTopologyType = RHI_PRIMITIVE_TOPOLOGY_TYPE::Triangle;
		Stream.VS					 = &GBufferVS;
		Stream.PS					 = &GBufferPS;
		Stream.DepthStencilState	 = DepthStencilState;
		Stream.RenderTargetState	 = RenderTargetState;

		GBufferPSO = Device->CreatePipelineState(L"GBuffer", Stream);
	}
	{
		struct PsoStream
		{
			PipelineStateStreamRootSignature RootSignature;
			PipelineStateStreamCS			 CS;
		} Stream;
		Stream.RootSignature = &ShadingRS;
		Stream.CS			 = &ShadingCS;
		ShadingPSO			 = Device->CreatePipelineState(L"Shading", Stream);
	}

	CommandSignatureDesc Builder(4, sizeof(CommandSignatureParams));
	Builder.AddConstant(0, 0, 1);
	Builder.AddVertexBufferView(0);
	Builder.AddIndexBufferView();
	Builder.AddDrawIndexed();

	CommandSignature = D3D12CommandSignature(
		Device,
		Builder,
		GBufferRS.GetApiHandle());

	IndirectCommandBuffer = D3D12Buffer(
		Device->GetLinkedDevice(),
		CommandBufferCounterOffset + sizeof(UINT64),
		sizeof(CommandSignatureParams),
		D3D12_HEAP_TYPE_DEFAULT,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	IndirectCommandBufferUav = D3D12UnorderedAccessView(Device->GetLinkedDevice(), &IndirectCommandBuffer, World::MeshLimit, CommandBufferCounterOffset);
}

void DeferredRenderer::RenderOptions()
{
	constexpr const char* View[] = { "Output", "Albedo", "Normal", "Motion", "Depth" };
	ImGui::Combo("GBuffer View", &ViewMode, View, static_cast<int>(std::size(View)));
}

void DeferredRenderer::Render(World* World, WorldRenderView* WorldRenderView, RHI::D3D12CommandContext& Context)
{
	WorldRenderView->Update(World, nullptr);
	if (World->WorldState & EWorldState::EWorldState_Update)
	{
		World->WorldState = EWorldState_Render;
	}

	_declspec(align(256)) struct GlobalConstants
	{
		Hlsl::Camera Camera;

		unsigned int NumMeshes;
		unsigned int NumLights;
	} g_GlobalConstants			= {};
	g_GlobalConstants.Camera	= GetHLSLCameraDesc(*WorldRenderView->Camera);
	g_GlobalConstants.NumMeshes = WorldRenderView->NumMeshes;
	g_GlobalConstants.NumLights = WorldRenderView->NumLights;

	D3D12SyncHandle ComputeSyncHandle;
	if (WorldRenderView->NumMeshes > 0)
	{
		D3D12CommandContext& Copy = Device->GetLinkedDevice()->GetCopyContext();
		Copy.Open();
		{
			Copy.ResetCounter(&IndirectCommandBuffer, CommandBufferCounterOffset);
		}
		Copy.Close();
		D3D12SyncHandle CopySyncHandle = Copy.Execute(false);

		D3D12CommandContext& AsyncCompute = Device->GetLinkedDevice()->GetComputeContext();
		AsyncCompute.GetCommandQueue()->WaitForSyncHandle(CopySyncHandle);

		AsyncCompute.Open();
		{
			D3D12ScopedEvent(AsyncCompute, "Gpu Frustum Culling");
			AsyncCompute.SetPipelineState(&IndirectCullPSO);
			AsyncCompute.SetComputeRootSignature(&IndirectCullRS);

			AsyncCompute.SetComputeConstantBuffer(0, sizeof(GlobalConstants), &g_GlobalConstants);
			AsyncCompute->SetComputeRootShaderResourceView(1, WorldRenderView->Meshes.GetGpuVirtualAddress());
			AsyncCompute->SetComputeRootDescriptorTable(2, IndirectCommandBufferUav.GetGpuHandle());

			AsyncCompute.Dispatch1D<128>(WorldRenderView->NumMeshes);
		}
		AsyncCompute.Close();
		ComputeSyncHandle = AsyncCompute.Execute(false);
	}

	Context.GetCommandQueue()->WaitForSyncHandle(ComputeSyncHandle);

	RenderGraph Graph(Allocator, Registry);

	struct GBufferParameters
	{
		RgResourceHandle Albedo;
		RgResourceHandle Normal;
		RgResourceHandle Material;
		RgResourceHandle Motion;
		RgResourceHandle Depth;

		RgResourceHandle RtvAlbedo;
		RgResourceHandle RtvNormal;
		RgResourceHandle RtvMaterial;
		RgResourceHandle RtvMotion;
		RgResourceHandle Dsv;

		RgResourceHandle SrvAlbedo;
		RgResourceHandle SrvNormal;
		RgResourceHandle SrvMaterial;
		RgResourceHandle SrvMotion;
		RgResourceHandle SrvDepth;
	} GBufferArgs			= {};
	constexpr float Color[] = { 0, 0, 0, 0 };
	GBufferArgs.Albedo		= Graph.Create<D3D12Texture>(RgTextureDesc::Texture2D("Albedo", DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, WorldRenderView->View.Width, WorldRenderView->View.Height, 1, RgClearValue(Color), true, false, false));
	GBufferArgs.Normal		= Graph.Create<D3D12Texture>(RgTextureDesc::Texture2D("Normal", DXGI_FORMAT_R32G32B32A32_FLOAT, WorldRenderView->View.Width, WorldRenderView->View.Height, 1, RgClearValue(Color), true, false, false));
	GBufferArgs.Material	= Graph.Create<D3D12Texture>(RgTextureDesc::Texture2D("Material", DXGI_FORMAT_R16G16_FLOAT, WorldRenderView->View.Width, WorldRenderView->View.Height, 1, RgClearValue(Color), true, false, false));
	GBufferArgs.Motion		= Graph.Create<D3D12Texture>(RgTextureDesc::Texture2D("Motion", DXGI_FORMAT_R16G16_FLOAT, WorldRenderView->View.Width, WorldRenderView->View.Height, 1, RgClearValue(Color), true, false, false));
	GBufferArgs.Depth		= Graph.Create<D3D12Texture>(RgTextureDesc::Texture2D("Depth", DXGI_FORMAT_D32_FLOAT, WorldRenderView->View.Width, WorldRenderView->View.Height, 1, RgClearValue(1.0f, 0xff), false, true, false));

	GBufferArgs.RtvAlbedo	= Graph.Create<D3D12RenderTargetView>(RgViewDesc().SetResource(GBufferArgs.Albedo).AsRtv(true));
	GBufferArgs.RtvNormal	= Graph.Create<D3D12RenderTargetView>(RgViewDesc().SetResource(GBufferArgs.Normal).AsRtv(false));
	GBufferArgs.RtvMaterial = Graph.Create<D3D12RenderTargetView>(RgViewDesc().SetResource(GBufferArgs.Material).AsRtv(false));
	GBufferArgs.RtvMotion	= Graph.Create<D3D12RenderTargetView>(RgViewDesc().SetResource(GBufferArgs.Motion).AsRtv(false));
	GBufferArgs.Dsv			= Graph.Create<D3D12DepthStencilView>(RgViewDesc().SetResource(GBufferArgs.Depth).AsDsv());

	GBufferArgs.SrvAlbedo	= Graph.Create<D3D12ShaderResourceView>(RgViewDesc().SetResource(GBufferArgs.Albedo).AsTextureSrv(true));
	GBufferArgs.SrvNormal	= Graph.Create<D3D12ShaderResourceView>(RgViewDesc().SetResource(GBufferArgs.Normal).AsTextureSrv());
	GBufferArgs.SrvMaterial = Graph.Create<D3D12ShaderResourceView>(RgViewDesc().SetResource(GBufferArgs.Material).AsTextureSrv());
	GBufferArgs.SrvMotion	= Graph.Create<D3D12ShaderResourceView>(RgViewDesc().SetResource(GBufferArgs.Motion).AsTextureSrv());
	GBufferArgs.SrvDepth	= Graph.Create<D3D12ShaderResourceView>(RgViewDesc().SetResource(GBufferArgs.Depth).AsTextureSrv());

	Graph.AddRenderPass("GBuffer")
		.Write({ &GBufferArgs.Albedo, &GBufferArgs.Normal, &GBufferArgs.Material, &GBufferArgs.Motion, &GBufferArgs.Depth })
		.Execute([=, this](RenderGraphRegistry& Registry, D3D12CommandContext& Context)
				 {
					 Context.SetPipelineState(&GBufferPSO);
					 Context.SetGraphicsRootSignature(&GBufferRS);
					 Context.SetGraphicsConstantBuffer(1, sizeof(GlobalConstants), &g_GlobalConstants);
					 Context->SetGraphicsRootShaderResourceView(2, WorldRenderView->Materials.GetGpuVirtualAddress());
					 Context->SetGraphicsRootShaderResourceView(3, WorldRenderView->Meshes.GetGpuVirtualAddress());

					 Context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					 Context.SetViewport(RHIViewport(0.0f, 0.0f, static_cast<float>(WorldRenderView->View.Width), static_cast<float>(WorldRenderView->View.Height), 0.0f, 1.0f));
					 Context.SetScissorRect(RHIRect(0, 0, WorldRenderView->View.Width, WorldRenderView->View.Height));

					 D3D12RenderTargetView* RenderTargetViews[4] = {
						 Registry.Get<D3D12RenderTargetView>(GBufferArgs.RtvAlbedo),
						 Registry.Get<D3D12RenderTargetView>(GBufferArgs.RtvNormal),
						 Registry.Get<D3D12RenderTargetView>(GBufferArgs.RtvMaterial),
						 Registry.Get<D3D12RenderTargetView>(GBufferArgs.RtvMotion)
					 };
					 D3D12DepthStencilView* DepthStencilView = Registry.Get<D3D12DepthStencilView>(GBufferArgs.Dsv);

					 Context.ClearRenderTarget(RenderTargetViews, DepthStencilView);
					 Context.SetRenderTarget(RenderTargetViews, DepthStencilView);

					 Context->ExecuteIndirect(
						 CommandSignature,
						 World::MeshLimit,
						 IndirectCommandBuffer.GetResource(),
						 0,
						 IndirectCommandBuffer.GetResource(),
						 CommandBufferCounterOffset);
				 });

	struct ShadingParameters
	{
		RgResourceHandle Output;

		RgResourceHandle UavOutput;

		RgResourceHandle SrvOutput;
	} ShadingArgs		  = {};
	ShadingArgs.Output	  = Graph.Create<D3D12Texture>(RgTextureDesc::Texture2D("Output", DXGI_FORMAT_R32G32B32A32_FLOAT, WorldRenderView->View.Width, WorldRenderView->View.Height, 1, RHI::RgClearValue(Color), false, false, true));
	ShadingArgs.UavOutput = Graph.Create<D3D12UnorderedAccessView>(RgViewDesc().SetResource(ShadingArgs.Output).AsTextureUav());
	ShadingArgs.SrvOutput = Graph.Create<D3D12ShaderResourceView>(RgViewDesc().SetResource(ShadingArgs.Output).AsTextureSrv());
	Graph.AddRenderPass("Shading")
		.Read({ GBufferArgs.Albedo, GBufferArgs.Normal, GBufferArgs.Material, GBufferArgs.Motion, GBufferArgs.Depth })
		.Write({ &ShadingArgs.Output })
		.Execute([=, this](RenderGraphRegistry& Registry, D3D12CommandContext& Context)
				 {
					 struct Parameters
					 {
						 Hlsl::Camera Camera;
						 Math::Vec2u  Viewport;
						 unsigned int NumLights;

						 HlslTexture2D Albedo;
						 HlslTexture2D Normal;
						 HlslTexture2D Material;
						 HlslTexture2D Depth;

						 HlslRWTexture2D Output;
					 } Args;
					 Args.Viewport	= { WorldRenderView->View.Width, WorldRenderView->View.Height };
					 Args.Camera	= g_GlobalConstants.Camera;
					 Args.NumLights = WorldRenderView->NumLights;

					 Args.Albedo   = Registry.Get<D3D12ShaderResourceView>(GBufferArgs.SrvAlbedo);
					 Args.Normal   = Registry.Get<D3D12ShaderResourceView>(GBufferArgs.SrvNormal);
					 Args.Material = Registry.Get<D3D12ShaderResourceView>(GBufferArgs.SrvMaterial);
					 Args.Depth	   = Registry.Get<D3D12ShaderResourceView>(GBufferArgs.SrvDepth);

					 Args.Output = Registry.Get<D3D12UnorderedAccessView>(ShadingArgs.UavOutput);

					 Context.ClearUnorderedAccessView(Registry.Get<D3D12UnorderedAccessView>(ShadingArgs.UavOutput));

					 Context.SetPipelineState(&ShadingPSO);
					 Context.SetComputeRootSignature(&ShadingRS);
					 Context.SetComputeConstantBuffer(0, Args);
					 Context->SetComputeRootShaderResourceView(1, WorldRenderView->Lights.GetGpuVirtualAddress());
					 Context.Dispatch2D<8, 8>(WorldRenderView->View.Width, WorldRenderView->View.Height);
				 });

	RgResourceHandle Views[] = {
		ShadingArgs.SrvOutput,
		GBufferArgs.SrvAlbedo,
		GBufferArgs.SrvNormal,
		GBufferArgs.SrvMotion,
		GBufferArgs.SrvDepth,
	};

	Graph.GetEpiloguePass()
		.Read({ ShadingArgs.Output, GBufferArgs.Albedo, GBufferArgs.Normal, GBufferArgs.Motion, GBufferArgs.Depth });

	Graph.Execute(Context);
	Viewport = reinterpret_cast<void*>(Registry.Get<D3D12ShaderResourceView>(Views[ViewMode])->GetGpuHandle().ptr);
}
