#include "AssetManager.h"

using namespace DirectX;

namespace Asset
{
	AssetManager::AssetManager(RHI::D3D12Device* Device)
	{
		Thread = std::jthread(
			[=]()
			{
				RHI::D3D12LinkedDevice* LinkedDevice = Device->GetLinkedDevice();

				while (true)
				{
					std::unique_lock Lock(Mutex);
					ConditionVariable.wait(Lock);

					if (Quit)
					{
						break;
					}

					std::vector<Mesh*>	  Meshes;
					std::vector<Texture*> Textures;

					LinkedDevice->BeginResourceUpload();

					// Process Mesh
					while (!MeshUploadQueue.empty())
					{
						Mesh* Mesh = MeshUploadQueue.front();
						MeshUploadQueue.pop();
						UploadMesh(Mesh, LinkedDevice);
						Meshes.push_back(Mesh);
					}

					// Process Texture
					while (!TextureUploadQueue.empty())
					{
						Texture* Texture = TextureUploadQueue.front();
						TextureUploadQueue.pop();
						UploadTexture(Texture, LinkedDevice);
						Textures.push_back(Texture);
					}

					LinkedDevice->EndResourceUpload(true);

					for (auto Mesh : Meshes)
					{
						// Release memory
						Mesh->Release();

						Mesh->Handle.State = true;
						MeshRegistry.UpdateHandleState(Mesh->Handle);
					}
					for (auto Texture : Textures)
					{
						// Release memory
						Texture->Release();

						Texture->Handle.State = true;
						TextureRegistry.UpdateHandleState(Texture->Handle);
					}
				}
			});
	}

	AssetManager::~AssetManager()
	{
		Quit = true;
		ConditionVariable.notify_all();
	}

	AssetType AssetManager::GetAssetTypeFromExtension(const std::filesystem::path& Path)
	{
		if (MeshImporter.SupportsExtension(Path))
		{
			return AssetType::Mesh;
		}
		if (TextureImporter.SupportsExtension(Path))
		{
			return AssetType::Texture;
		}

		return AssetType::Unknown;
	}

	void AssetManager::UploadTexture(Texture* AssetTexture, RHI::D3D12LinkedDevice* Device)
	{
		const auto& Metadata = AssetTexture->TexImage.GetMetadata();

		DXGI_FORMAT Format = Metadata.format;
		if (AssetTexture->Options.sRGB)
		{
			Format = DirectX::MakeSRGB(Format);
		}

		D3D12_RESOURCE_DESC ResourceDesc = {};
		switch (Metadata.dimension)
		{
		case TEX_DIMENSION_TEXTURE1D:
			ResourceDesc = CD3DX12_RESOURCE_DESC::Tex1D(
				Format,
				static_cast<UINT64>(Metadata.width),
				static_cast<UINT16>(Metadata.arraySize));
			break;

		case TEX_DIMENSION_TEXTURE2D:
			ResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				Format,
				static_cast<UINT64>(Metadata.width),
				static_cast<UINT>(Metadata.height),
				static_cast<UINT16>(Metadata.arraySize),
				static_cast<UINT16>(Metadata.mipLevels));
			break;

		case TEX_DIMENSION_TEXTURE3D:
			ResourceDesc = CD3DX12_RESOURCE_DESC::Tex3D(
				Format,
				static_cast<UINT64>(Metadata.width),
				static_cast<UINT>(Metadata.height),
				static_cast<UINT16>(Metadata.depth));
			break;
		}

		AssetTexture->DxTexture = RHI::D3D12Texture(Device, ResourceDesc, std::nullopt, AssetTexture->IsCubemap);
		AssetTexture->Srv		= RHI::D3D12ShaderResourceView(Device, &AssetTexture->DxTexture, false, std::nullopt, std::nullopt);

		std::vector<D3D12_SUBRESOURCE_DATA> Subresources(AssetTexture->TexImage.GetImageCount());
		const auto							pImages = AssetTexture->TexImage.GetImages();
		for (size_t i = 0; i < AssetTexture->TexImage.GetImageCount(); ++i)
		{
			Subresources[i].RowPitch   = pImages[i].rowPitch;
			Subresources[i].SlicePitch = pImages[i].slicePitch;
			Subresources[i].pData	   = pImages[i].pixels;
		}

