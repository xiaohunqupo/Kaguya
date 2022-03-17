#pragma once
#include "PathIntegratorDXR1_0.h"

class PathIntegratorDXR1_1 final : public Renderer
{
public:
	PathIntegratorDXR1_1(
		RHI::D3D12Device*	 Device,
		RHI::D3D12SwapChain* SwapChain,
		ShaderCompiler*		 Compiler,
		Window*				 MainWindow);

private:
	void RenderOptions() override;
	void Render(World* World, WorldRenderView* WorldRenderView, RHI::D3D12CommandContext& Context) override;

private:
	RaytracingAccelerationStructure AccelerationStructure;

	// Temporal accumulation
	UINT NumTemporalSamples = 0;
	UINT FrameCounter		= 0;

	PathIntegratorState PathIntegratorState;
};
