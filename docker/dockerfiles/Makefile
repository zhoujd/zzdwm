### Build in Docker

.PHONY: all
all: alpine-base void-linux-base

CTN_USER ?= ${USER}
IMG_NS ?= zhoujd

BUILD_ARG += ## --rm --no-cache

Alpine_BASE_TAG := $(IMG_NS)/alpine:base
Alpine_LATEST_TAG := $(IMG_NS)/alpine:latest
Void_Linux_BASE_TAG := $(IMG_NS)/void-linux:base
Void_Linux_LATEST_TAG := $(IMG_NS)/void-linux:latest

alpine-base: alpine.dockerfile
	docker build $(BUILD_ARG) -t $(Alpine_BASE_TAG) -f $< .
	docker tag $(Alpine_BASE_TAG) $(Alpine_LATEST_TAG)

void-linux-base: void-linux.dockerfile
	docker build $(BUILD_ARG) -t $(Void_Linux_BASE_TAG) -f $< .
	docker tag $(Void_Linux_BASE_TAG) $(Void_Linux_LATEST_TAG)

.PHONY: clean
clean:
	-docker rmi $(Alpine_BASE_TAG)
	-docker rmi $(Void_Linux_BASE_TAG)

.PHONY: help
help:
	@echo "Usage:"
	@echo "$$ make"
	@echo "$$ make CTN_USER=zhoujd"
	@echo "$$ make clean"
	@echo "$$ make help"
	@echo "$$ make alpine-base"
	@echo "$$ make void-linux-base"
