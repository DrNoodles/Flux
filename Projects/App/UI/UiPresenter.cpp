#include "UiPresenter.h"
#include "PropsView/MaterialViewState.h"
#include "PropsView/PropsView.h"

#include <Framework/FileService.h>
#include <State/LibraryManager.h>
#include <State/SceneManager.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>


void UiPresenter::BuildFramebuffer()
{
	// Scene framebuffer is only the size of the scene render region on screen
	{
		const auto sceneRect = ViewportRect();
		
		_sceneFramebuffer = std::make_unique<FramebufferResources>(FramebufferResources::CreateSceneFramebuffer(
			VkExtent2D{ sceneRect.Extent.Width, sceneRect.Extent.Height },
			VK_FORMAT_R16G16B16A16_SFLOAT,
			_renderer.GetRenderPass(),
			_vk.MsaaSamples(),
			_vk.LogicalDevice(), _vk.PhysicalDevice(), _vk.Allocator()));
	}
}


void UiPresenter::HandleSwapchainRecreated(u32 width, u32 height, u32 numSwapchainImages)
{
	_sceneFramebuffer->Destroy();
	_postProcessPass.DestroyDescriptorResources();
	
	BuildFramebuffer(); // resolution
	_postProcessPass.CreateDescriptorResources(TextureData{_sceneFramebuffer->OutputDescriptor}); // num images
}


UiPresenter::UiPresenter(IUiPresenterDelegate& dgate, LibraryManager& library, SceneManager& scene, Renderer& renderer, VulkanService& vulkan, IWindow* window, const std::string& shaderDir):
	_delegate(dgate),
	_scene{scene},
	_library{library},
	_renderer{renderer},
	_vk{vulkan},
	_window{window},
	_sceneView{SceneView{this}},
	_propsView{PropsView{this}},
	_viewportView{ViewportView{this, renderer}},
	_shaderDir{shaderDir}
{
	_window->WindowSizeChanged.Attach(_windowSizeChangedHandler);
	_window->PointerMoved.Attach(_pointerMovedHandler);
	_window->PointerWheelChanged.Attach(_pointerWheelChangedHandler);
	_window->KeyDown.Attach(_keyDownHandler);
	_window->KeyUp.Attach(_keyUpHandler);



	_shadowDrawResources = ShadowMap::ShadowmapDrawResources{{ 4096,4096 }, _shaderDir, _vk, _renderer.Hack_GetPbrPipelineLayout()};
	
	BuildFramebuffer();
	_postProcessPass = PostProcessPass(shaderDir, &vulkan);
	_postProcessPass.CreateDescriptorResources(TextureData{_sceneFramebuffer->OutputDescriptor});
}

