#pragma once
#include "System/System.h"
#include "RHI/RHI.h"
#include "Core/World/World.h"

#define RAYTRACING_INSTANCEMASK_ALL (0xff)

class RaytracingAccelerationStructure
{
public:
	RaytracingAccelerationStructure() noexcept = default;
	RaytracingAccelerationStructure(RHI::D3D12Device* Device, UINT NumHitGroups);

	RHI::D3D12ShaderResourceView* GetShaderResourceView() { return IsValid() ? &SRV : nullptr; }

	[[nodiscard]] bool IsValid() const noexcept { return !TopLevelAccelerationStructure.empty(); }

	void Reset();

	void AddInstance(const Math::Transform& Transform, StaticMeshComponent* StaticMesh);

	void Build(RHI::D3D12CommandContext& Context);

	// Call this after the command context for Build has been executed, this will
	// update internal BLAS address
	void PostBuild(RHI::D3D12SyncHandle SyncHandle);

	RHI::D3D12Device* Device	   = nullptr;
	UINT			  NumHitGroups = 0;

	RHI::D3D12RaytracingManager Manager;

	RHI::D3D12RaytracingScene				TopLevelAccelerationStructure;
	std::vector<StaticMeshComponent*>		StaticMeshes;
	robin_hood::unordered_set<Asset::Mesh*> ReferencedGeometries;
	UINT									CurrentInstanceID						   = 0;
	UINT									CurrentInstanceContributionToHitGroupIndex = 0;

	RHI::D3D12Buffer   TlasScratch;
	RHI::D3D12ASBuffer TlasResult;
	RHI::D3D12Buffer   InstanceDescsBuffer;

	RHI::D3D12ShaderResourceView SRV;
};
