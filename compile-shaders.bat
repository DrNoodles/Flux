@echo off

mkdir %CD%\\Bin

set outputdir=%CD%\Bin
set compiler=%CD%\External\vulkan\Bin\glslc.exe

set shaderdir=%CD%\Projects\Renderer\Source\Shaders
FOR %%G IN (.vert, .frag) DO FORFILES -p "%shaderdir%" -m *%%G /s /c "CMD /c %compiler% %shaderdir%\\@RELPATH -o %outputdir%\\@FILE.spv"
