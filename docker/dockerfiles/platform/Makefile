### Build in Docker

TARGET: wine-base

CTN_USER ?= ${USER}
IMG_NS ?= zhoujd

BUILD_ARG += ## --rm --no-cache
BUILD_ARG += --build-arg USER_NAME=$(CTN_USER)

Wine_BASE_TAG := $(IMG_NS)/wine:base
Wine_LATEST_TAG := $(IMG_NS)/wine:latest

.PHONY: all
all: $(TARGET)

wine-base: wine.dockerfile
	docker build $(BUILD_ARG) -t $(Wine_BASE_TAG) -f $< .
	docker tag $(Wine_BASE_TAG) $(Wine_LATEST_TAG)

.PHONY: clean
clean:
	-docker rmi $(Wine_BASE_TAG) $(Wine_LATEST_TAG)

.PHONY: help
help:
	@echo "Usage:"
	@echo "$$ make"
	@echo "$$ make CTN_USER=zach"
	@echo "$$ make clean"
	@echo "$$ make help"
	@echo "$$ make wine-base"
