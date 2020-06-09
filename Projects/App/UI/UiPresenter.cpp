#include "UiPresenter.h"
#include "PropsView/MaterialViewState.h"
#include "PropsView/PropsView.h"

#include <Framework/FileService.h>
#include <State/LibraryManager.h>
#include <State/SceneManager.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>


void UiPresenter::CreateSceneFramebuffer()
{
	// Scene framebuffer is only the size of the scene render region on screen
	const auto sceneRect = ViewportRect();
	const auto sceneExtent = VkExtent2D{sceneRect.Extent.Width, sceneRect.Extent.Height};

	_sceneFramebuffer = OffScreen::CreateSceneOffscreenFramebuffer(
		sceneExtent,
		VK_FORMAT_R16G16B16A16_SFLOAT,
		_renderer.GetRenderPass(),
		_vulkan.MsaaSamples(),
		_vulkan.LogicalDevice(), _vulkan.PhysicalDevice());
}

void UiPresenter::CreateQuadResources(const std::string& shaderDir)
{
	// Create quad resources
	_postPassDrawResources = OnScreen::CreateQuadResources(
		_vulkan.GetSwapchain().GetRenderPass(),
		shaderDir,
		_vulkan.LogicalDevice(), _vulkan.PhysicalDevice(), _vulkan.CommandPool(), _vulkan.GraphicsQueue());
}

void UiPresenter::CreateQuadDescriptorSets()
{
	// Create quad descriptor sets
	_postPassDescriptorResources = OnScreen::QuadDescriptorResources::Create(_sceneFramebuffer.OutputDescriptor,
	                                                                 _vulkan.GetSwapchain().GetImageCount(),
	                                                                 _postPassDrawResources.DescriptorSetLayout,
	                                                                 _vulkan.LogicalDevice(), _vulkan.PhysicalDevice());
}

void UiPresenter::HandleSwapchainRecreated(u32 width, u32 height, u32 numSwapchainImages)
{
	_sceneFramebuffer.Destroy(_vulkan.LogicalDevice(), _vulkan.Allocator());
	_postPassDescriptorResources.Destroy(_vulkan.LogicalDevice(), _vulkan.Allocator());
	_shadowDescriptorResources.Destroy(_vulkan.LogicalDevice(), _vulkan.Allocator());

	
	CreateSceneFramebuffer();
	CreateQuadDescriptorSets();
	_shadowDescriptorResources = ShadowMap::ShadowmapDescriptorResources::Create(_vulkan.GetSwapchain().GetImageCount(), _shadowDrawResources.DescriptorSetLayout, _vulkan.LogicalDevice(), _vulkan.PhysicalDevice());
}


UiPresenter::UiPresenter(IUiPresenterDelegate& dgate, LibraryManager& library, SceneManager& scene, Renderer& renderer, VulkanService& vulkan, IWindow* window, const std::string& shaderDir):
	_delegate(dgate),
	_scene{scene},
	_library{library},
	_renderer{renderer},
	_vulkan{vulkan},
	_window{window},
	_sceneView{SceneView{this}},
	_propsView{PropsView{this}},
	_viewportView{ViewportView{this, renderer}}
{
	_window->WindowSizeChanged.Attach(_windowSizeChangedHandler);
	_window->PointerMoved.Attach(_pointerMovedHandler);
	_window->PointerWheelChanged.Attach(_pointerWheelChangedHandler);
	_window->KeyDown.Attach(_keyDownHandler);
	_window->KeyUp.Attach(_keyUpHandler);

	//CreateShadowFramebuffer();
	_shadowDrawResources = ShadowMap::ShadowmapDrawResources{{ 1024,1024 }, shaderDir, _vulkan};
	_shadowDescriptorResources = ShadowMap::ShadowmapDescriptorResources::Create(_vulkan.GetSwapchain().GetImageCount(), _shadowDrawResources.DescriptorSetLayout, _vulkan.LogicalDevice(), _vulkan.PhysicalDevice());
	CreateSceneFramebuffer();
	CreateQuadResources(shaderDir);
	CreateQuadDescriptorSets();
}

