<h1>Cauldron SDF Fluid Rendering</h1>

![Fluid](/docs/media/fluid.png)

This is a small test for rendering SDF fluids (currently only lava) using Cauldron as the rendering engine. It uses tile based particle rendering to increase performance and limit the worst case rendering time.

<h2>Building</h2>

A convenience batch file `BuildFluidDX12.bat` is provided to generate a Visual Studio solution file. Note that only DirectX12 is supported for now. Vulkan may or may not work.

This batch file should be run using the Developer Command prompt for VS. The generated solution can be built like normal.

You will need the relevant media files for the project. They can be downloaded using the `UpdateMedia.bat` file, or sourced from the Release file.

<h2>Known issues</h2>

- Sphere size can be underestimated, possibly resulting in tears between tiles.
- The maximum tile sphere count can easily be reached. There are currently no precautions regarding this other than sorting spheres front to back.
- Colors are a bit flat. This could be alleviated by using textures optimized for triplanar mapping as well as taking into account emissive and other detail channels.
- No shadows. SDF is currently only rendering into the GBuffer and doesn't cast any shadows.
- Performance. Currently not all tricks to increase SDF performance are implemented.

<h2>License</h2>

All code is using the same license as the AMD FidelityFX SDK, the MIT license.

For more information on the license terms please refer to [license](/sdk/LICENSE.txt).
