@echo off

mkdir %CD%\\Bin

set outputdir=%CD%\Bin
set shaderdir=%CD%\Projects\Renderer\Source\Shaders
set compiler=%CD%\External\vulkan\Bin\glslc.exe

FOR %%G IN (.vert, .frag) DO FORFILES -p "%shaderdir%" -m *%%G /c "CMD /c %compiler% %shaderdir%\\@FILE -o %outputdir%\\@FILE.spv"
