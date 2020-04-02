#pragma once

#include "IViewportViewDelegate.h"
#include "Renderer/Renderer.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ViewportView
{
private:
	// Dependencies
	IViewportViewDelegate* _delegate = nullptr;
	Renderer& _renderer;

public:
	ViewportView(IViewportViewDelegate* delegate, Renderer& renderer) :
		_delegate(delegate),
		_renderer(renderer)
	{
	}
	
	void Draw();

};
