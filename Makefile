VULKAN_SDK ?=

ifneq ($(VULKAN_SDK),)
export VULKAN_SDK
export VK_ICD_FILENAMES ?= $(VULKAN_SDK)/share/vulkan/icd.d/MoltenVK_icd.json
export VK_LAYER_PATH ?= $(VULKAN_SDK)/share/vulkan/explicit_layer.d
export DYLD_LIBRARY_PATH ?= $(VULKAN_SDK)/lib
endif

SHADERS ?= col tex text

all: deps shaders configure build run

deps:
	git submodule update --init --recursive

configure:
	cmake -S . -B cmake-build-debug

build:
	cmake --build cmake-build-debug --target vulkan

run:
	./cmake-build-debug/vulkan

shaders:
	for s in $(SHADERS); do \
		$(MAKE) -C Engine/shad NAME=$$s; \
	done