void UiPresenter::Shutdown()
{
	_window->WindowSizeChanged.Detach(_windowSizeChangedHandler);
	_window->PointerMoved.Detach(_pointerMovedHandler);
	_window->PointerWheelChanged.Detach(_pointerWheelChangedHandler);
	_window->KeyDown.Detach(_keyDownHandler);
	_window->KeyUp.Detach(_keyUpHandler);

	// Cleanup renderpass resources
	_sceneFramebuffer.Destroy(_vulkan.LogicalDevice(), _vulkan.Allocator());
	_postPassDescriptorResources.Destroy(_vulkan.LogicalDevice(), _vulkan.Allocator());
	_postPassDrawResources.Destroy(_vulkan.LogicalDevice(), _vulkan.Allocator());
	_shadowDrawResources.Destroy(_vulkan.LogicalDevice(), _vulkan.Allocator());
	_shadowDescriptorResources.Destroy(_vulkan.LogicalDevice(), _vulkan.Allocator());
}

void UiPresenter::NextSkybox()
{
	_activeSkybox = ++_activeSkybox % _library.GetSkyboxes().size();
	LoadSkybox(_library.GetSkyboxes()[_activeSkybox].Path);
}

void UiPresenter::LoadSkybox(const std::string& path) const
{
	_scene.LoadAndSetSkybox(path);
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
	auto viewport = ViewportRect();
	const auto aspect = viewport.Extent.Width / (f32)viewport.Extent.Height;
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
			const auto rect = SceneRect();
			ImGui::SetNextWindowPos( ImVec2{ (f32)rect.Offset.X,     (f32)rect.Offset.Y      });
			ImGui::SetNextWindowSize(ImVec2{ (f32)rect.Extent.Width, (f32)rect.Extent.Height });

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
			const auto rect = PropsRect();
			ImGui::SetNextWindowPos( ImVec2{ (f32)rect.Offset.X,     (f32)rect.Offset.Y      });
			ImGui::SetNextWindowSize(ImVec2{ (f32)rect.Extent.Width, (f32)rect.Extent.Height });

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
	const auto& entities = _scene.EntitiesView();
	std::vector<RenderableResourceId> renderables;
	std::vector<Light> lights;
	std::vector<glm::mat4> transforms;

	bool lightFound = false;
	glm::mat4 lightProjection = {};
	glm::vec3 lightPos = {};
	glm::mat4 lightView = {};
	
	for (const auto& entity : entities)
	{
		if (entity->Renderable.has_value())
		{
			for (auto&& componentSubmesh : entity->Renderable->GetSubmeshes())
			{
				renderables.emplace_back(componentSubmesh.Id);
				transforms.emplace_back(entity->Transform.GetMatrix());
			}
		}

		// Use the first directional light as shadow caster
		if (!lightFound && entity->Light.has_value() && entity->Light->Type == LightComponent::Types::directional)
		{
			lightFound = true;
			lightPos = entity->Transform.GetPos();;

			//lightProjection = glm::ortho(-5.f, 5.f, 5.f, -5.f, 0.01f, 20.f); // TODO Set the bounds dynamically

			auto region = ViewportRect();
			const auto aspect = region.Extent.Width / (f32)region.Extent.Height;
			lightProjection = glm::perspective(glm::radians(45.f), aspect, 2.f, 20.f);
			lightProjection = glm::scale(lightProjection, glm::vec3{ 1.f,-1.f,1.f });// flip Y to convert glm from OpenGL coord system to Vulkan

			lightView = glm::lookAt(lightPos, {0,0,0}, {0,1,0});
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
	auto view = camera.GetViewMatrix();
	glm::vec3 camPos = camera.Position;
	
	// Calc Projection
	const auto vfov = 45.f;
	Rect2D region = ViewportRect();
	const auto aspect = region.Extent.Width / (f32)region.Extent.Height;
	auto projection = glm::perspective(glm::radians(vfov), aspect, 0.05f, 1000.f);
	projection = glm::scale(projection, glm::vec3{ 1.f,-1.f,1.f });// flip Y to convert glm from OpenGL coord system to Vulkan


	// TEMP - view the scene using the light's projection/view matrix
	if (lightFound) 
	{
		view = lightView;
		projection = lightProjection;
		camPos = lightPos;
	}

	_renderer.Draw(
		commandBuffer, imageIndex, _scene.GetRenderOptions(),
		renderables, transforms, lights, view, projection, camPos);
}

void UiPresenter::DrawPostProcessedViewport(VkCommandBuffer commandBuffer, i32 imageIndex)
{
	// Update Ubo
	{
		const auto& ro = _scene.GetRenderOptions();
		PostUbo ubo;
		
		ubo.ShowClipping = (i32)ro.ShowClipping;
		ubo.ExposureBias =      ro.ExposureBias;
		
		ubo.EnableVignette = (i32)ro.Vignette.Enabled;
		ubo.VignetteColor       = ro.Vignette.Color;
		ubo.VignetteInnerRadius = ro.Vignette.InnerRadius;
		ubo.VignetteOuterRadius = ro.Vignette.OuterRadius;

		ubo.EnableGrain   = (i32)ro.Grain.Enabled;
		ubo.GrainStrength      = ro.Grain.Strength;
		ubo.GrainColorStrength = ro.Grain.ColorStrength;
		ubo.GrainSize          = ro.Grain.Size;

		
		void* data;
		const auto size = sizeof(ubo);
		vkMapMemory(_vulkan.LogicalDevice(), _postPassDescriptorResources.UboBuffersMemory[imageIndex], 0, size, 0, &data);
		memcpy(data, &ubo, size);
		vkUnmapMemory(_vulkan.LogicalDevice(), _postPassDescriptorResources.UboBuffersMemory[imageIndex]);
	}

	// Draw
	const MeshResource& mesh = _postPassDrawResources.Quad;
	VkBuffer vertexBuffers[] = { mesh.VertexBuffer };
	VkDeviceSize pOffsets = {0};
	
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _postPassDrawResources.Pipeline);

	
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, &pOffsets);
	vkCmdBindIndexBuffer(commandBuffer, mesh.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdBindDescriptorSets(commandBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		_postPassDrawResources.PipelineLayout,
		0, 1,
		&_postPassDescriptorResources.DescriptorSets[imageIndex], 0, nullptr);

	vkCmdDrawIndexed(commandBuffer, (u32)mesh.IndexCount, 1, 0, 0, 0);
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
	// Clear colour
	std::vector<VkClearValue> clearColors(2);
	clearColors[0].color = {0.f, 1.f, 0.f, 1.f};
	clearColors[1].depthStencil = {1.f, 0ui32};

	auto& vk = _vulkan;
	const auto& swap = vk.GetSwapchain();
	const auto swapExtent = swap.GetExtent();


	// Whole screen framebuffer dimensions
	const auto framebufferRect = vki::Rect2D({0, 0}, swapExtent);
	const auto framebufferViewport = vki::Viewport(framebufferRect);

	
	// Scene Viewport / Region. Only the part of the screen showing the scene.
	const auto sceneRectShared = ViewportRect();


	// Draw shadow pass - One big HACK to get it working
	{
		auto shadow = _shadowDrawResources;
		auto shadowRect = vki::Rect2D(0, 0, shadow.Size.width, shadow.Size.height);
		auto shadowViewport = vki::Viewport(shadowRect);
		const auto renderPassBeginInfo = vki::RenderPassBeginInfo(shadow.RenderPass, shadow.Framebuffer.Framebuffer,
			shadowRect,
			clearColors);


		// Prep scene objects for drawing - TODO Duplicate effort done here and DrawViewport
		bool lightFound = false;
		glm::mat4 lightProjection = {};
		glm::mat4 lightView = {};
		std::vector<RenderableResourceId> renderableIds = {};
		std::vector<glm::mat4> transforms = {};
		
		for (const auto& entity : _scene.EntitiesView())
		{
			if (entity->Renderable.has_value())
			{
				for (auto&& componentSubmesh : entity->Renderable->GetSubmeshes())
				{
					renderableIds.emplace_back(componentSubmesh.Id);
					transforms.emplace_back(entity->Transform.GetMatrix());
				}
			}

			// Use the first directional light as shadow caster
			if (!lightFound && entity->Light.has_value() && entity->Light->Type == LightComponent::Types::directional)
			{
				lightFound = true;
				//lightProjection = glm::ortho(-5.f, 5.f, 5.f, -5.f, 0.01f, 20.f); // TODO Set the bounds dynamically

				auto region = ViewportRect();
				const auto aspect = region.Extent.Width / (f32)region.Extent.Height;
				lightProjection = glm::perspective(glm::radians(45.f), aspect, 2.f, 20.f);
				lightProjection = glm::scale(lightProjection, glm::vec3{ 1.f,-1.f,1.f });// flip Y to convert glm from OpenGL coord system to Vulkan

				lightView = glm::lookAt(entity->Transform.GetPos(), {0,0,0}, {0,1,0});
			}
		}

		// Draw the scene
		if (lightFound)
		{
			const auto& renderables = _renderer.Hack_GetRenderables();
			const auto& meshes = _renderer.Hack_GetMeshes();

			// Update Ubo - TODO introduce a new MVP only vert shader only ubo for use with Pbr and Shadow shaders. 
			for (size_t i = 0; i < renderableIds.size(); i++)
			{
				const auto& renderable = *renderables[renderableIds[i].Id];
				const auto& modelBufferMemory = renderable.FrameResources[imageIndex].UniformBufferMemory;

				ShadowVertUbo ubo = {};
				ubo.MVP = lightProjection * lightView * transforms[i];
				
				// Copy to gpu - TODO PERF Keep mem mapped 
				void* data;
				auto size = sizeof(ubo);
				vkMapMemory(vk.LogicalDevice(), modelBufferMemory, 0, size, 0, &data);
				memcpy(data, &ubo, size);
				vkUnmapMemory(vk.LogicalDevice(), modelBufferMemory);
			}


			// Draw
			vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			{
				vkCmdSetViewport(commandBuffer, 0, 1, &shadowViewport);
				vkCmdSetScissor(commandBuffer, 0, 1, &shadowRect);
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow.Pipeline);

				for (const auto& id : renderableIds)
				{
					const auto& renderable = renderables[id.Id].get();
					const auto& mesh = *meshes[renderable->MeshId.Id];

					// Draw mesh
					VkBuffer vertexBuffers[] = { mesh.VertexBuffer };
					VkDeviceSize offsets[] = { 0 };
					vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
					vkCmdBindIndexBuffer(commandBuffer, mesh.IndexBuffer, 0, VK_INDEX_TYPE_UINT32);

					vkCmdBindDescriptorSets(commandBuffer,
						VK_PIPELINE_BIND_POINT_GRAPHICS, shadow.PipelineLayout, 
						0, 1, &_shadowDescriptorResources.DescriptorSets[imageIndex], 0, nullptr);

					vkCmdDrawIndexed(commandBuffer, (u32)mesh.IndexCount, 1, 0, 0, 0);
				}
			}
			vkCmdEndRenderPass(commandBuffer);
		}
	}


	
	// Draw scene to gbuf
	{
		const auto sceneRectNoOffset = vki::Rect2D({0, 0}, {sceneRectShared.Extent.Width, sceneRectShared.Extent.Height});
		const auto sceneViewportNoOffset = vki::Viewport(sceneRectNoOffset);
		
		const auto renderPassBeginInfo = vki::RenderPassBeginInfo(_renderer.GetRenderPass(), _sceneFramebuffer.Framebuffer,
		                                                          sceneRectNoOffset,
		                                                          clearColors);

		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			vkCmdSetViewport(commandBuffer, 0, 1, &sceneViewportNoOffset);
			vkCmdSetScissor(commandBuffer, 0, 1, &sceneRectNoOffset);
			
			DrawViewport(imageIndex, commandBuffer);
		}
		vkCmdEndRenderPass(commandBuffer);
	}

	
	// Draw Ui full screen
	{
		const auto renderPassBeginInfo = vki::RenderPassBeginInfo(swap.GetRenderPass(),
		                                                          swap.GetFramebuffers()[imageIndex],
		                                                          framebufferRect,
		                                                          clearColors);

		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			const auto sceneRect = vki::Rect2D({sceneRectShared.Offset.X, sceneRectShared.Offset.Y},
			                                   {sceneRectShared.Extent.Width, sceneRectShared.Extent.Height});
			const auto sceneViewport = vki::Viewport(sceneRect);

			vkCmdSetViewport(commandBuffer, 0, 1, &sceneViewport);
			vkCmdSetScissor(commandBuffer, 0, 1, &sceneRect);
			
			DrawPostProcessedViewport(commandBuffer, imageIndex);

			vkCmdSetViewport(commandBuffer, 0, 1, &framebufferViewport);
			DrawUi(commandBuffer);
		}
		vkCmdEndRenderPass(commandBuffer);
	}
}

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
	auto [dir, filename] = FileService::SplitPathAsDirAndFilename(path);


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
	entity->Transform.SetPos({-10, -10, -10});

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

	return MaterialViewState::CreateFrom(mat);
}

