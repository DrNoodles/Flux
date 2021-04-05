#pragma once

#include "IViewportViewDelegate.h"
//#include <Renderer/HighLevel/ForwardRenderer.h>

class ForwardRenderer;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ViewportView
{
private:
	// Dependencies
	IViewportViewDelegate* _delegate = nullptr;
	ForwardRenderer* _renderer = nullptr;

public:
	ViewportView() = default;
	ViewportView(IViewportViewDelegate* delegate, ForwardRenderer* renderer) :
		_delegate(delegate),
		_renderer(renderer)
	{
	}
	
	void Draw();

};
