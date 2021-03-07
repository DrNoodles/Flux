#pragma once

#include "IViewportViewDelegate.h"
//#include <Renderer/HighLevel/SceneRenderer.h>

class SceneRenderer;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ViewportView
{
private:
	// Dependencies
	IViewportViewDelegate* _delegate = nullptr;
	SceneRenderer* _sceneRenderer = nullptr;

public:
	ViewportView() = default;
	ViewportView(IViewportViewDelegate* delegate, SceneRenderer* renderer) :
		_delegate(delegate),
		_sceneRenderer(renderer)
	{
	}
	
	void Draw();

};
