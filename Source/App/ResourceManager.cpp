
#include "ResourceManager.h"

#include <vector>
#include <memory>
#include <unordered_map>


std::unique_ptr<TextureResource> ResourceManager::LoadTextureResource(const std::string& path) const
{
	auto texture = std::make_unique<TextureResource>();

	// TODO Pull the teture library out of the CreateTextureImage, just work on array of pixels
	std::tie(texture->Image, texture->Memory, texture->MipLevels, texture->Width, texture->Height)
		= _gpuService->CreateTextureImage(path, _commandPool, _graphicsQueue, _physicalDevice, _device);

	texture->View = CreateTextureImageView(texture->Image, texture->MipLevels, _device);

	texture->Sampler = CreateTextureSampler(texture->MipLevels, _device);

	return texture;
}


std::unique_ptr<MeshResource> ResourceManager::LoadMeshResource(const ModelDefinition& modelDefinition) const
{
	assert(!modelDefinition.Meshes.empty());


	// NOTE: Only loading first mesh found in model for now
	// TODO Load all meshes
	auto meshDef = modelDefinition.Meshes[0];

	auto meshResources = std::make_unique<MeshResource>();

	// Compute AABB
	std::vector<glm::vec3> positions{ meshDef.Vertices.size() };
	for (size_t i = 0; i < meshDef.Vertices.size(); i++)
	{
		positions[i] = meshDef.Vertices[i].Pos;
	}
	const AABB bounds{ positions };


	// Load mesh resource
	meshResources->IndexCount = meshDef.Indices.size();
	meshResources->VertexCount = meshDef.Vertices.size();
	meshResources->Bounds = bounds;

	std::tie(meshResources->VertexBuffer, meshResources->VertexBufferMemory)
		= CreateVertexBuffer(meshDef.Vertices, _graphicsQueue, _commandPool, _physicalDevice, _device);

	std::tie(meshResources->IndexBuffer, meshResources->IndexBufferMemory)
		= CreateIndexBuffer(meshDef.Indices, _graphicsQueue, _commandPool, _physicalDevice, _device);

	return meshResources;
}


std::vector<ModelInfoResource> ResourceManager::CreateModelInfoResources(const Model& model) const
{
	std::vector<ModelInfoResource> modelInfos{};

	const auto numThingsToMake = _swapchainImages.size();

	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<VkDescriptorSet> descriptorSets;

	std::tie(uniformBuffers, uniformBuffersMemory) = CreateUniformBuffers(numThingsToMake, _device, _physicalDevice);

	descriptorSets = CreateDescriptorSets((uint32_t)numThingsToMake, _descriptorSetLayout, _descriptorPool,
		uniformBuffers, *model.BasecolorMap, *model.NormalMap, _device);


	modelInfos.resize(numThingsToMake);
	for (auto i = 0; i < numThingsToMake; i++)
	{
		modelInfos[i].UniformBuffer = uniformBuffers[i];
		modelInfos[i].UniformBufferMemory = uniformBuffersMemory[i];
		modelInfos[i].DescriptorSet = descriptorSets[i];
	}

	return modelInfos;
}




RenderableComponent ResourceManager::LoadRenderableFromFile(const std::string& path)
{
	std::vector<RenderableMesh> renderableMeshes{};

	auto modelDef = _modelLoaderService->LoadModel(path);
	if (!modelDef.has_value())
	{
		throw std::invalid_argument("Couldn't load model");
	}
	
	for (const auto& meshDef : modelDef->Meshes)
	{
		RenderableMesh asset{};
		asset.MeshId = StoreMesh(std::make_unique<MeshResource>(meshDef.Name, meshDef.Vertices, meshDef.Indices));

		for (const auto& texDef : meshDef.Textures)
		{

			
			// Load texture resource to GPU
			Texture texRes{};
			texRes.ResourceId = LoadTexture(texDef.Path);
			texRes.Type = texDef.Type;
			texRes.Path = texDef.Path;
			
			
			switch (texDef.Type)
			{
			case TextureType::BaseColor: 
				asset.Mat.UseAlbedoMap = true;
				asset.Mat.AlbedoMap = texRes;
				break;
				
			case TextureType::Normals:
				//asset.Mat.UseNormalMap = true;
				asset.Mat.NormalMap = texRes;
				break;

			case TextureType::AmbientOcclusion:
				//asset.Mat.UseAoMap = true;
				asset.Mat.AoMap = texRes;
				break;

			case TextureType::Roughness:
				asset.Mat.UseRoughnessMap = true;
				asset.Mat.RoughnessMap = texRes;
				break;

			case TextureType::Metalness:
				asset.Mat.UseMetallicMap = true;
				asset.Mat.MetalnessMap = texRes;
				break;

			case TextureType::Undefined:
			default:
				std::cerr << "Discarding unknown texture type" << std::endl;
			}
		}
	
		renderableMeshes.emplace_back(asset);
	}

	return Renderable{ renderableMeshes };
}

std::tuple<u32, u32, u32, u32, u32> 
ResourceManager::LoadEnvironmentMap(const std::string& path, const std::string& shadersDir)
{
	auto meshId = StoreMesh(std::make_unique<CubeMesh>());
	
	auto [envMap, irradianceMap, prefilterMap, brdfLUT] =
		TextureLoader::LoadEnvironmentMap(path, shadersDir);
	
	auto e = StoreTexture(std::make_unique<TextureResource>(std::move(envMap)));
	auto i = StoreTexture(std::make_unique<TextureResource>(std::move(irradianceMap)));
	auto p = StoreTexture(std::make_unique<TextureResource>(std::move(prefilterMap)));
	auto b = StoreTexture(std::make_unique<TextureResource>(std::move(brdfLUT)));
	return std::make_tuple(meshId, e, i, p, b);
}

u32 ResourceManager::LoadTexture(const std::string& path)
{
	// Is teh tex already loaded?
	const auto it = _loadedTextures.find(path);
	if (it != _loadedTextures.end())
	{
		return it->second;
	}

	// TODO safety check this actually exists :)
	const size_t idx = path.find_last_of("/\\");
	const auto directory = path.substr(0, idx + 1);
	const auto filename = path.substr(idx + 1);

	auto texRes = TextureLoader::LoadTextureFromFileAndPushToGpu(filename, directory, false);
	auto id = StoreTexture(std::make_unique<TextureResource>(std::move(texRes)));

	_loadedTextures.insert(std::make_pair(path, id));

	return id;
}

u32 ResourceManager::StoreMesh(std::unique_ptr<IMeshResource> mesh)
{
	_meshResources.emplace_back(std::move(mesh));
	return u32(_meshResources.size() - 1);
}

u32 ResourceManager::StoreShader(const std::string& vert, const std::string& frag)
{
	Shaders.emplace_back(std::make_unique<ShaderResource>(vert.c_str(), frag.c_str()));
	return u32(Shaders.size() - 1);
}

u32 ResourceManager::StoreTexture(std::unique_ptr<TextureResource> t)
{
	_textureResources.emplace_back(std::move(t));
	return u32(_textureResources.size() - 1);
}
