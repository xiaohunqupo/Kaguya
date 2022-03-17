#pragma once
#include "RHI/RHI.h"
#include "RHI/RenderGraph/RenderGraph.h"
#include "Core/World/World.h"
#include "Core/Window.h"
#include "View.h"

#include "UI/WorldWindow.h"
#include "UI/InspectorWindow.h"
#include "UI/AssetWindow.h"

#include "Globals.h"

class RendererPresent : public RHI::IPresent
{
public:
	RendererPresent(RHI::D3D12CommandContext& Context) noexcept
		: Context(Context)
	{
	}

	void PrePresent() override
	{
		SyncHandle = Context.Execute(false);
	}

	void PostPresent() override
	{
		SyncHandle.WaitForCompletion();
	}

	RHI::D3D12CommandContext& Context;
	RHI::D3D12SyncHandle	  SyncHandle;
};

class Renderer
{
public:
	Renderer(
		RHI::D3D12Device*	 Device,
		RHI::D3D12SwapChain* SwapChain,
		ShaderCompiler*		 Compiler,
		Window*				 MainWindow);
	virtual ~Renderer();

	void OnRenderOptions();

	void OnRender(World* World, WorldRenderView* WorldRenderView);

	void OnResize(uint32_t Width, uint32_t Height);

	void OnMove(std::int32_t x, std::int32_t y);

protected:
	virtual void RenderOptions()																		   = 0;
	virtual void Render(World* World, WorldRenderView* WorldRenderView, RHI::D3D12CommandContext& Context) = 0;

protected:
	RHI::D3D12Device*	 Device		= nullptr;
	RHI::D3D12SwapChain* SwapChain	= nullptr;
	ShaderCompiler*		 Compiler	= nullptr;
	Window*				 MainWindow = nullptr;

	RHI::RenderGraphAllocator Allocator;
	RHI::RenderGraphRegistry  Registry;

	View View;

	size_t FrameIndex = 0;

	void* Viewport = nullptr;

	WorldWindow		WorldWindow;
	InspectorWindow InspectorWindow;
	AssetWindow		AssetWindow;
};
