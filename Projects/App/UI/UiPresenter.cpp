#include "UiPresenter.h"
#include "PropsView/MaterialViewState.h"
#include "PropsView/PropsView.h"
#include "RendererConverters.h"

#include <Framework/FileService.h>
#include <State/LibraryManager.h>
#include <State/SceneManager.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

UiPresenter::UiPresenter(IUiPresenterDelegate& dgate, LibraryManager& library, SceneManager& scene, VulkanService& vulkan, IWindow* window, const std::string& shaderDir, const std::string& assetDir, IModelLoaderService& modelLoaderService) :
	_delegate(dgate),
	_scene{scene},
	_library{library},
	_vk{vulkan},
	_window{window},
	_sceneView{SceneView{this}},
	_propsView{PropsView{this}},
	_shaderDir{shaderDir}
{
	_window->WindowSizeChanged.Attach(_windowSizeChangedHandler);
	_window->PointerMoved.Attach(_pointerMovedHandler);
	_window->PointerWheelChanged.Attach(_pointerWheelChangedHandler);
	_window->KeyDown.Attach(_keyDownHandler);
	_window->KeyUp.Attach(_keyUpHandler);

	_sceneRenderer = std::make_unique<SceneRenderer>(_vk, _shaderDir, assetDir, modelLoaderService, 
		Extent2D{ ViewportRect().Extent.Width, ViewportRect().Extent.Height });
	
	_postProcessPass = PostProcessRenderPass(shaderDir, &vulkan);
	_postProcessPass.CreateDescriptorResources(TextureData{_sceneRenderer->GetOutputDescritpor()});

	_viewportView = ViewportView{this, _sceneRenderer.get()};
}

void UiPresenter::Shutdown()
{
	_window->WindowSizeChanged.Detach(_windowSizeChangedHandler);
	_window->PointerMoved.Detach(_pointerMovedHandler);
	_window->PointerWheelChanged.Detach(_pointerWheelChangedHandler);
	_window->KeyDown.Detach(_keyDownHandler);
	_window->KeyUp.Detach(_keyUpHandler);

	// Cleanup renderpass resources
	_sceneRenderer = nullptr; // RAII
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


		// Collect materials - Must be done below scene. See below for details.

		// TODO CAUTION! While building Scene View above it can break GUI. Example below.

		/*
		 * Creating a new object (eg sphere) will create a new material. The resources are created during that code block.
		 * Properties View below will SelectSubMesh(0) on a new selection.
		 * The materials list would be out of date if it was not updated right here, between Scene and Props views.
		 * This isn't scalable as it adds a hidden dependency!
		 * 
		 * The solution is to queue a state change command, and do it outside of the GUI refresh.
		 * This solution is a good first step towards undo/redo functionality.
		 */
		const auto materials = _scene.GetMaterials();
		const auto count = materials.size();
		
		_materials.resize(count);
		for (size_t i = 0; i < count; i++)
		{
			_materials[i] = std::make_pair(_scene.GetMaterial(materials[i]->Id)->Name, materials[i]->Id);
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
				_submeshes.clear();
				if (selection->Renderable.has_value())
				{
					for (const auto& componentSubmesh : selection->Renderable->GetSubmeshes())
					{
						_submeshes.emplace_back(componentSubmesh.Name);
					}
				}

				SelectSubMesh(0);
			}
			else
			{
				// Same selection as last frame
			}

			
			if (_selectedMaterialIndex >= (int)_materials.size())
			{
				assert(false); // selected material is out of scope. 
				_selectedMaterialIndex =  (int)_materials.size() - 1;
			}

			_propsView.BuildUI(selectionCount, _tvm, _lvm);
		}
	}
	ImGui::EndFrame();
	ImGui::Render();
}

void UiPresenter::HandleSwapchainRecreated(u32 width, u32 height, u32 numSwapchainImages)
{
	_postProcessPass.DestroyDescriptorResources();
	_sceneRenderer->HandleSwapchainRecreated(ViewportRect().Extent.Width, ViewportRect().Extent.Height, numSwapchainImages);
	_postProcessPass.CreateDescriptorResources(TextureData{_sceneRenderer->GetOutputDescritpor()});
}

