#include "UiPresenter.h"
#include "PropsView/MaterialViewState.h"
#include "PropsView/PropsView.h"
#include "UiPresenterHelpers.h"

#include <Framework/FileService.h>
#include <Renderer/TextureResource.h> // todo decouple renderer details from this layer
#include <State/Entity/Actions/TurntableActionComponent.h>
#include <State/LibraryManager.h>
#include <State/SceneManager.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

namespace uvh = UiPresenterHelpers;

UiPresenter::UiPresenter(IUiPresenterDelegate& dgate, LibraryManager& library, SceneManager& scene, Renderer& renderer, VulkanService& vulkan, const std::string& shaderDir):
	_delegate(dgate),
	_scene{scene},
	_library{library},
	_renderer{renderer},
	_vulkan{vulkan},
	_sceneView{SceneView{this}},
	_propsView{PropsView{this}},
	_viewportView{ViewportView{this, renderer}}
{

	/*auto screenTexture = TextureResourceHelpers::LoadTexture(shaderDir + "debug.png", 
		_vulkan.CommandPool(), _vulkan.GraphicsQueue(), _vulkan.PhysicalDevice(), _vulkan.LogicalDevice());*/

	// TODO Create a texture for teh offscreen buffers that's RGBA	16

	auto width = _vulkan.SwapchainExtent().width;
	auto height = _vulkan.SwapchainExtent().height;

	/*
	// Offscreen renderpass setup
	{
		auto&& tex = UiPresenterHelpers::CreateScreenTexture(width, height, _vulkan);
		_offscreenTextureResource = std::make_unique<TextureResource>(std::move(tex));

		_offscreenFramebuffer = UiPresenterHelpers::CreateSceneOffscreenFramebuffer(_offscreenTextureResource->DescriptorImageInfo().imageView, _renderer._renderPass, _vulkan);
	}
	
	_postPassResources = uvh::CreatePostPassResources(_offscreenTextureResource->DescriptorImageInfo(), vulkan.SwapchainImageCount(), shaderDir, vulkan);
	*/
}

void UiPresenter::Shutdown()
{
	/*
	_postPassResources.Destroy(_vulkan.LogicalDevice(), _vulkan.Allocator());
	
	_offscreenFramebuffer.Destroy(_vulkan.LogicalDevice(), _vulkan.Allocator());
	_offscreenTextureResource = nullptr;
	*/
}

void UiPresenter::NextSkybox()
{
	_activeSkybox = ++_activeSkybox % _library.GetSkyboxes().size();
	LoadSkybox(_library.GetSkyboxes()[_activeSkybox].Path);
}

void UiPresenter::LoadSkybox(const std::string& path) const
{
	const SkyboxResourceId resId = _scene.LoadAndSetSkybox(path);
}

void UiPresenter::DeleteSelected()
{
	printf("DeleteSelected()\n");
	std::vector<int> ids{};
	std::for_each(_selection.begin(), _selection.end(), [&ids](Entity* s) { ids.emplace_back(s->Id); });
	_delegate.Delete(ids);
}

void UiPresenter::FrameSelectionOrAll()
{
	std::vector<Entity*> targets = {};


	if (_selection.empty()) // Frame scene
	{
		for (auto&& e : _scene.EntitiesView())
		{
			if (e->Renderable)
			{
				targets.emplace_back(e.get());
			}
		}
	}
	else // Frame selection
	{
		for (auto&& e : _selection)
		{
			if (e->Renderable)
			{
				targets.emplace_back(e);
			}
		}
	}


	// Nothing to frame?
	if (targets.empty())
	{
		return;
	}


	// Compute the world space bounds of all renderable's in the selection
	AABB totalWorldBounds;
	bool first = true;

	for (auto& entity : targets)
	{
		auto localBounds = entity->Renderable->GetBounds();
		auto worldBounds = localBounds.Transform(entity->Transform.GetMatrix());

		if (first)
		{
			first = false;
			totalWorldBounds = worldBounds;
		}
		else
		{
			totalWorldBounds = totalWorldBounds.Merge(worldBounds);
		}
	}

	if (first == true || totalWorldBounds.IsEmpty())
	{
		return;
	}

	// Focus the bounds
	auto radius = glm::length(totalWorldBounds.Max() - totalWorldBounds.Min());
	const auto aspect = float(ViewportSize().x) / float(ViewportSize().y);
	_scene.GetCamera().Focus(totalWorldBounds.Center(), radius, aspect);
}