		Device->Upload(Subresources, AssetTexture->DxTexture.GetResource());
	}

	void AssetManager::UploadMesh(Mesh* AssetMesh, RHI::D3D12LinkedDevice* Device)
	{
		UINT64 VertexBufferSizeInBytes = AssetMesh->Vertices.size() * sizeof(Vertex);
		UINT64 IndexBufferSizeInBytes  = AssetMesh->Indices.size() * sizeof(uint32_t);

		AssetMesh->VertexResource = RHI::D3D12Buffer(Device, VertexBufferSizeInBytes, sizeof(Vertex), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_NONE);
		AssetMesh->IndexResource  = RHI::D3D12Buffer(Device, IndexBufferSizeInBytes, sizeof(uint32_t), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_NONE);

		Device->Upload(AssetMesh->Vertices.data(), VertexBufferSizeInBytes, AssetMesh->VertexResource.GetResource());
		Device->Upload(AssetMesh->Indices.data(), IndexBufferSizeInBytes, AssetMesh->IndexResource.GetResource());

		if (AssetMesh->Options.GenerateMeshlets)
		{
			UINT64 MeshletBufferSizeInBytes			  = AssetMesh->Meshlets.size() * sizeof(Meshlet);
			UINT64 UniqueVertexIndexBufferSizeInBytes = AssetMesh->UniqueVertexIndices.size() * sizeof(uint8_t);
			UINT64 PrimitiveIndexBufferSizeInBytes	  = AssetMesh->PrimitiveIndices.size() * sizeof(MeshletTriangle);

			AssetMesh->MeshletResource			 = RHI::D3D12Buffer(Device, MeshletBufferSizeInBytes, sizeof(Meshlet), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_NONE);
			AssetMesh->UniqueVertexIndexResource = RHI::D3D12Buffer(Device, UniqueVertexIndexBufferSizeInBytes, sizeof(uint8_t), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_NONE);
			AssetMesh->PrimitiveIndexResource	 = RHI::D3D12Buffer(Device, PrimitiveIndexBufferSizeInBytes, sizeof(MeshletTriangle), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_NONE);

			Device->Upload(AssetMesh->Meshlets.data(), MeshletBufferSizeInBytes, AssetMesh->MeshletResource.GetResource());
			Device->Upload(AssetMesh->UniqueVertexIndices.data(), UniqueVertexIndexBufferSizeInBytes, AssetMesh->UniqueVertexIndexResource.GetResource());
			Device->Upload(AssetMesh->PrimitiveIndices.data(), PrimitiveIndexBufferSizeInBytes, AssetMesh->PrimitiveIndexResource.GetResource());
		}

		D3D12_GPU_VIRTUAL_ADDRESS IndexAddress	= AssetMesh->IndexResource.GetGpuVirtualAddress();
		D3D12_GPU_VIRTUAL_ADDRESS VertexAddress = AssetMesh->VertexResource.GetGpuVirtualAddress();

		D3D12_RAYTRACING_GEOMETRY_DESC RaytracingGeometryDesc		= {};
		RaytracingGeometryDesc.Type									= D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		RaytracingGeometryDesc.Flags								= D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
		RaytracingGeometryDesc.Triangles.Transform3x4				= NULL;
		RaytracingGeometryDesc.Triangles.IndexFormat				= DXGI_FORMAT_R32_UINT;
		RaytracingGeometryDesc.Triangles.VertexFormat				= DXGI_FORMAT_R32G32B32_FLOAT;
		RaytracingGeometryDesc.Triangles.IndexCount					= static_cast<UINT>(AssetMesh->Indices.size());
		RaytracingGeometryDesc.Triangles.VertexCount				= static_cast<UINT>(AssetMesh->Vertices.size());
		RaytracingGeometryDesc.Triangles.IndexBuffer				= IndexAddress;
		RaytracingGeometryDesc.Triangles.VertexBuffer.StartAddress	= VertexAddress;
		RaytracingGeometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);

		AssetMesh->Blas.AddGeometry(RaytracingGeometryDesc);

		AssetMesh->VertexView = RHI::D3D12ShaderResourceView(Device, &AssetMesh->VertexResource, true, 0, VertexBufferSizeInBytes);
		AssetMesh->IndexView  = RHI::D3D12ShaderResourceView(Device, &AssetMesh->IndexResource, true, 0, IndexBufferSizeInBytes);
	}

	void AssetManager::RequestUpload(Texture* Texture)
	{
		//D3D12LinkedDevice* Device = RenderCore::Device->GetLinkedDevice();
		//Device->BeginResourceUpload();
		//UploadTexture(Texture, Device);
		//Device->EndResourceUpload(true);
		//// Release memory
		//Texture->Release();

		//Texture->Handle.State = true;
		//TextureCache.UpdateHandleState(Texture->Handle);

		std::scoped_lock Lock(Mutex);
		TextureUploadQueue.push(Texture);
		ConditionVariable.notify_all();
	}

	void AssetManager::RequestUpload(Mesh* Mesh)
	{
		//D3D12LinkedDevice* Device = RenderCore::Device->GetLinkedDevice();
		//Device->BeginResourceUpload();
		//UploadMesh(Mesh, Device);
		//Device->EndResourceUpload(true);
		//// Release memory
		//Mesh->Release();

		//Mesh->Handle.State = true;
		//MeshCache.UpdateHandleState(Mesh->Handle);

		std::scoped_lock Lock(Mutex);
		MeshUploadQueue.push(Mesh);
		ConditionVariable.notify_all();
	}
} // namespace Asset
