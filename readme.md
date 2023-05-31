## Quake 2 Experiments

A pile of varied Quake 2 experiements, nothing particularly focused, but I need to put them somewhere. 

### game

Quake 2's default game dll ported to C++. Doesn't use classes yet. Adds a few new debugging features, and a new CVAR for more accurate aiming.

### ref_vk

A new Vulkan renderer for Quake 2. Also written in C++ as a learning project, it adds a small amount of new features to somewhat enhance the game's graphics, without significantly going out of line with enhancements.

Note that there isn't a build system for shaders yet. I want to set something up eventually. 

I want to port this to Yamagi Quake 2 when it's more done. It already has a port of vkquake2's renderer, but I'd like to get my own working there, and it's much better than using the crusty original engine. 