void UiPresenter::CommitMaterialChanges(const MaterialViewState& state)
{
	Entity* selection = _selection.size() == 1 ? *_selection.begin() : nullptr;
	if (!selection) {
		throw std::runtime_error("How are we commiting a material change when there's no valid selection?");
	}
	
	const auto newMat = MaterialViewState::ToMaterial(state, _scene);

	const auto& renComp = *selection->Renderable;
	const auto& componentSubmesh = renComp.GetSubmeshes()[_selectedSubMesh];

	_scene.SetMaterial(componentSubmesh.Id, newMat);
}


void UiPresenter::OnKeyDown(IWindow* sender, KeyEventArgs args)
{
	//std::cout << "OnKeyDown()\n";
	
	// TODO Refactor - this is ugly as it's accessing the gui's state in a global way.
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantTextInput || io.WantCaptureKeyboard)
		return;
	
	// ONLY on pressed is handled
	if (args.Action == KeyAction::Repeat) return;

	if (args.Key == VirtualKey::F)      { FrameSelectionOrAll(); }
	if (args.Key == VirtualKey::C)      { NextSkybox(); }
	if (args.Key == VirtualKey::N)      { _delegate.ToggleUpdateEntities(); }
	if (args.Key == VirtualKey::Delete) { DeleteSelected(); }
}