void UiPresenter::ReplaceSelection(Entity* const entity)
{
	ClearSelection();
	_selection.insert(entity);
}

void UiPresenter::ClearSelection()
{
	_selection.clear();
}

void UiPresenter::BuildImGui()
{
	// Start the Dear ImGui frame
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	{
		//auto show_demo_window = true;
		//ImGui::ShowDemoWindow(&show_demo_window);

		// Scene View
		{
			ImGui::SetNextWindowPos(ImVec2(float(ScenePos().x), float(ScenePos().y)));
			ImGui::SetNextWindowSize(ImVec2(float(SceneSize().x), float(SceneSize().y)));

			const auto& entsView = _scene.EntitiesView();
			std::vector<Entity*> allEnts{entsView.size()};
			std::transform(entsView.begin(), entsView.end(), allEnts.begin(), [](const std::unique_ptr<Entity>& pe)
			{
				return pe.get();
			});
			_sceneView.BuildUI(allEnts, _selection);
		}


		// Properties View
		{
			ImGui::SetNextWindowPos(ImVec2(float(PropsPos().x), float(PropsPos().y)));
			ImGui::SetNextWindowSize(ImVec2(float(PropsSize().x), float(PropsSize().y)));

			Entity* selection = _selection.size() == 1 ? *_selection.begin() : nullptr;

			const auto selectionCount = (int)_selection.size();
			if (selectionCount != 1)
			{
				// Reset it all
				_selectionId = -1;
				_tvm = TransformVm{};
				_lvm = std::nullopt;
			}
			else if (selection && selection->Id != _selectionId)
			{
				// New selection

				_selectionId = selection->Id;


				_tvm = TransformVm{&selection->Transform};


				_lvm = selection->Light.has_value()
					       ? std::optional(LightVm{&selection->Light.value()})
					       : std::nullopt;


				// Collect submeshes
				_selectedSubMesh = 0;
				_submeshes.clear();
				if (selection->Renderable.has_value())
				{
					for (const auto& componentSubmesh : selection->Renderable->GetSubmeshes())
					{
						_submeshes.emplace_back(componentSubmesh.Name);
					}
				}
			}
			else
			{
				// Same selection as last frame
			}

			_propsView.BuildUI(selectionCount, _tvm, _lvm);
		}
	}
	ImGui::EndFrame();
	ImGui::Render();
}

void UiPresenter::DrawViewport(u32 imageIndex, VkCommandBuffer commandBuffer)
{
	auto& entities = _scene.EntitiesView();
	std::vector<RenderableResourceId> renderables;
	std::vector<Light> lights;
	std::vector<glm::mat4> transforms;

	for (auto& entity : entities)
	{
		if (entity->Renderable.has_value())
		{
			for (auto&& componentSubmesh : entity->Renderable->GetSubmeshes())
			{
				renderables.emplace_back(componentSubmesh.Id);
				transforms.emplace_back(entity->Transform.GetMatrix());
			}
		}

		if (entity->Light.has_value())
		{
			// Convert from LightComponent to Light

			auto& lightComp = *entity->Light;
			Light light{};

			light.Color = lightComp.Color;
			light.Intensity = lightComp.Intensity;
			switch (lightComp.Type) {
			case LightComponent::Types::point:       light.Type = Light::LightType::Point;       break;
			case LightComponent::Types::directional: light.Type = Light::LightType::Directional; break;
				//case Types::spot: 
			default:
				throw std::invalid_argument("Unsupport light component type");
			}

			light.Pos = entity->Transform.GetPos();
			lights.emplace_back(light);
		}
	}

	auto& camera = _scene.GetCamera();
	const auto view = camera.GetViewMatrix();

		
	_renderer.Draw(
		commandBuffer, imageIndex, _scene.GetRenderOptions(),
		renderables, transforms, lights, view, camera.Position, ViewportPos(), ViewportSize());
}

