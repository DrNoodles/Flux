```

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
```
![](Data/Assets/FluxApp.jpg)

[Download a Binary](https://computermen.itch.io/flux-renderer) or continue here to build from source...

```

Requirements:
- Vulkan SDK - https://vulkan.lunarg.com/sdk/home
- Premake5 - https://premake.github.io/
- Latest Visual C++ Redist found in "External/VC_redist.x64.exe"
- Latest GPU drivers with Vulkan 1.1 support

Build and run:
1. create-build-files.bat
2. compile-shaders.bat
3. open Build/Vulkan.sln in VS2019 and compile
4. run Bin/Flux_CONFIG.exe

Camera Controls:
- Drag LMB to arc
- Drag MMB to pan
- Drag RMB to zoom

Usage by example:
Load a Model
1. Load 3D model from the `Load` > `Object` menu button.
2. Browse to the included \Data\Assets\Models\Railgun\q2railgun.gltf model.
3. Select the railgun from the `Outliner`.
4. The RHS panel controls the current selection. Play around with the `Transform` section to position the model.
5. In the RHS panel, note that the Base Color, Normals and Ambient Occlusion maps were detected from the file. Now lets finish loading textures
6. Change Metallic from `Value` to `Texture`. Browse to and load Data\Assets\Models\Railgun\ORM.png. This texture has the AO, Roughness and Metalness packed into the RGB channels, respectively. Set the metallic channel to Blue to correlate to metalness channel of the texture.
7. Change Roughness from `Value` to `Texture`. Browse to and load Data\Assets\Models\Railgun\ORM.png. Set the channel to Green to correlate to roughness channel of the texture.

Light the scene
1. Experiment with the Image Based Lighting LHS panel to select an IBL and position it so it's pleasing.
2. In the 'Create' LHS panel, add a Point Light. (Max 8 lights)
3. The new light is selected by default, but it can be reselected in the Outliner if necessary.
   (There is no visual of the light in scene yet, sorry!)
4. Position the light using the `Transform` widget on the RHS and tweak the light Colour and Intensity on the RHS.
5. Optionally: There is also a Directional Light with no falloff. This is difficult to position unless thinking like a coder. The Position defines the direction vector. Eg Pos(0,-1,0) will direct the light downwards.

MVP Items Remaining:
- Selection picking in 3D
- Selection outlines
- Selection transformation gizmo
- Wireframe object for lights
- Material load/save
- Scene load/save
- Render and save high quality image at custom resolution
```
