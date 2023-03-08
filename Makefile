.PHONY: image dev

USER=gem5
IMAGE=gem5-runahead:dev
CONTAINER_LABEL=gem5-devenv
MOUNT_HOST_DIR=${GEM5_DEV_DIR}
MOUNT_GUEST_DIR=/home/$(USER)/gem5-runahead
DEV_SHELL=/bin/bash

DOCKER_RUN_ARGS=-ia
DOCKER_RUN_CMD=start
DOCKER_CMD=

# reuse existing dev containers or create one if none exist
CONTAINER=$(shell docker ps -a -f label=$(CONTAINER_LABEL) --format "{{.ID}}")
ifeq ($(CONTAINER),)
	DOCKER_RUN_CMD=run
	DOCKER_RUN_ARGS=-it -v $(MOUNT_HOST_DIR):$(MOUNT_GUEST_DIR) -l $(CONTAINER_LABEL)
	CONTAINER=$(IMAGE)
	DOCKER_CMD=$(DEV_SHELL)
endif

# build the dev environment
image:
	docker build -t $(IMAGE) .

# run the dev environment
dev:
	docker $(DOCKER_RUN_CMD) $(DOCKER_RUN_ARGS) $(CONTAINER) $(DOCKER_CMD)

### for use inside a dev container ###
.PHONY: gem5

BUILD_THREADS=4
# ARM/NULL/MIPS/POWER/RISCV/SPARC/X86
# case sensitive
ISA=X86
# debug/opt/fast
BUILD_VARIANT=opt

GEM5_EXTRAS=gem5-extensions/

gem5:
	cd gem5
	scons EXTRAS=$(GEM5_EXTRAS) build/$(ISA)/gem5.$(BUILD_VARIANT) -j $(BUILD_THREADS)
