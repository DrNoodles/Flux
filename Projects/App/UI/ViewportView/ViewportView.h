#pragma once

#include "IViewportViewDelegate.h"
#include <Renderer/SceneRenderer.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ViewportView
{
private:
	// Dependencies
	IViewportViewDelegate* _delegate = nullptr;
	SceneRenderer& _sceneRenderer;

public:
	ViewportView(IViewportViewDelegate* delegate, SceneRenderer& renderer) :
		_delegate(delegate),
		_sceneRenderer(renderer)
	{
	}
	
	void Draw();

};