void UiPresenter::DrawUi(VkCommandBuffer commandBuffer)
{
	// Update Ui
	const auto currentTime = std::chrono::high_resolution_clock::now();
	if (currentTime - _lastUiUpdate > _uiUpdateRate)
	{
		_lastUiUpdate = currentTime;
		BuildImGui();
	}

	// Draw
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}
void UiPresenter::Draw(u32 imageIndex, VkCommandBuffer commandBuffer)
{
	const auto swapchainExtent = _vulkan.SwapchainExtent();
	
	std::vector<VkClearValue> clearColors(2);
	clearColors[0].color = { 0.f, 1.f, 0.f, 1.f };
	clearColors[1].depthStencil = { 1.f, 0ui32 };

	// Draw scene to screen
	{
		const auto renderPassBeginInfo = vki::RenderPassBeginInfo(
			_vulkan.SwapchainRenderPass(), 
			_vulkan.SwapchainFramebuffers()[imageIndex],
			vki::Rect2D({0,0}, swapchainExtent), 
			clearColors);

		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			DrawViewport(imageIndex, commandBuffer);
			DrawUi(commandBuffer);
		}
		vkCmdEndRenderPass(commandBuffer);
	}
}
/*
void UiPresenter::Draw(u32 imageIndex, VkCommandBuffer commandBuffer)
{
	const auto swapchainExtent = _vulkan.SwapchainExtent();
	
	std::vector<VkClearValue> clearColors(2);
	clearColors[0].color = { 0.f, 1.f, 0.f, 1.f };
	clearColors[1].depthStencil = { 1.f, 0ui32 };

	auto& vk = _vulkan;
	
	// Prep offscreen texture for writing to 
	{
		const auto* cmdBuf = vkh::BeginSingleTimeCommands(vk.CommandPool(), vk.LogicalDevice());

		vkh::TransitionImageLayout(cmdBuf,
			_offscreenTextureResource->Image(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
			VK_IMAGE_ASPECT_COLOR_BIT);

		vkh::EndSingeTimeCommands(cmdBuf, vk.CommandPool(), vk.GraphicsQueue(), vk.LogicalDevice());
	}
	
	
	// Draw scene to gbuf
	{
		const auto renderPassBeginInfo = vki::RenderPassBeginInfo(
			_renderer._renderPass, 
			_offscreenFramebuffer.Framebuffer,
			vki::Rect2D({0,0}, swapchainExtent), 
			clearColors);

		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			DrawViewport(imageIndex, commandBuffer);
			DrawUi(commandBuffer);
		}
		vkCmdEndRenderPass(commandBuffer);
	}


	// Prep offscreen texture for sampling from
	{
		const auto cmdBuf = vkh::BeginSingleTimeCommands(vk.CommandPool(), vk.LogicalDevice());

		vkh::TransitionImageLayout(cmdBuf,
			_offscreenTextureResource->Image(),
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 
			VK_IMAGE_ASPECT_COLOR_BIT);

		vkh::EndSingeTimeCommands(cmdBuf, vk.CommandPool(), vk.GraphicsQueue(), vk.LogicalDevice());
	}

	
	// Draw gbuf to screen
	{
		const auto renderPassBeginInfo = vki::RenderPassBeginInfo(
			_vulkan.SwapchainRenderPass(), 
			_vulkan.SwapchainFramebuffers()[imageIndex],
			vki::Rect2D({0,0}, swapchainExtent), 
			clearColors);


		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			// Need to draw a quad
			// Need pipeline
			
			// Render region - Note: this region is the 3d viewport only. ImGui defines it's own viewport
			
			auto viewport = vki::Viewport(0,0, (f32)swapchainExtent.width, (f32)swapchainExtent.height, 0,1);
			auto scissor = vki::Rect2D({ 0,0 }, swapchainExtent);
			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);


			
			const MeshResource mesh = _postPassResources.Quad;
			VkBuffer vertexBuffers[] = { mesh.VertexBuffer };
			VkDeviceSize offsets[] = { 0 };
			
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _postPassResources.Pipeline);
			
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
			vkCmdBindIndexBuffer(commandBuffer, mesh.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _postPassResources.PipelineLayout, 0, 1, &_postPassResources.DescriptorSets[imageIndex], 0, nullptr);
			vkCmdDrawIndexed(commandBuffer, (u32)mesh.IndexCount, 1, 0, 0, 0);
			
		}
		vkCmdEndRenderPass(commandBuffer);
	}
}
*/

void UiPresenter::LoadDemoScene()
{
	printf("LoadDemoScene()\n");
	_library.LoadDemoScene();
	ClearSelection();
	FrameSelectionOrAll();
}

void UiPresenter::LoadHeavyDemoScene()
{
	printf("LoadHeavyDemoScene()\n");
	_library.LoadDemoSceneHeavy();
	ClearSelection();
	FrameSelectionOrAll();
}

