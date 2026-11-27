BUILD_DIR   := build
SANITIZERS  ?= ON

CMAKE_FLAGS := -G Ninja -DENABLE_SANITIZERS=$(SANITIZERS)

FORMAT_FILES := $(shell find src include -type f \( -name '*.c' -o -name '*.h' \) 2>/dev/null)

.PHONY: build run format check-format clean

build:
	@cmake -B $(BUILD_DIR) $(CMAKE_FLAGS)
	@cmake --build $(BUILD_DIR)

format:
	@clang-format -i $(FORMAT_FILES)

check-format:
	@clang-format --dry-run --Werror $(FORMAT_FILES)

clean:
	@rm -rf $(BUILD_DIR)