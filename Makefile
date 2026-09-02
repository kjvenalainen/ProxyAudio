GTEST_COLOR ?= yes
export GTEST_COLOR

CMAKE ?= cmake
CMAKE_ARGS ?= -Wno-dev

NUM_CPU ?= $(shell sysctl -n hw.logicalcpu 2>/dev/null || echo 1)

CODESIGN_ID ?= $(shell security find-identity -v 2>/dev/null | \
	grep 'Apple Development' | head -1 | awk '{print $$2}')

all: release

# Release build
release_cmake:
	mkdir -p build/ProxyAudio/Release
	cd build/ProxyAudio/Release && $(CMAKE) $(CMAKE_ARGS) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=/ \
		-DCODESIGN_ID=$(CODESIGN_ID) \
		../../../src

release: release_cmake
	cd build/ProxyAudio/Release && make -j$(NUM_CPU)
	python3 script/merge_compile_commands.py compile_commands.json \
		build/ProxyAudio/Release/compile_commands.json \
		build/ProxyAudio/Release/libASPL-build/compile_commands.json
	python3 script/fix_compile_commands_paths.py compile_commands.json .
	bash script/writeLatestBuildLink.sh Release

# Debug build
debug_cmake:
	mkdir -p build/ProxyAudio/Debug
	cd build/ProxyAudio/Debug && $(CMAKE) $(CMAKE_ARGS) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCODESIGN_ID=$(CODESIGN_ID) \
		../../../src

debug: debug_cmake
	cd build/ProxyAudio/Debug && make -j$(NUM_CPU)
	python3 script/merge_compile_commands.py compile_commands.json \
		build/ProxyAudio/Debug/compile_commands.json \
		build/ProxyAudio/Debug/libASPL-build/compile_commands.json
	python3 script/fix_compile_commands_paths.py compile_commands.json .
	bash script/writeLatestBuildLink.sh Debug

# Deterministic unit tests. Building this target does not build, sign, install,
# or load the HAL driver bundle.
.PHONY: test test_cmake
test_cmake:
	mkdir -p build/ProxyAudio/Test
	cd build/ProxyAudio/Test && $(CMAKE) $(CMAKE_ARGS) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DBUILD_TESTING=ON \
		-DCODESIGN_ID= \
		../../../src

test: test_cmake
	$(CMAKE) --build build/ProxyAudio/Test --target proxy_audio_tests -j$(NUM_CPU)
	ctest --test-dir build/ProxyAudio/Test --output-on-failure --verbose

# Clean build artifacts
clean:
	rm -rf build
	rm -rf html
	rm -f compile_commands.json

# Install using the latest build symlink
.PHONY: install
install:
	bash script/install.sh

# Uninstall
.PHONY: uninstall
uninstall:
	bash script/uninstall.sh

# Manager app
APP_NAME = ProxyAudioManager
APP_BUNDLE = build/$(APP_NAME).app
APP_SOURCES = $(wildcard app/*.swift)
SWIFTC_FLAGS = -sdk $(shell xcrun --show-sdk-path) \
	-target $(shell uname -m)-apple-macos13.0 \
	-framework SwiftUI -framework CoreAudio

app:
	@test -L build/latest || { echo "Error: build/latest not found. Run 'make release' or 'make debug' first."; exit 1; }
	mkdir -p "$(APP_BUNDLE)/Contents/MacOS"
	mkdir -p "$(APP_BUNDLE)/Contents/Resources"
	swiftc $(SWIFTC_FLAGS) -o "$(APP_BUNDLE)/Contents/MacOS/$(APP_NAME)" $(APP_SOURCES)
	cp app/Info.plist "$(APP_BUNDLE)/Contents/Info.plist"
	cp -RL build/latest "$(APP_BUNDLE)/Contents/Resources/ProxyAudio.driver"
ifneq ($(CODESIGN_ID),)
	codesign --force --deep -s "$(CODESIGN_ID)" "$(APP_BUNDLE)"
endif
	@echo "Built $(APP_BUNDLE)"

app-clean:
	rm -rf "$(APP_BUNDLE)"

# Code formatting
fmt:
	find . -type f -name '*.[ch]pp' -not -name '*.g.*' \
		| xargs clang-format --verbose -i

# Markdown utilities
md:
	markdown-toc --maxdepth 2 --bullets=- -i README.md
	md-authors --format classic --append AUTHORS.md
