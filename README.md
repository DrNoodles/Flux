

   ▄████████  ▄█       ███    █▄  ▀████    ▐████▀ 
  ███    ███ ███       ███    ███   ███▌   ████▀  
  ███    █▀  ███       ███    ███    ███  ▐███    
 ▄███▄▄▄     ███       ███    ███    ▀███▄███▀    
▀▀███▀▀▀     ███       ███    ███    ████▀██▄     
  ███        ███       ███    ███   ▐███  ▀███    
  ███        ███▌    ▄ ███    ███  ▄███     ███▄  
  ███        █████▄▄██ ████████▀  ████       ███▄ 
             ▀                                    

Flux is a real-time PBR asset renderer, written with Vulkan.


Requirements:
- Latest Visual C++ Redist found in "External/VC_redist.x64.exe"
- Latest GPU drivers with Vulkan 1.1 support

Build and run:
1. create-build-files.bat
2. compile-shaders.bat
3. open Build/Vulkan.sln in VS2019 and compile
4. run Bin/Flux_CONFIG.exe

Controls:
- 'x' loads a demo scene
- 'c' to change skybox
- Drag LMB to arc
- Drag MMB to pan
- Drag RMB to zoom
