FROM ubuntu:18.04
LABEL maintainer="rkfg <rkfg@rkfg.me>"
RUN apt-get update && apt-get install -y --no-install-recommends libboost-filesystem-dev libtorrent-rasterbar-dev git g++ ca-certificates \
libfuse-dev libcurl4-openssl-dev ninja-build python3-pip python3-setuptools python3-wheel fuse sudo && \
pip3 install meson && git clone https://github.com/rkfg/btfsng.git && cd btfsng && meson build && cd build && ninja &&\
echo 'user_allow_other' > /etc/fuse.conf