#pragma once
#include <Framework/CommonRenderer.h>
#include <Framework/IModelLoaderService.h>

#include "TextureResource.h"
#include "VulkanService.h"

// The purpose of this class is to create/manage/destroy GPU textures and buffers resources.
class ResourceRegistry
{
public: // Data
private:// Data

	// Dependencies
	VulkanService* _vk;

	std::vector<std::unique_ptr<MeshResource>> _meshes{};
	std::vector<std::unique_ptr<TextureResource>> _textures{};

public: // Lifetime
	ResourceRegistry() = delete;
	explicit ResourceRegistry(VulkanService* vk)
		: _vk(vk)
	{
	}

	ResourceRegistry(const ResourceRegistry&) = delete;
	ResourceRegistry(ResourceRegistry&&) = delete;
	ResourceRegistry& operator=(const ResourceRegistry&) = delete;
	ResourceRegistry& operator=(ResourceRegistry&&) = delete;
	~ResourceRegistry()
	{
		// TODO Make all resources RAII
		for (auto& mesh : _meshes)  
		{
			vkDestroyBuffer(_vk->LogicalDevice(), mesh->IndexBuffer, nullptr);
			vkFreeMemory(_vk->LogicalDevice(), mesh->IndexBufferMemory, nullptr);
			vkDestroyBuffer(_vk->LogicalDevice(), mesh->VertexBuffer, nullptr);
			vkFreeMemory(_vk->LogicalDevice(), mesh->VertexBufferMemory, nullptr);
		}
		
		_textures.clear(); // RAII will cleanup
	}

public: // Methods
	TextureResourceId CreateTextureResource(const std::string& path)
	{
		const auto id = TextureResourceId(static_cast<u32>(_textures.size()));
		auto texRes = TextureResourceHelpers::LoadTexture(path, _vk->CommandPool(), _vk->GraphicsQueue(), _vk->PhysicalDevice(), _vk->LogicalDevice());
		_textures.emplace_back(std::make_unique<TextureResource>(std::move(texRes)));
		return id;
	}

	MeshResourceId CreateMeshResource(const MeshDefinition& meshDefinition)
	{
		// Load mesh resource
		auto mesh = std::make_unique<MeshResource>();

		mesh->IndexCount = meshDefinition.Indices.size();
		mesh->VertexCount = meshDefinition.Vertices.size();
		//mesh->Bounds = meshDefinition.Bounds;

		std::tie(mesh->VertexBuffer, mesh->VertexBufferMemory)
			= vkh::CreateVertexBuffer(meshDefinition.Vertices, _vk->GraphicsQueue(), _vk->CommandPool(), _vk->PhysicalDevice(), _vk->LogicalDevice());

		std::tie(mesh->IndexBuffer, mesh->IndexBufferMemory)
			= vkh::CreateIndexBuffer(meshDefinition.Indices, _vk->GraphicsQueue(), _vk->CommandPool(), _vk->PhysicalDevice(), _vk->LogicalDevice());


		const auto id = MeshResourceId(static_cast<u32>(_meshes.size()));
		_meshes.emplace_back(std::move(mesh));

		return id;
	}

	const TextureResource& GetTexture(TextureResourceId id) const
	{
		return *_textures[id.Value()];
	}

	const MeshResource& GetMesh(MeshResourceId id) const
	{
		return *_meshes[id.Value()];
	}

	const std::vector<std::unique_ptr<MeshResource>>& Hack_GetMeshes() const { return _meshes; }

private:// Methods
};