void UiPresenter::Shutdown()
{
	_window->WindowSizeChanged.Detach(_windowSizeChangedHandler);
	_window->PointerMoved.Detach(_pointerMovedHandler);
	_window->PointerWheelChanged.Detach(_pointerWheelChangedHandler);
	_window->KeyDown.Detach(_keyDownHandler);
	_window->KeyUp.Detach(_keyUpHandler);

	// Cleanup renderpass resources
	_sceneFramebuffer->Destroy();
	_shadowDrawResources.Destroy(_vk.LogicalDevice(), _vk.Allocator());
	_postProcessPass.Destroy(_vk.LogicalDevice(), _vk.Allocator());
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


struct ShadowCaster
{
	glm::mat4 Projection;
	glm::mat4 View;
	//glm::vec3 Pos;
};

std::optional<ShadowCaster> FindShadowCaster(Entity* entity)
{
	assert(entity != nullptr);

	if (!entity->Light.has_value() || !(entity->Light->Type == LightComponent::Types::directional))
		return std::nullopt;
	
	ShadowCaster s = {};
	const auto eye = entity->Transform.GetPos();
	s.View = glm::lookAt(eye, {0,0,0}, {0,1,0});
	s.Projection = glm::ortho(-50.f, 50.f, -50.f, 50.f, -50.f, 50.f); // TODO Set the bounds dynamically. Near/Far clip planes seems weird. -50 near solves odd clipping issues. Check my understanding of this.
	return s;
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
	// TODO Just update the descriptor for this imageIndex????
	//_postProcessPass.CreateDescriptorResources(TextureData{_sceneFramebuffer.OutputDescriptor});

	
	// Prep scene objects for drawing - TODO There's duplicate effort done below
	std::optional<ShadowCaster> shadowCaster = {};
	std::vector<RenderableResourceId> renderableIds = {};
	std::vector<glm::mat4> transforms = {};
	std::vector<Light> lights;
	
	for (const auto& entity : _scene.EntitiesView())
	{
		// Gather renderable/transform pairs
		if (entity->Renderable.has_value())
		{
			for (auto&& componentSubmesh : entity->Renderable->GetSubmeshes())
			{
				renderableIds.emplace_back(componentSubmesh.Id);
				transforms.emplace_back(entity->Transform.GetMatrix());
			}
		}

		if (!shadowCaster.has_value())
		{
			shadowCaster = FindShadowCaster(entity.get());
		}

		if (entity->Light.has_value())
		{
			auto light = [&entity]() -> Light
			{
				auto& lightComp = *entity->Light;

				Light l = {};
				l.Pos = entity->Transform.GetPos();
				l.Color = lightComp.Color;
				l.Intensity = lightComp.Intensity;
				
				switch (lightComp.Type) {
				case LightComponent::Types::point:       l.Type = Light::LightType::Point;       break;
				case LightComponent::Types::directional: l.Type = Light::LightType::Directional; break;
					//case Types::spot: 
				default:
					throw std::invalid_argument("Unsupport light component type");
				}
				
				return l;
			}();

			lights.emplace_back(light);
		}
	}


	// Update all descriptors
	_renderer.UpdateDescriptors(GetRenderOptions());
	// shadow? post? gui?

	
	auto lightSpaceMatrix = shadowCaster.has_value()
		? shadowCaster->Projection * shadowCaster->View
		: glm::identity<glm::mat4>();
	
	// Draw shadow pass
	if (shadowCaster.has_value())
	{
		const auto& renderables = _renderer.Hack_GetRenderables();
		const auto& meshes = _renderer.Hack_GetMeshes();

		// Update all UBOs - TODO introduce a new MVP only vert shader only ubo for use with Pbr and Shadow shaders. 
		for (size_t i = 0; i < renderableIds.size(); i++)
		{
			const auto& renderable = *renderables[renderableIds[i].Id];
			// TODO need to create UBOs 
			const auto& modelBufferMemory = renderable.FrameResources[imageIndex].UniformBufferMemory; // NOTE This memory is used for UniversalUbo PBR rendering!

			UniversalUbo ubo = {};
			ubo.LightSpaceMatrix = lightSpaceMatrix;
			ubo.Model = transforms[i];
			
			// Copy to gpu - TODO PERF Keep mem mapped 
			void* data;
			auto size = sizeof(ubo);
			vkMapMemory(_vk.LogicalDevice(), modelBufferMemory, 0, size, 0, &data);
			memcpy(data, &ubo, size);
			vkUnmapMemory(_vk.LogicalDevice(), modelBufferMemory);
		}

		
		// Draw all objects

		// Clear colour
		std::vector<VkClearValue> clearColors(1);
		clearColors[0].depthStencil = { 1.f, 0 };

		float depthBiasConstant = 1.0f;
		float depthBiasSlope = 1.f;
		auto& shadow = _shadowDrawResources;
		auto shadowRect = vki::Rect2D(0, 0, shadow.Resolution.width, shadow.Resolution.height);
		auto shadowViewport = vki::Viewport(shadowRect);
		const auto renderPassBeginInfo = vki::RenderPassBeginInfo(shadow.RenderPass, shadow.Framebuffer->Framebuffer,
			shadowRect,
			clearColors);
		
		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			vkCmdSetViewport(commandBuffer, 0, 1, &shadowViewport);
			vkCmdSetScissor(commandBuffer, 0, 1, &shadowRect);
			vkCmdSetDepthBias(commandBuffer, depthBiasConstant, 0, depthBiasSlope);
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
					VK_PIPELINE_BIND_POINT_GRAPHICS, _renderer.Hack_GetPbrPipelineLayout(),
					0, 1, &renderable->FrameResources[imageIndex].DescriptorSet, 0, nullptr);
				vkCmdDrawIndexed(commandBuffer, (u32)mesh.IndexCount, 1, 0, 0, 0);
			}
		}
		vkCmdEndRenderPass(commandBuffer);
	}


	// Draw scene to gbuf
	{
		auto& camera = _scene.GetCamera();
		auto view = camera.GetViewMatrix();
		glm::vec3 camPos = camera.Position;

		// Calc Projection
		const auto vfov = 45.f;
		Rect2D region = ViewportRect();
		const auto aspect = region.Extent.Width / (f32)region.Extent.Height;
		auto projection = glm::perspective(glm::radians(vfov), aspect, 0.05f, 1000.f);
		projection = glm::scale(projection, glm::vec3{ 1.f,-1.f,1.f });// flip Y to convert glm from OpenGL coord system to Vulkan


		// Scene Viewport / Region. Only the part of the screen showing the scene.

		// Clear colour
		std::vector<VkClearValue> clearColors(2);
		clearColors[0].color = { 0.f, 1.f, 0.f, 1.f };
		clearColors[1].depthStencil = { 1.f, 0ui32 };

	
		auto renderRect = vki::Rect2D({ 0, 0 }, { _sceneFramebuffer->Desc.Extent });
		auto renderViewport = vki::Viewport(renderRect);
		
		const auto renderPassBeginInfo = vki::RenderPassBeginInfo(_renderer.GetRenderPass(),
			_sceneFramebuffer->Framebuffer,
			renderRect,
			clearColors);

		vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			vkCmdSetViewport(commandBuffer, 0, 1, &renderViewport);
			vkCmdSetScissor(commandBuffer, 0, 1, &renderRect);

			_renderer.Draw(commandBuffer, imageIndex, GetRenderOptions(), renderableIds, transforms, lights, view, projection, camPos, lightSpaceMatrix);
		}
		vkCmdEndRenderPass(commandBuffer);

	}

	// Draw Ui full screen
	{
		// Clear colour
		std::vector<VkClearValue> clearColors(2);
		clearColors[0].color = {0.f, 1.f, 0.f, 1.f};
		clearColors[1].depthStencil = {1.f, 0ui32};

		// Whole screen framebuffer dimensions
		const auto& swap = _vk.GetSwapchain();
		const auto framebufferRect = vki::Rect2D({ 0, 0 }, swap.GetExtent());
		const auto framebufferViewport = vki::Viewport(framebufferRect);

		const auto sceneRectShared = ViewportRect();
		const auto sceneRect = vki::Rect2D(
			{ sceneRectShared.Offset.X,     sceneRectShared.Offset.Y },
			{ sceneRectShared.Extent.Width, sceneRectShared.Extent.Height });
		
		const auto sceneViewport = vki::Viewport(sceneRect);
		
		const auto beginInfo = vki::RenderPassBeginInfo(swap.GetRenderPass(), swap.GetFramebuffers()[imageIndex],
			framebufferRect, clearColors);

		vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			vkCmdSetViewport(commandBuffer, 0, 1, &sceneViewport);
			vkCmdSetScissor(commandBuffer, 0, 1, &sceneRect);
			_postProcessPass.Draw(commandBuffer, imageIndex, _scene.GetRenderOptions());

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
	entity->Transform.SetPos({10, 10, 10});

	entity->Light = LightComponent{};
	entity->Light->Type = LightComponent::Types::directional;
	entity->Light->Intensity = 5;

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
	_vk.InvalidateSwapchain();
	//std::cout << "OnWindowSizeChanged: (" << args.Size.Width << "," << args.Size.Height << ")\n";
}
