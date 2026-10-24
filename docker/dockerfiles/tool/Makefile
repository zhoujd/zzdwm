### Build in Docker

TARGET: valgrind-base wine-base mingw-base docs-base autossh-base

CTN_USER ?= ${USER}
IMG_NS ?= zhoujd

BUILD_ARG += ## --rm --no-cache
BUILD_ARG += --build-arg USER_NAME=$(CTN_USER)

Valgrind_BASE_TAG := $(IMG_NS)/valgrind:base
Valgrind_LATEST_TAG := $(IMG_NS)/valgrind:latest

Wine_BASE_TAG := $(IMG_NS)/wine:base
Wine_LATEST_TAG := $(IMG_NS)/wine:latest

Mingw_BASE_TAG := $(IMG_NS)/mingw:base
Mingw_LATEST_TAG := $(IMG_NS)/mingw:latest

Docs_BASE_TAG := $(IMG_NS)/docs:base
Docs_LATEST_TAG := $(IMG_NS)/docs:latest

Autossh_BASE_TAG := $(IMG_NS)/autossh:base
Autossh_LATEST_TAG := $(IMG_NS)/autossh:latest

.PHONY: all
all: $(TARGET)

valgrind-base: valgrind.dockerfile
	docker build $(BUILD_ARG) -t $(Valgrind_BASE_TAG) -f valgrind.dockerfile .
	docker tag $(Valgrind_BASE_TAG) $(Valgrind_LATEST_TAG)

wine-base: wine.dockerfile
	docker build $(BUILD_ARG) -t $(Wine_BASE_TAG) -f wine.dockerfile .
	docker tag $(Wine_BASE_TAG) $(Wine_LATEST_TAG)

mingw-base: mingw.dockerfile
	docker build $(BUILD_ARG) -t $(Mingw_BASE_TAG) -f mingw.dockerfile .
	docker tag $(Mingw_BASE_TAG) $(Mingw_LATEST_TAG)

docs-base: docs.dockerfile
	docker build $(BUILD_ARG) -t $(Docs_BASE_TAG) -f docs.dockerfile .
	docker tag $(Docs_BASE_TAG) $(Docs_LATEST_TAG)

autossh-base: autossh.dockerfile autossh-entrypoint.sh bin/autossh
	docker build $(BUILD_ARG) -t $(Autossh_BASE_TAG) -f autossh.dockerfile .
	docker tag $(Autossh_BASE_TAG) $(Autossh_LATEST_TAG)

.PHONY: clean
clean:
	-docker rmi $(Valgrind_BASE_TAG) $(Valgrind_LATEST_TAG)
	-docker rmi $(Mingw_BASE_TAG) $(Mingw_LATEST_TAG)
	-docker rmi $(Docs_BASE_TAG) $(Docs_LATEST_TAG)
	-docker rmi $(Autossh_BASE_TAG) $(Autossh_LATEST_TAG)

.PHONY: help
help:
	@echo "Usage:"
	@echo "$$ make"
	@echo "$$ make clean"
	@echo "$$ make help"
	@echo "$$ make valgrind-base"
	@echo "$$ make wine-base"
	@echo "$$ make mingw-base"
	@echo "$$ make docs-base"
	@echo "$$ make autossh-base"
