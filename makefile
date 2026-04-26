

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

clean:
	rm -rf build

install:
	@test -f build/flottywm || { echo "Error: build/flottywm not found. Run 'make build' first."; exit 1; }
	install -Dm755 build/flottywm $(DESTDIR)$(BINDIR)/flottywm

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/flottywm

build:
	mkdir build
	meson build
	ninja -C build

run:
	WLR_LOG=debug ./build/flottywm

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

	WLR_LOG=debug ./build/flottywm
