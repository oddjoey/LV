"%VULKAN_SDK%/Bin/glslc.exe" shader.vert -o ../Shaders/Compiled/vert.spv
"%VULKAN_SDK%/Bin/glslc.exe" shader.frag -o ../Shaders/Compiled/frag.spv
"%VULKAN_SDK%/Bin/glslc.exe" -DMOTION_VECTORS shader.vert -o ../Shaders/Compiled/vert_mv.spv
"%VULKAN_SDK%/Bin/glslc.exe" -DMOTION_VECTORS shader.frag -o ../Shaders/Compiled/frag_mv.spv
"%VULKAN_SDK%/Bin/glslc.exe" shadow.vert -o ../Shaders/Compiled/shadow.spv
"%VULKAN_SDK%/Bin/glslc.exe" composite.vert -o ../Shaders/Compiled/composite_vert.spv
"%VULKAN_SDK%/Bin/glslc.exe" composite.frag -o ../Shaders/Compiled/composite_frag.spv
pause
