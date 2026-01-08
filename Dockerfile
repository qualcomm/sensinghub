FROM public.ecr.aws/lts/ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ARG USER
ARG USER_ID
ARG GROUP_ID

# Force apt "--no-install-recommends" to limit the image size
RUN echo 'APT::Install-Recommends   "0";'   >> /etc/apt/apt.conf.d/99local && \
    echo 'APT::Install-Suggests     "0";'   >> /etc/apt/apt.conf.d/99local && \
    echo 'APT::Get::Assume-Yes      "1";'   >> /etc/apt/apt.conf.d/99local

WORKDIR /tmp

COPY ./create_user.sh .
RUN bash -xe -- create_user.sh && rm -rf -- * && rm -rf -- /var/lib/apt/lists/*

# Set timezone to UTC
RUN TZ="Etc/UTC" \
    ln -snf /usr/share/zoneinfo/$TZ /etc/localtime \
    echo $TZ | sudo tee /etc/timezone

# Install required dependencies
RUN apt-get update && apt install -y apt-transport-https apt-utils fuseext2 \
	build-essential \
	chrpath \
	curl \
	cpio \
	debianutils \
	diffstat \
	file \
	gawk \
	gcc \
	gcc-multilib \
	git \
	gpg-agent \
	iputils-ping \
	locales \
	liblz4-tool \
	libsdl1.2-dev \
	openssh-client \
	python3 \
	python3-git \
	python3-pip \
	python3-pexpect \
	python3-software-properties \
	socat \
	software-properties-common \
	tar \
	texinfo \
	tmux \
	unzip \
	vim \
	wget \
	xterm \
	xz-utils \
	zstd \
	# --- Autotools & protobuf for SensingHub build ---
    autoconf \
    automake \
    libtool \
    pkg-config \
    protobuf-compiler \
    libprotobuf-dev \
    && rm -rf -- /var/lib/apt/lists/*

# Install Python packages
RUN pip install --no-cache-dir requests kas==4.7

# Set python default
RUN update-alternatives --install /usr/bin/python python /usr/bin/python3.10 1

# Ensure /bin/sh points to bash
RUN ln -sf /bin/bash /bin/sh

# Locale
RUN apt-get update && \
    apt-get install --no-install-recommends -y --allow-downgrades locales && \
    rm -rf /var/lib/apt/lists/* && \
    sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen && \
    echo 'LANG="en_US.UTF-8"' > /etc/default/locale && \
    dpkg-reconfigure --frontend=noninteractive locales && \
    update-locale LANG=en_US.UTF-8

ENV LC_ALL=en_US.UTF-8
ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US.UTF-8

USER "$USER"

WORKDIR "/home/$USER"