void UiPresenter::LoadModel(const std::string& path)
{
	printf("LoadModel(%s)\n", path.c_str());

	// Split path into a dir and filename so we can name the entity
	std::string dir, filename;
	std::tie(dir, filename) = FileService::SplitPathAsDirAndFilename(path);


	// Load asset
	auto renderableComponent = _scene.LoadRenderableComponentFromFile(path);
	if (!renderableComponent.has_value())
	{
		// TODO User facing prompt about failure to load
		return;
	}
	
	// Create new entity
	auto entity = std::make_unique<Entity>();
	entity->Name = filename;
	entity->Transform.SetPos(glm::vec3{0, 0, 0});
	entity->Renderable = std::move(renderableComponent);

	//_scene.SetMaterial(*entity->Renderable, LibraryManager::CreateRandomMaterial());

	ReplaceSelection(entity.get());

	_scene.AddEntity(std::move(entity));

	FrameSelectionOrAll();
}

void UiPresenter::CreateDirectionalLight()
{
	printf("CreateDirectionalLight()\n");

	auto entity = std::make_unique<Entity>();
	entity->Name = "DirectionalLight" + std::to_string(entity->Id);
	entity->Light = LightComponent{};
	entity->Light->Type = LightComponent::Types::directional;
	entity->Light->Intensity = 5;
	entity->Transform.SetPos({0, -1, 0});

	ReplaceSelection(entity.get());
	_scene.AddEntity(std::move(entity));
}

void UiPresenter::CreatePointLight()
{
	printf("CreatePointLight()\n");

	auto entity = std::make_unique<Entity>();
	entity->Name = "PointLight" + std::to_string(entity->Id);
	entity->Light = LightComponent{};
	entity->Light->Type = LightComponent::Types::point;
	entity->Light->Intensity = 250;

	ReplaceSelection(entity.get());
	_scene.AddEntity(std::move(entity));
}

void UiPresenter::CreateSphere()
{
	printf("CreateSphere()\n");

	auto entity = _library.CreateSphere();
	//entity->Action = std::make_unique<TurntableAction>(entity->Transform);
	_scene.SetMaterial(*entity->Renderable, LibraryManager::CreateRandomMaterial());

	ReplaceSelection(entity.get());
	_scene.AddEntity(std::move(entity));
}

void UiPresenter::CreateBlob()
{
	printf("CreateBlob()\n");

	auto entity = _library.CreateBlob();
	//entity->Action = std::make_unique<TurntableAction>(entity->Transform);
	_scene.SetMaterial(*entity->Renderable, LibraryManager::CreateRandomMaterial());

	ReplaceSelection(entity.get());
	_scene.AddEntity(std::move(entity));
}

void UiPresenter::CreateCube()
{
	printf("CreateCube()\n");

	auto entity = _library.CreateCube();
	entity->Transform.SetScale(glm::vec3{0.9f});
	_scene.SetMaterial(*entity->Renderable, LibraryManager::CreateRandomMetalMaterial());

	//entity->Action = std::make_unique<TurntableAction>(entity->Transform);

	ReplaceSelection(entity.get());
	_scene.AddEntity(std::move(entity));
}

void UiPresenter::DeleteAll()
{
	printf("DeleteAll()\n");
	const auto& entities = _scene.EntitiesView();
	std::vector<int> ids{};
	std::for_each(entities.begin(), entities.end(),
	              [&ids](const std::unique_ptr<Entity>& e) { ids.emplace_back(e->Id); });

	_delegate.Delete(ids);
}

RenderOptions UiPresenter::GetRenderOptions()
{
	return _scene.GetRenderOptions();
}

void UiPresenter::SetRenderOptions(const RenderOptions& ro)
{
	_scene.SetRenderOptions(ro);
}

void UiPresenter::LoadAndSetSkybox()
{
	printf("LoadAndSetSkybox()\n");
	
	const auto path = FileService::FilePicker("Load equirectangular map", { "*.hdr" }, "HDR");
	if (!path.empty()) {
		_scene.LoadAndSetSkybox(path);
	}
	// TODO Figure out what to do with the active skybox. Need to design the UI solution first.
	//_activeSkybox = idx;
}

const std::vector<SkyboxInfo>& UiPresenter::GetSkyboxList()
{
	return _library.GetSkyboxes();
}

