.PHONY: docker-image config build release run tests sysroot clean

docker-image:
	docker build -t native-gcc-musl .

config:
	docker run --rm -u $(shell id -u):$(shell id -g) -v "$(PWD)":"$(PWD)" -w "$(PWD)" native-gcc-musl cmake -B build -DCMAKE_TOOLCHAIN_FILE=linux_musl_toolchain.cmake -G "Ninja Multi-Config"
build:
	docker run --rm -u $(shell id -u):$(shell id -g) -v "$(PWD)":"$(PWD)" -w "$(PWD)" native-gcc-musl cmake --build build --config Debug

release:
	docker run --rm -u $(shell id -u):$(shell id -g) -v "$(PWD)":"$(PWD)" -w "$(PWD)" native-gcc-musl cmake --build build --config RelWithDebInfo --verbose

run: build
	docker run --rm -u $(shell id -u):$(shell id -g) -v "$(PWD)":"$(PWD)" -w "$(PWD)" -e LD_LIBRARY_PATH=/usr/local/x86_64-linux-musl/lib64:/usr/local/x86_64-linux-musl/lib  native-gcc-musl ./build/src/Debug/ocat ./src/main.cpp

tests: build
	./build/tests/Debug/unit_tests --gtest_color=yes

pack: build
	docker run --rm -u $(shell id -u):$(shell id -g) -v "$(PWD)":"$(PWD)" -w "$(PWD)/build" native-gcc-musl cpack . -C Debug

sysroot:
	docker run --rm -v "$(PWD)/sysroot":/out native-gcc-musl bash -c "rm -rf /out/* && cp -r /usr/local/x86_64-linux-musl/include /out/ && cp -r /usr/local/x86_64-linux-musl/lib /out/ && cp -r /usr/include /out/sys-include || true"

clean:
	rm -rf build
