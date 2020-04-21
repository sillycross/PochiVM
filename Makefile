fake:
	@echo "Please run either 'make debug' or 'make release'."

debug: build/debug/Makefile
	cd build/debug; \
	make generated.dependency DBG=1; \
	make main DBG=1
	cp build/debug/main main
	
build/debug/Makefile: Makefile.real
	mkdir -p build
	mkdir -p build/debug
	cp Makefile.real build/debug/Makefile
	
release: build/release/Makefile
	cd build/release; \
	make generated.dependency; \
	make main
	cp build/release/main main
	
build/release/Makefile: Makefile.real
	mkdir -p build
	mkdir -p build/release
	cp Makefile.real build/release/Makefile

clean:
	rm -rf build
	rm main

