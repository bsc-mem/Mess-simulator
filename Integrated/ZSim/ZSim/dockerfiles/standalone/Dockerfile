FROM ubuntu:14.04 as build

ENV PINPATH="/home/pin/"

RUN apt-get clean && apt-get update && apt-get install -y git \
g++ \
libconfig++-dev \
libhdf5-dev \
libelf-dev \
scons

COPY --from=pin /home/pin/ /home/pin/

RUN cd /home && git clone -b linux-4 https://github.com/shavvn/zsim.git \
&& cd /home/zsim && scons -j8 \
&& cd /home/zsim/build/opt/ && rm *.os *.o *.d *.ot

FROM ubuntu:14.04

RUN apt-get clean && apt-get update && apt-get install -y \
libconfig++-dev \
libhdf5-dev \
libelf-dev

COPY --from=pin /home/pin/ /home/pin/
COPY --from=build /home/zsim/ /home/zsim/

ENTRYPOINT [ "/home/zsim/build/opt/zsim" ]

# run this image with --security-opt seccomp=unconfined so that some syscalls
# actually works