#pragma once

#include "PropsView.h"
#include "TransformVm.h"
#include "RenderableVm.h"
#include "LightVm.h"

#include "App/Entity/Entity.h"
#include "Renderer/TextureResource.h"


// Purpose of the class is to take app state, prepare it for view (via view models), create view and inject the view models.
// It's also responsible for converting view changes into state changes.
class PropsPresenter
{
public:
	PropsPresenter() = default;

	// Precondition: selection must NOT be null if selectionCount = 1
	void Draw(int selectionCount, Entity* selection, 
		const std::vector<TextureResource*>& textures, 
		const std::vector<MeshResource*>& models/*,
		ResourceManager* res*/)
	{
		if (selectionCount != 1)
		{
			// Reset it all
			_selectionId = -1;
			_tvm = TransformVm{};
			_rvm = std::nullopt;
			_lvm = std::nullopt;
		}
		else if (selection->Id != _selectionId)
		{
			// New selection
			_selectionId = selection->Id;
			_tvm = TransformVm{ &selection->Transform };

			_lvm = selection->Light.has_value()
				? std::optional(LightVm{ &selection->Light.value() })
				: std::nullopt;

			_rvm = selection->Renderable.has_value()
				? std::nullopt//std::optional(RenderableVm{ res, &selection->Renderable.value(), &textures, &models })
				: std::nullopt;
		}
		else
		{
			// Same selection as last frame
		}

		_view.DrawUI(selectionCount, _tvm, _rvm, _lvm);
	}

private:
	int _selectionId = -1;
	TransformVm _tvm{}; // TODO Make optional and remove default constructor
	std::optional<RenderableVm> _rvm = std::nullopt;
	std::optional<LightVm> _lvm = std::nullopt; 
	PropsView _view{};
};