void UiPresenter::Draw(u32 imageIndex, VkCommandBuffer commandBuffer)
{
	// TODO Just update the descriptor for this imageIndex????
	//_postProcessPass.CreateDescriptorResources(TextureData{_sceneFramebuffer.OutputDescriptor});


	// Draw Scene
	{
		// Convert scene to render primitives
		SceneRendererPrimitives scene = {};
		for (const auto& entity : _scene.EntitiesView())
		{
			if (entity->Renderable.has_value())
			{
				for (auto&& submesh : entity->Renderable->GetSubmeshes())
				{
					Material* mat = _scene.GetMaterial(submesh.MatId);
					scene.Materials.emplace(mat);
					
					SceneRendererPrimitives::RenderableObject object = {
						submesh.Id,
						entity->Transform.GetMatrix(),
						*mat
					};
					scene.Objects.emplace_back(object);
				}
			}

			if (entity->Light.has_value())
			{
				scene.Lights.emplace_back(Converters::ToLight(*entity));
			}
		}

		// Get camera deets
		const auto& camera = _scene.GetCamera();
		scene.ViewPosition = camera.Position;
		scene.ViewMatrix = camera.GetViewMatrix();
		
		_sceneRenderer->Draw(imageIndex, commandBuffer, scene, GetRenderOptions());
	}
	


	// Draw Ui full screen
	{
		// Clear colour
		std::vector<VkClearValue> clearColors(2);
		clearColors[0].color = {0.f, 1.f, 1.f, 1.f};
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
			// Post Processing - TODO This should be in SceneRenderer.
			{
				vkCmdSetViewport(commandBuffer, 0, 1, &sceneViewport);
				vkCmdSetScissor(commandBuffer, 0, 1, &sceneRect);
				_postProcessPass.Draw(commandBuffer, imageIndex, _scene.GetRenderOptions());
			}

			// UI
			{
				vkCmdSetViewport(commandBuffer, 0, 1, &framebufferViewport);

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
	entity->Transform.SetPos({5, 5, 5});
	entity->Light = LightComponent{};
	entity->Light->Type = LightComponent::Types::point;
	entity->Light->Intensity = 250;

	ReplaceSelection(entity.get());
	_scene.AddEntity(std::move(entity));
}

void UiPresenter::CreateSphere()
{
	printf("CreateSphere()\n");

	auto matId = _library.CreateRandomMaterial();
	auto entity = _library.CreateSphere(matId);
	//entity->Action = std::make_unique<TurntableAction>(entity->Transform);


	ReplaceSelection(entity.get());
	_scene.AddEntity(std::move(entity));
}

void UiPresenter::CreateBlob()
{
	printf("CreateBlob()\n");

	auto matId = _library.CreateRandomMaterial();
	auto entity = _library.CreateBlob(matId);
	//entity->Action = std::make_unique<TurntableAction>(entity->Transform);


	ReplaceSelection(entity.get());
	_scene.AddEntity(std::move(entity));
}

void UiPresenter::CreateCube()
{
	printf("CreateCube()\n");

	auto matId = _library.CreateRandomMetalMaterial();
	auto entity = _library.CreateCube(matId);
	entity->Transform.SetScale(glm::vec3{0.9f});


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

void UiPresenter::SelectSubMesh(int index)
{
	_selectedSubMesh = index;

	// TODO Find and select the material associated with this submesh
	Entity* selection = _selection.size() == 1 ? *_selection.begin() : nullptr;
	if (!selection)
	{
		throw std::runtime_error("How are we commiting a material change when there's no valid selection?");
	}

	if (!selection->Renderable.has_value())
	{
		return; // There is no submesh to select
	}
	
	const auto& targetSubmesh = selection->Renderable->GetSubmeshes()[_selectedSubMesh];

	_selectedMaterialIndex = [&]() -> int
	{
		for (size_t m = 0; m < _materials.size(); m++)
		{
			if (_materials[m].second == targetSubmesh.MatId)
				return (int)m;
		}

		throw std::runtime_error("Couldn't find the material for the given submesh");
	}();
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

std::vector<std::string> UiPresenter::GetMaterials()
{	
	std::vector<std::string> matNames(_materials.size());
	
	for (size_t i = 0; i < _materials.size(); i++)
	{
		matNames[i] = _materials[i].first;
	}
	
	return matNames;
}

void UiPresenter::SelectMaterial(int i)
{
	_selectedMaterialIndex = i;

	if (_selectedMaterialIndex < 0)
		return; // deselected material

	// Apply to current submesh selection
	{
		Entity* selectedEntity = _selection.size() == 1 ? *_selection.begin() : nullptr;
		if (!selectedEntity || !selectedEntity->Renderable.has_value())
		{
			return; // No selection to apply the material to
		}

		auto& selectedSubmesh = selectedEntity->Renderable->GetSubmeshes()[_selectedSubMesh];
		const auto matId = _materials[_selectedMaterialIndex].second;

		selectedSubmesh.AssignMaterial(matId);
	}
}

std::optional<MaterialViewState> UiPresenter::GetMaterialState()
{
	if (_selectedMaterialIndex < 0)
		return std::nullopt; // no material selection
	
	const auto& [_, matId] = _materials[_selectedMaterialIndex];

	Material* mat = _scene.GetMaterial(matId);
	return MaterialViewState::CreateFrom(*mat);
}

void UiPresenter::CommitMaterialChanges(const MaterialViewState& state)
{
	auto* mat = _scene.GetMaterial(state.MaterialId);
	MaterialViewState::ToMaterial(state, mat, _scene);
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
