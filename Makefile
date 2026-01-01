GTEST_COLOR ?= yes
export GTEST_COLOR

CMAKE ?= cmake
CMAKE_ARGS ?= -Wno-dev

NUM_CPU ?= $(shell sysctl -n hw.logicalcpu 2>/dev/null || echo 1)

CODESIGN_ID ?= $(shell security find-identity -v 2>/dev/null | \
	grep 'Apple Development' | head -1 | awk '{print $$2}')

all: proxyaudio_release_build

release_cmake:
	mkdir -p build/Release
	cd build/Release && $(CMAKE) $(CMAKE_ARGS) -DCMAKE_BUILD_TYPE=Release ../..

release_build: release_cmake
	cd build/Release && make -j$(NUM_CPU)

debug_cmake:
	mkdir -p build/Debug
	cd build/Debug && $(CMAKE) $(CMAKE_ARGS) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DENABLE_SANITIZERS=ON \
		-DBUILD_TESTING=ON \
		-DBUILD_DOCUMENTATION=ON \
		../..

debug_build: debug_cmake
	cd build/Debug && make -j$(NUM_CPU)

proxyaudio_release_cmake:
	mkdir -p build/ProxyAudio/Release
	cd build/ProxyAudio/Release && $(CMAKE) $(CMAKE_ARGS) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX=/ \
		-DCODESIGN_ID=$(CODESIGN_ID) \
		../../../src

proxyaudio_release_build: proxyaudio_release_cmake
	cd build/ProxyAudio/Release && make -j$(NUM_CPU)

proxyaudio_debug_cmake:
	mkdir -p build/ProxyAudio/Debug
	cd build/ProxyAudio/Debug && $(CMAKE) $(CMAKE_ARGS) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCODESIGN_ID=$(CODESIGN_ID) \
		../../../src

proxyaudio_debug_build: proxyaudio_debug_cmake
	cd build/ProxyAudio/Debug && make -j$(NUM_CPU)

.PHONY: test
test: debug_build
	cd build/Debug && make test ARGS="-V"

gen: debug_cmake
	cd build/Debug && make gen

.PHONY: install
install: proxyaudio_release_build
	cd build/ProxyAudio/Release && sudo make install

clean:
	rm -rf build
	rm -rf html
	rm -f compile_commands.json

proxyaudio_clean:
	rm -rf build/ProxyAudio

clobber: clean
	rm -f src/*.g.cpp

fmt:
	find -type f -name '*.[ch]pp' -not -name '*.g.*' \
		| xargs clang-format --verbose -i

md:
	markdown-toc --maxdepth 2 --bullets=- -i README.md
	md-authors --format classic --append AUTHORS.md