void UiPresenter::OnKeyUp(IWindow* sender, KeyEventArgs args)
{
	//std::cout << "OnKeyUp()\n";//: (" << args.CurrentPoint.Position.X << "," << args.CurrentPoint.Position.Y << ")\n";
}

void UiPresenter::OnPointerWheelChanged(IWindow* sender, PointerEventArgs args)
{
	//std::cout << "OnPointerWheelChanged(" << args.CurrentPoint.Properties.IsHorizonalMouseWheel << "," << args.CurrentPoint.Properties.MouseWheelDelta << ")\n";
	
	// TODO Refactor - this is ugly as it's accessing the gui's state in a global way.
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse)
		return;

	//_scene.GetCamera().ProcessMouseScroll(float(offset.Y));
}

void UiPresenter::OnPointerMoved(IWindow* sender, PointerEventArgs args)
{
	//std::cout << "OnPointerMoved: (" << args.CurrentPoint.Position.X << "," << args.CurrentPoint.Position.Y << ")\n";

	// TODO Refactor - this is ugly as it's accessing the gui's state in a global way.
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse)
		return;
	
	const auto xPos = args.CurrentPoint.Position.X;
	const auto yPos = args.CurrentPoint.Position.Y;
	
	// On first input lets remove a snap
	if (_firstCursorInput)
	{
		_lastCursorX = xPos;
		_lastCursorY = yPos;
		_firstCursorInput = false;
	}

	const auto xDiff = xPos - _lastCursorX;
	const auto yDiff = _lastCursorY - yPos;
	_lastCursorX = xPos;
	_lastCursorY = yPos;


	const auto windowSize = sender->GetSize();
	const glm::vec2 diffRatio{ xDiff/(f64)windowSize.Height, yDiff/(f64)windowSize.Height}; // Note both are / Height
	
	const bool isLmb = sender->GetMouseButton(MouseButton::Left)   == MouseButtonAction::Pressed;
	const bool isMmb = sender->GetMouseButton(MouseButton::Middle) == MouseButtonAction::Pressed;
	const bool isRmb = sender->GetMouseButton(MouseButton::Right)  == MouseButtonAction::Pressed;

	
	// Camera control
	auto& camera = _scene.GetCamera();
	if (isLmb)
	{
		const float arcSpeed = 1.5f*3.1415f;
		camera.Arc(diffRatio.x * arcSpeed, diffRatio.y * arcSpeed);
	}
	if (isMmb || isRmb)
	{
		const auto dir = isMmb
			? glm::vec3{ diffRatio.x, -diffRatio.y, 0 } // mmb pan
		: glm::vec3{ 0, 0, diffRatio.y };     // rmb zoom

		const auto len = glm::length(dir);
		if (len > 0.000001f) // small float
		{
			auto speed = Speed::Normal;
			if (_window->GetKey(VirtualKey::LeftControl) == KeyAction::Pressed)
			{
				speed = Speed::Slow;
			}
			if (_window->GetKey(VirtualKey::LeftShift) == KeyAction::Pressed)
			{
				speed = Speed::Fast;
			}
			camera.Move(len, glm::normalize(dir), speed);
		}
	}
}
void UiPresenter::OnWindowSizeChanged(IWindow* sender, const WindowSizeChangedEventArgs args)
{
	_vulkan.InvalidateSwapchain();
	//std::cout << "OnWindowSizeChanged: (" << args.Size.Width << "," << args.Size.Height << ")\n";
}
