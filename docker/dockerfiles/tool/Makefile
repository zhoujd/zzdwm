### Build in Docker

TARGET: valgrind-base wine-base

CTN_USER ?= ${USER}
IMG_NS ?= zhoujd

BUILD_ARG += ## --rm --no-cache
BUILD_ARG += --build-arg USER_NAME=$(CTN_USER)

Valgrind_BASE_TAG := $(IMG_NS)/valgrind:base
Valgrind_LATEST_TAG := $(IMG_NS)/valgrind:latest

Wine_BASE_TAG := $(IMG_NS)/wine:base
Wine_LATEST_TAG := $(IMG_NS)/wine:latest

.PHONY: all
all: $(TARGET)

valgrind-base: valgrind.dockerfile
	docker build $(BUILD_ARG) -t $(Valgrind_BASE_TAG) -f $< .
	docker tag $(Valgrind_BASE_TAG) $(Valgrind_LATEST_TAG)

wine-base: wine.dockerfile
	docker build $(BUILD_ARG) -t $(Wine_BASE_TAG) -f $< .
	docker tag $(Wine_BASE_TAG) $(Wine_LATEST_TAG)

.PHONY: clean
clean:
	-docker rmi $(Valgrind_BASE_TAG) $(Valgrind_LATEST_TAG)

.PHONY: help
help:
	@echo "Usage:"
	@echo "$$ make"
	@echo "$$ make clean"
	@echo "$$ make help"
	@echo "$$ make valgrind-base"
	@echo "$$ make wine-base"
