

clean:
	rm -rf build

build:
	mkdir build
	meson build
	ninja -C build

run:
	build/myexe

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

	build/myexe
