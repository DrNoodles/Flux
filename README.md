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
- Press 'F' to frame the current selection

Usage by example (5 min tutorial):
Load a Model
1. Clear the default scene: 'Object List > Delete All'.
2. Load 3D model from the 'Load > Object' menu button.
3. Browse to and open the included \Data\Assets\Models\Grapple\export\Grapple.gltf model. 
4. Select the Grapple Hook from the `Object List`.
5. The RHS panel controls the current selection. Play around with the `Transform` section to rotate the model. Double click a value to type, or simply drag the value.
6. In the RHS panel, note there are 3 submeshes detected. With the "Barrel_Mesh" selected, observe that the 'Base Color', 'Normals', 'Ambient Occlusion' and 'Emissive' texture maps were detected from the file. Now lets finish loading textures.
7. Change Metallic from `Value` to `Texture`. Browse to and load Data\Assets\Models\Grapple\export\Barrel_ORM.png. This texture has the AO, Roughness and Metalness packed into the RGB channels, respectively. Set the metalness channel to Blue to correlate to the texture's metalness channel.
8. Change Roughness from `Value` to `Texture`. Browse to and load Data\Assets\Models\Grapple\export\Barrel_ORM.png. Set the channel to Green which correlates to the texture's roughness channel.
9. Increase the 'Emissive' 'Intensity' to 5 to see it glow. Oooo shiny.
10. Repeat steps 6-9 for the Hook_Mesh and Stock_Mesh to complete the material, or skip ahead to play with lighting.

Light the scene
1. Experiment with the Image Based Lighting LHS panel to select an IBL, change its 'Rotation' and 'Intensity' so it's pleasing.
2. In the 'Create' LHS panel, add a Point Light. (Max 8 lights)
3. The new light is selected by default, but it can be reselected in the 'Object List' if necessary.
   (There is no visual of the light in scene yet, sorry!)
4. By default the light is inside the model, so lets reposition the light using the `Transform` widget on the RHS and tweak the light 'Color' and 'Intensity'.
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
