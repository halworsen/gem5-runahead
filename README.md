# gem5 Runahead

Implementation of Runahead Execution in the gem5 simulator

Features:
 * A traditional runahead execution CPU model for gem5
 * Short and overlapping period runahead elimination
 * Runahead instruction stream filtering
 * 4 runahead exit policies
   * Eager exit
   * Minimum work delayed exit
   * No load left behind delayed exit
   * Dynamic delayed exit 

## Setup

Clone the repository with

```bash
git clone --recurse-submodules https://github.com/halworsen/gem5-runahead.git
```

This will additionally clone [v22.0.0.2](https://gem5.googlesource.com/public/gem5/+/refs/tags/v22.0.0.2) of gem5.

If you run Windows or just want to use a container, try the following. It's been a while since I used this myself, so things aren't guaranteed to be that smooth.

* Run `make image` to build a container image with all required and optional
  development packages + some utilities.
* Set the `GEM5_DEV_DIR` env variable to the path to this repository. Use `make run` to
  create/start a dev container with the `GEM5_DEV_DIR` mounted in the home directory.
* Navigate to the mounted directory and build gem5 with the runahead extensions using `make gem5`.
* Run gem5 with `make run`.

If you get CRLF-related errors, `dos2unix` can take care of that.

## Building gem5 with the runahead extension

Runahead is implemented as a new CPU model which is based on the O3CPU model. The runahead CPU
is structured as an extension of gem5 and can therefore be built by specifying `EXTRAS` when
building gem5 with scons.

To build gem5 with the runahead extension, follow
[the gem5 documentation on building gem5](https://www.gem5.org/documentation/general_docs/building)
to setup any dependencies. Optionally setup a Python virtual environment with the Scons build tool

```bash
# create the virtual environment
python -m venv venv
# activate the virtual environment
source venv/bin/activate
# install scons
python -m pip install -r requirements.txt
```

Then build gem5 by running

```bash
make gem5

# specific build variants
make gem5-debug
make gem5-opt
make gem5-fast

# without runahead extensions
make gem5-bare
```
