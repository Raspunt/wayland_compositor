

clean:
	rm -rf build

build:
	mkdir build
	meson build
	ninja -C build

run:
	WLR_LOG=debug ./build/myexe

rebuild:
	rm -rf build

	mkdir build
	meson build
	ninja -C build

rebuild_and_run:
	rm -rf build

	mkdir build
	meson build
	ninja -C build

	WLR_LOG=debug ./build/myexe
