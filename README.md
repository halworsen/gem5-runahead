# gem5 Runahead
Implementations of Runahead Execution in the gem5 simulator

## Setup
Clone the repository with

```bash
git clone --recurse-submodules https://github.com/halworsen/gem5-runahead.git
```

This will additionally clone [v22.1.0.0](https://gem5.googlesource.com/public/gem5/+/refs/tags/v22.1.0.0) of gem5.

If you run Windows or just want to use a container:

* Run `make image` to build a container image with all required and optional
  development packages + some utilities.
* Set the `GEM5_DEV_DIR` env variable to the path to this repository. Use `make run` to
  create/start a dev container with the `GEM5_DEV_DIR` mounted in the home directory.
* Navigate to the mounted directory and build gem5 with the runahead extensions using `make gem5`.
* Run gem5 with `make run`.

If you get CRLF-related errors, `dos2unix` can take care of that.
