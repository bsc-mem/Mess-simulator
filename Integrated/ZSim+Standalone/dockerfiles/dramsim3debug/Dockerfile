FROM ubuntu:14.04 as prebuild

RUN apt-get clean && apt-get update && apt-get install -y \
g++ \
libconfig++-dev \
libhdf5-dev \
libelf-dev \
scons

COPY --from=angel7.ece.umd.edu:5000/pin /home/pin/ /home/pin/
COPY --from=angel7.ece.umd.edu:5000/dramsim3:ubuntu14_gcc /home/DRAMsim3/ /home/DRAMsim3/
COPY --from=angel7.ece.umd.edu:5000/dramsim2:ubuntu14 /home/DRAMSim2/ /home/DRAMSim2/

ENV DRAMSIMPATH="/home/DRAMSim2/"
ENV DRAMSIM3PATH="/home/DRAMsim3/"
ENV PINPATH="/home/pin/"

WORKDIR /home/zsim/

COPY src/ src/
COPY SConstruct .

RUN scons -j8 debug=1

ENTRYPOINT [ "/home/zsim/build/opt/zsim" ]

# run this image with --security-opt seccomp=unconfined so that some syscalls
# actually works