### Build in Docker

TARGET: valgrind-base

CTN_USER ?= ${USER}
IMG_NS ?= zhoujd

BUILD_ARG += ## --rm --no-cache
BUILD_ARG += --build-arg USER_NAME=$(CTN_USER)

Valgrind_BASE_TAG := $(IMG_NS)/valgrind:base
Valgrind_LATEST_TAG := $(IMG_NS)/valgrind:latest

.PHONY: all
all: $(TARGET)

valgrind-base: valgrind.dockerfile
	docker build $(BUILD_ARG) -t $(Valgrind_BASE_TAG) -f $< .
	docker tag $(Valgrind_BASE_TAG) $(Valgrind_LATEST_TAG)

.PHONY: clean
clean:
	-docker rmi $(Valgrind_BASE_TAG) $(Valgrind_LATEST_TAG)

.PHONY: help
help:
	@echo "Usage:"
	@echo "$$ make"
	@echo "$$ make CTN_USER=zach"
	@echo "$$ make clean"
	@echo "$$ make help"
	@echo "$$ make valgrind-base"
