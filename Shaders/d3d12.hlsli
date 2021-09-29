﻿#pragma once

// typedef uint2 D3D12_GPU_VIRTUAL_ADDRESS;
typedef uint64_t D3D12_GPU_VIRTUAL_ADDRESS;

struct D3D12_DRAW_ARGUMENTS
{
	uint VertexCountPerInstance;
	uint InstanceCount;
	uint StartVertexLocation;
	uint StartInstanceLocation;
};

struct D3D12_DRAW_INDEXED_ARGUMENTS
{
	uint IndexCountPerInstance;
	uint InstanceCount;
	uint StartIndexLocation;
	int	 BaseVertexLocation;
	uint StartInstanceLocation;
};

struct D3D12_DISPATCH_ARGUMENTS
{
	uint ThreadGroupCountX;
	uint ThreadGroupCountY;
	uint ThreadGroupCountZ;
};

// Raytracing
struct D3D12_RAYTRACING_INSTANCE_DESC
{
	float3x4				  Transform;
	uint					  Instance_ID_Mask;
	uint					  Instance_ContributionToHitGroupIndex_Flags;
	D3D12_GPU_VIRTUAL_ADDRESS AccelerationStructure;

	uint GetID() { return Instance_ID_Mask & 0xFFFFFF00; }
	uint GetMask() { return Instance_ID_Mask & 0x000000FF; }

	uint GetContributionToHitGroupIndex() { return Instance_ContributionToHitGroupIndex_Flags & 0xFFFFFF00; }
	uint GetFlags() { return Instance_ContributionToHitGroupIndex_Flags & 0x000000FF; }
};
