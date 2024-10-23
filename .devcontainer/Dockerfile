FROM ubuntu:24.04
LABEL version="1.0"
LABEL maintainer="perplexedpigmy@xmousse.com"
LABEL description="Ubuntu based gcc 13 docker image with OpenCV"

ARG USER_UID=1000
ARG USER_GID=$USER_UID
ARG USERNAME=lynx
ARG BIN_EXE_DIR=/home/${USERNAME}/.local/bin


ENV CXX=/usr/bin/g++
RUN \
  # Remove user $USER_UID if it already exists 
  if id -u $USER_UID &>/dev/null; then \
    REPLACED_USER=$(id -u $USER_UID -n); \ 
    echo replacing USER $REPLACED_USER with $USERNAME; \  
    userdel $REPLACED_USER; \
  fi \
  #
  # Add a new user a $USER_UID 
  && groupadd -f --gid $USER_GID $USERNAME \
  && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
  && usermod --shell /bin/bash $USERNAME \
  #
  # Persist history between different builds
  && SNIPPET="PROMPT_COMMAND='history -a' && export HISTFILE=/cmdhistory/.bash_history" \
  && mkdir /cmdhistory \
  && touch /cmdhistory/.bash_history \
  && chown -R $USERNAME /cmdhistory/.bash_history \
  && echo $SNIPPET >> /home/$USERNAME/.bashrc \
  #
  # Install required packages
  && apt-get -qq update \
  && apt-get -qq install -y \
     sudo \
     wget \ 
     git \
     libopencv-dev \
     gcc-14 \
     g++-14 \
     gdb \
     ninja-build \
     parallel \
     build-essential \
     doxygen \
     cmake \
     make \
     file \
     binutils \
     libssl-dev \
     libboost-system-dev \
     libcurl4-openssl-dev \
     zlib1g-dev \
  # 
  # Configure c++ alternatives 
  && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100 \
  && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100 \
  && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 90 \
  && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 90 \
  #
  # Insatll the bin utilit
  && sudo -i -u $USERNAME mkdir -p ${BIN_EXE_DIR} \
  && sudo -i -u $USERNAME wget -O /tmp/bin https://github.com/perplexedpigmy/bin/releases/download/v0.17.5/bin_0.17.5_linux_amd64 \
  && sudo -i -u $USERNAME chmod a+x /tmp/bin \
  && sudo -i -u $USERNAME bash -c "BIN_EXE_DIR=${BIN_EXE_DIR} /tmp/bin install github.com/perplexedpigmy/bin" \
  && sudo -i -u $USERNAME rm /tmp/bin \
  # 
  # Install useful tools
  && sudo -i -u $USERNAME bash -c "BIN_EXE_DIR=${BIN_EXE_DIR} ${BIN_EXE_DIR}/bin install github.com/Wilfred/difftastic" \
  && sudo -i -u $USERNAME bash -c "BIN_EXE_DIR=${BIN_EXE_DIR} ${BIN_EXE_DIR}/bin install github.com/casey/just -s just-1.36.0-x86_64-unknown-linux-musl.tar.gz:just" \
  && sudo -i -u $USERNAME bash -c "BIN_EXE_DIR=${BIN_EXE_DIR} ${BIN_EXE_DIR}/bin install github.com/ogham/exa -s exa-linux-x86_64-v0.10.1.zip:bin/exa" \
  && sudo -i -u $USERNAME bash -c "BIN_EXE_DIR=${BIN_EXE_DIR} ${BIN_EXE_DIR}/bin install github.com/sharkdp/bat -s bat-v0.24.0-x86_64-unknown-linux-gnu.tar.gz:bat-v0.24.0-x86_64-unknown-linux-gnu/bat" \
  && sudo -i -u $USERNAME bash -c "BIN_EXE_DIR=${BIN_EXE_DIR} ${BIN_EXE_DIR}/bin install github.com/mikefarah/yq  -s yq_linux_amd64" \
  && sudo -i -u $USERNAME bash -c "BIN_EXE_DIR=${BIN_EXE_DIR} ${BIN_EXE_DIR}/bin install github.com/direnv/direnv"  \
  && sudo -i -u $USERNAME bash -c "BIN_EXE_DIR=${BIN_EXE_DIR} ${BIN_EXE_DIR}/bin install github.com/BurntSushi/ripgrep -s ripgrep-14.1.1-x86_64-unknown-linux-musl.tar.gz:ripgrep-14.1.1-x86_64-unknown-linux-musl/rg" \
  #
  # Add sudo permissions to user
  && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
  && chmod 0440 /etc/sudoers.d/$USERNAME \
  #
  # Cleanups
  && apt-get clean \
  && rm -rf /var/lib/apt/lists/* 

USER $USERNAME