void UiPresenter::SetActiveSkybox(u32 idx)
{
	const auto& skyboxInfo = _library.GetSkyboxes()[idx];
	_scene.LoadAndSetSkybox(skyboxInfo.Path);
	_activeSkybox = idx;
}

std::optional<MaterialViewState> UiPresenter::GetMaterialState()
{
	Entity* selection = _selection.size() == 1 ? *_selection.begin() : nullptr;

	if (!selection || !selection->Renderable.has_value())
		return std::nullopt;

	const auto& componentSubmesh = selection->Renderable->GetSubmeshes()[_selectedSubMesh];
	const auto& mat = _scene.GetMaterial(componentSubmesh.Id);

	return PopulateMaterialState(mat);
}

void UiPresenter::CommitMaterialChanges(const MaterialViewState& state)
{
	Entity* selection = _selection.size() == 1 ? *_selection.begin() : nullptr;
	if (!selection)
	{
		throw std::runtime_error("How are we commiting a material change when there's no valid selection?");
	}

	const auto& renComp = *selection->Renderable;
	const auto& componentSubmesh = renComp.GetSubmeshes()[_selectedSubMesh];
	auto mat = _scene.GetMaterial(componentSubmesh.Id);


	// Update material properties

	mat.UseBasecolorMap = state.UseBasecolorMap;
	mat.UseMetalnessMap = state.UseMetalnessMap;
	mat.UseRoughnessMap = state.UseRoughnessMap;
	//mat.UseNormalMap = state.UseNormalMap;
	//mat.UseAoMap = state.UseAoMap;

	mat.Basecolor = state.Basecolor;
	mat.Metalness = state.Metalness;
	mat.Roughness = state.Roughness;
	mat.EmissiveIntensity = state.EmissiveIntensity;
	mat.TransparencyCutoffThreshold = state.TransparencyCutoffThreshold;

	mat.InvertNormalMapY = state.InvertNormalMapY;
	mat.InvertNormalMapZ = state.InvertNormalMapZ;
	mat.InvertAoMap = state.InvertAoMap;
	mat.InvertRoughnessMap = state.InvertRoughnessMap;
	mat.InvertMetalnessMap = state.InvertMetalnessMap;

	mat.MetalnessMapChannel = (Material::Channel)state.ActiveMetalnessChannel;
	mat.RoughnessMapChannel = (Material::Channel)state.ActiveRoughnessChannel;
	mat.AoMapChannel = (Material::Channel)state.ActiveAoChannel;
	mat.TransparencyMapChannel = (Material::Channel)state.ActiveTransparencyChannel;

	switch (state.ActiveSolo)
	{
	case 0: mat.ActiveSolo = TextureType::Undefined;        break;
	case 1: mat.ActiveSolo = TextureType::Basecolor;        break;
	case 2: mat.ActiveSolo = TextureType::Normals;          break;
	case 3: mat.ActiveSolo = TextureType::Metalness;        break;
	case 4: mat.ActiveSolo = TextureType::Roughness;        break;
	case 5: mat.ActiveSolo = TextureType::AmbientOcclusion; break;
	case 6: mat.ActiveSolo = TextureType::Emissive;         break;
	case 7: mat.ActiveSolo = TextureType::Transparency;     break;
	default:
		throw std::out_of_range("");
	}

	switch (state.TransparencyMode)
	{
	case 0: mat.TransparencyMode = TransparencyMode::Additive; break;
	case 1: mat.TransparencyMode = TransparencyMode::Cutoff; break;
	default:
		throw std::out_of_range("Unsupported TransparencyMode");
	}
	
	auto UpdateMap = [&](const std::string& newPath, Material& targetMat, const TextureType type)
	{
		std::optional<Material::Map>* pMap;
		
		switch (type)
		{
		case TextureType::Basecolor:        pMap = &targetMat.BasecolorMap;    break;
		case TextureType::Normals:          pMap = &targetMat.NormalMap;       break;
		case TextureType::Roughness:        pMap = &targetMat.RoughnessMap;    break;
		case TextureType::Metalness:        pMap = &targetMat.MetalnessMap;    break;
		case TextureType::AmbientOcclusion: pMap = &targetMat.AoMap;           break;
		case TextureType::Emissive:         pMap = &targetMat.EmissiveMap;     break;
		case TextureType::Transparency:     pMap = &targetMat.TransparencyMap; break;
		case TextureType::Undefined:
		default:
			throw std::invalid_argument("unhandled TextureType");
		}

		if (newPath.empty()) {
			*pMap = std::nullopt;
		}
		else // path is empty, make sure the material is also
		{
			if (!pMap->has_value()) {
				*pMap = Material::Map{}; // make sure we have something to assign to
			}
			
			const bool pathIsDifferent = (*pMap)->Path != newPath;
			if (pathIsDifferent)
			{
				(*pMap)->Id = *_scene.LoadTexture(newPath);
				(*pMap)->Path = newPath;
			}
		}
	};

	UpdateMap(state.BasecolorMapPath, mat, TextureType::Basecolor);
	UpdateMap(state.NormalMapPath, mat, TextureType::Normals);
	UpdateMap(state.MetalnessMapPath, mat, TextureType::Metalness);
	UpdateMap(state.RoughnessMapPath, mat, TextureType::Roughness);
	UpdateMap(state.AoMapPath, mat, TextureType::AmbientOcclusion);
	UpdateMap(state.EmissiveMapPath, mat, TextureType::Emissive);
	UpdateMap(state.TransparencyMapPath, mat, TextureType::Transparency);


	_scene.SetMaterial(componentSubmesh.Id, mat);
}

