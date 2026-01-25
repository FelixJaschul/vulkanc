all:
	export VULKAN_SDK="/Users/felixjaschul/VulkanSDK/1.4.335.1/macOS"
	export VK_ICD_FILENAMES="$VULKAN_SDK/share/vulkan/icd.d/MoltenVK_icd.json"
	export VK_LAYER_PATH="$VULKAN_SDK/share/vulkan/explicit_layer.d"
	export DYLD_LIBRARY_PATH="$VULKAN_SDK/lib"
	cmake --build cmake-build-debug --target vulkan
	./cmake-build-debug/vulkan