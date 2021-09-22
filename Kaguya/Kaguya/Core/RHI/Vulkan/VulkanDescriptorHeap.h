#pragma once
#include "VulkanCommon.h"

struct ResourceDescriptorHeapDesc
{
	UINT NumTextureDescriptors;
	UINT NumRWTextureDescriptors;
};

class VulkanResourceDescriptorHeap : public VulkanDeviceChild
{
public:
	VulkanResourceDescriptorHeap() noexcept = default;
	explicit VulkanResourceDescriptorHeap(VulkanDevice* Parent, const ResourceDescriptorHeapDesc& Desc);

	void Destroy();

	void Allocate(UINT PoolIndex, UINT& Index);

	void Release(UINT PoolIndex, UINT Index);

	VkDescriptorSetLayout  DescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool	   DescriptorPool	   = VK_NULL_HANDLE;
	VkDescriptorSet		   DescriptorSet	   = VK_NULL_HANDLE;
	std::vector<DescriptorIndexPool> IndexPoolArray;
};

class VulkanSamplerDescriptorHeap : public VulkanDeviceChild
{
public:
	VulkanSamplerDescriptorHeap() noexcept = default;
	explicit VulkanSamplerDescriptorHeap(VulkanDevice* Parent, UINT NumSamplers);

	void Destroy();

	void Allocate(UINT& Index);

	void Release(UINT Index);

	VkDescriptorSetLayout DescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool	  DescriptorPool	  = VK_NULL_HANDLE;
	VkDescriptorSet		  DescriptorSet		  = VK_NULL_HANDLE;
	DescriptorIndexPool			  IndexPool;

	std::unordered_map<UINT64, VkSampler> SamplerTable;
};