MaterialViewState UiPresenter::PopulateMaterialState(const Material& mat)
{
	MaterialViewState rvm = {};

	rvm.UseBasecolorMap = mat.UseBasecolorMap;
	rvm.UseMetalnessMap = mat.UseMetalnessMap;
	rvm.UseRoughnessMap = mat.UseRoughnessMap;
	//rvm.UseNormalMap = mat.UseNormalMap;
	//rvm.UseAoMap = mat.UseAoMap;

	rvm.Basecolor = mat.Basecolor;
	rvm.Metalness = mat.Metalness;
	rvm.Roughness = mat.Roughness;
	rvm.EmissiveIntensity = mat.EmissiveIntensity;
	rvm.TransparencyCutoffThreshold = mat.TransparencyCutoffThreshold;
	
	rvm.InvertNormalMapY = mat.InvertNormalMapY;
	rvm.InvertNormalMapZ = mat.InvertNormalMapZ;
	rvm.InvertAoMap = mat.InvertAoMap;
	rvm.InvertRoughnessMap = mat.InvertRoughnessMap;
	rvm.InvertMetalnessMap = mat.InvertMetalnessMap;

	rvm.ActiveMetalnessChannel = int(mat.MetalnessMapChannel);
	rvm.ActiveRoughnessChannel = int(mat.RoughnessMapChannel);
	rvm.ActiveAoChannel = int(mat.AoMapChannel);
	rvm.ActiveTransparencyChannel = int(mat.TransparencyMapChannel);

	switch (mat.ActiveSolo)
	{
	case TextureType::Undefined:    rvm.ActiveSolo = 0;     break;
	case TextureType::Basecolor:    rvm.ActiveSolo = 1;     break;
	case TextureType::Normals:      rvm.ActiveSolo = 2;     break;
	case TextureType::Metalness:    rvm.ActiveSolo = 3;     break;
	case TextureType::Roughness:    rvm.ActiveSolo = 4;     break;
	case TextureType::AmbientOcclusion: rvm.ActiveSolo = 5; break;
	case TextureType::Emissive:     rvm.ActiveSolo = 6;     break;
	case TextureType::Transparency: rvm.ActiveSolo = 7;     break;
	default:
		throw std::out_of_range("Unsupported ActiveSolo");
	}

	switch (mat.TransparencyMode)
	{
	case TransparencyMode::Additive: rvm.TransparencyMode = 0; break;
	case TransparencyMode::Cutoff: rvm.TransparencyMode = 1; break;
	default:
		throw std::out_of_range("Unsupported TransparencyMode");
	}

	auto GetPath = [](const std::optional<Material::Map>& map) { return map.has_value() ? map->Path : ""; };
	rvm.BasecolorMapPath = GetPath(mat.BasecolorMap);
	rvm.NormalMapPath = GetPath(mat.NormalMap);
	rvm.MetalnessMapPath = GetPath(mat.MetalnessMap);
	rvm.RoughnessMapPath = GetPath(mat.RoughnessMap);
	rvm.AoMapPath = GetPath(mat.AoMap);
	rvm.EmissiveMapPath = GetPath(mat.EmissiveMap);
	rvm.TransparencyMapPath = GetPath(mat.TransparencyMap);

	return rvm;
}
