FROM ubuntu:22.04

# development environment dependencies
RUN apt-get update -y && apt-get upgrade -y
# essential dependencies
RUN apt-get install -y \
    build-essential m4 scons python3-dev
# optional dependencies
RUN apt-get install -y \
    git zlib1g zlib1g-dev libprotobuf-dev protobuf-compiler libprotoc-dev \
    libgoogle-perftools-dev doxygen libboost-all-dev libhdf5-serial-dev \
    libpng-dev libelf-dev pkg-config pip python3-pydot python3-venv black
# python packages
RUN pip install mypy pre-commit

# utilities
RUN apt-get install -y \
    bash nano vim valgrind dos2unix

# setup a home directory to work in
RUN mkdir /home/gem5
WORKDIR /home/gem5

# no copy as everything is mounted as a volume
