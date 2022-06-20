# Docker image used for building the firmware.
# docker build . -t scsi2sd-build
# docker run --rm -v ${PWD}:/src scsi2sd-build make -C src -f Makefile.2021
FROM debian:bullseye-slim

RUN apt update && apt install -y gcc-arm-none-eabi make python3 python-is-python3