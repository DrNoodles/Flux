
#include "SceneManager.h"
#include "Entity/RenderableComponent.h"

#include <vector>
#include <unordered_map>
#include <iostream>

RenderableComponent SceneManager::LoadRenderableComponentFromFile(const std::string& path) const
{
	auto modelDefinition = _modelLoaderService.LoadModel(path);
	if (!modelDefinition.has_value())
	{
		throw std::invalid_argument("Couldn't load model");
	}

	RenderableComponent rc = {};

	
	for (const auto& meshDef : modelDefinition->Meshes)
	{
		RenderableCreateInfo renderableCreateInfo = {};

		// Create the Mesh resource
		renderableCreateInfo.Mesh = _renderer.CreateMeshResource(meshDef);


		// Create Texture resources and config Material
		auto& mat = renderableCreateInfo.Mat;
		for (const auto& texDef : meshDef.Textures)
		{
			const auto texResId = _renderer.CreateTextureResource(texDef.Path);

			switch (texDef.Type)
			{
			case TextureType::Basecolor:
				mat.UseBasecolorMap = true;
				mat.BasecolorMap = texResId;
				break;

			case TextureType::Normals:
				mat.UseNormalMap = true;
				mat.NormalMap = texResId;
				break;

			case TextureType::Roughness:
				mat.UseRoughnessMap = true;
				mat.RoughnessMap = texResId;
				break;

			case TextureType::Metalness:
				mat.UseMetalnessMap = true;
				mat.MetalnessMap = texResId;
				break;

			case TextureType::AmbientOcclusion:
				mat.UseAoMap = false;
				mat.AoMap = texResId;
				break;

			case TextureType::Undefined:
			default:
				std::cerr << "Discarding unknown texture type" << std::endl;
			}
		}

		rc.RenderableId = _renderer.CreateRenderable(renderableCreateInfo);
		
		break; // TODO Only processing the first mesh for now!
	}

	return rc;
}


//u32 ResourceManager::LoadTexture(const std::string& path)
//{
//	// Is teh tex already loaded?
//	const auto it = _loadedTextures.find(path);
//	if (it != _loadedTextures.end())
//	{
//		return it->second;
//	}
//
//	// TODO safety check this actually exists :)
//	const size_t idx = path.find_last_of("/\\");
//	const auto directory = path.substr(0, idx + 1);
//	const auto filename = path.substr(idx + 1);
//
//	auto texRes = TextureLoader::LoadTextureFromFileAndPushToGpu(filename, directory, false);
//	auto id = StoreTextureResource(std::make_unique<TextureResource>(std::move(texRes)));
//
//	_loadedTextures.insert(std::make_pair(path, id));
//
//	return id;
//}


//ModelResourceId ResourceManager::StoreModelResource(std::unique_ptr<ModelResource> model)
//{
//	_modelResources.emplace_back(std::move(model));
//	return ModelResourceId(_modelResources.size() - 1);
//}
//MeshResourceId ResourceManager::StoreMeshResource(std::unique_ptr<MeshResource> mesh)
//{
//	_meshResources.emplace_back(std::move(mesh));
//	return MeshResourceId(_meshResources.size() - 1);
//}
//TextureResourceId ResourceManager::StoreTextureResource(std::unique_ptr<TextureResource> texture)
//{
//	_textureResources.emplace_back(std::move(texture));
//	return TextureResourceId(_textureResources.size() - 1);
//}
