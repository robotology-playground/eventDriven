# base image
FROM ubuntu:focal

ENV DEBIAN_FRONTEND=noninteractive

ARG CODE_DIR=/usr/local/src

#basic environment
RUN apt update && apt install -y \
    ca-certificates \
    build-essential \
    git \
    cmake \
    cmake-curses-gui \
    libace-dev \
    libassimp-dev \
    libglew-dev \
    libglfw3-dev \
    libglm-dev \
    libeigen3-dev

# Suggested dependencies for YARP
RUN apt update && apt install -y \
    qtbase5-dev qtdeclarative5-dev qtmultimedia5-dev \
    qml-module-qtquick2 qml-module-qtquick-window2 \
    qml-module-qtmultimedia qml-module-qtquick-dialogs \
    qml-module-qtquick-controls qml-module-qt-labs-folderlistmodel \
    qml-module-qt-labs-settings \
    libqcustomplot-dev \
    libgraphviz-dev \
    libjpeg-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-libav

# Add metavision-sdk in sources.list
RUN echo "deb [arch=amd64 trusted=yes] https://apt.prophesee.ai/dists/public/b4b3528d/ubuntu focal sdk" >> /etc/apt/sources.list &&\
    apt update

RUN apt install -y \
    libcanberra-gtk-module \
    mesa-utils \
    ffmpeg \
    libboost-program-options-dev \
    libopencv-dev \
    metavision-sdk

# YCM
ARG YCM_VERSION=v0.15.2
RUN cd $CODE_DIR && \
    git clone --depth 1 --branch $YCM_VERSION https://github.com/robotology/ycm.git && \
    cd ycm && \
    mkdir build && cd build && \
    cmake .. && \
    make -j `nproc` install

# YARP
ARG YARP_VERSION=v3.8.0
RUN cd $CODE_DIR && \
    git clone --depth 1 --branch $YARP_VERSION https://github.com/robotology/yarp.git &&\
    cd yarp &&\
    mkdir build && cd build &&\
    cmake .. &&\
    make -j `nproc` install

EXPOSE 10000/tcp 10000/udp
RUN yarp check


# event-driven
ARG ED_VERSION=master
RUN cd $CODE_DIR &&\
    git clone --depth 1 --branch $ED_VERSION https://github.com/robotology/event-driven.git &&\
    cd event-driven &&\
    mkdir build && cd build &&\
    cmake .. &&\
    make -j `nproc` install

# Add User ID and Group ID
ARG UNAME=testuser
ARG UID=1000
ARG GID=1000
RUN groupadd -g $GID -o $UNAME
RUN useradd -m -u $UID -g $GID -o -s /bin/bash $UNAME

# Change user and working directory
USER $UNAME
WORKDIR /home/${UNAME}