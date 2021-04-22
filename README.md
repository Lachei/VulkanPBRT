# VulkanPBRT
Vulkan physically based raytracer including denoising.

The GPU raytracer is based on Vulkan only, as well as for the denoising only the Vulkan Compute capabilities will be used.

For faster and easier development the [VulkanSceneGraph](https://github.com/vsg-dev/VulkanSceneGraph) library is used. It provides an easier interface to Vulkan, has resource management included and allows pipeline setup via a scene graph. Further GLSL like math classes are included.

# Compilation Notes
This project depends on the Vulkan library to be installed, so head over to the [Vulkan Website](https://vulkan.lunarg.com/sdk/home) and download the appropriate SDK.

Further it is required that [VulkanSceneGraph](https://github.com/vsg-dev/VulkanSceneGraph) library is installed.

Both libraries have to be set up so that they can be found by cmake via `find_package`. For details go to the links posted above and follow the install instructions on the websites.

The shaders for the projects are automatically compiled by cmake when the project is built. If you should change the source code of the shaders this compilation will automatically be re-run on the next build instruction.

To build the project, simply
```
cd VulkanPBRT
mkdir build
cd build
cmake ..
make -j 8
```
(the `-j 8` instruction for the `make` command enables multi threaded compilation)