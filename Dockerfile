################
FROM ubuntu:16.04 AS dpainless_base
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt install -y openssh-server iproute2 openmpi-bin openmpi-common iputils-ping \
    && mkdir /var/run/sshd \
    && sed 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' -i /etc/pam.d/sshd \
    && setcap CAP_NET_BIND_SERVICE=+eip /usr/sbin/sshd \
    && useradd -ms /bin/bash dpainless \
    && chown -R dpainless /etc/ssh/ \
    && su - dpainless -c \
        'ssh-keygen -q -t rsa -f ~/.ssh/id_rsa -N "" \
        && cp ~/.ssh/id_rsa.pub ~/.ssh/authorized_keys \
        && cp /etc/ssh/sshd_config ~/.ssh/sshd_config \
        && sed -i "s/UsePrivilegeSeparation yes/UsePrivilegeSeparation no/g" ~/.ssh/sshd_config \
        && printf "Host *\n\tStrictHostKeyChecking no\n" >> ~/.ssh/config'
WORKDIR /home/dpainless
ENV NOTVISIBLE "in users profile"
RUN echo "export VISIBLE=now" >> /etc/profile
EXPOSE 22
################
FROM ubuntu:16.04 AS builder
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt install -y cmake build-essential zlib1g-dev libopenmpi-dev git wget unzip build-essential zlib1g-dev iproute2 cmake python python-pip build-essential gfortran wget curl
#ADD dpainlesssat dpainlesssat
ADD painless-v2 painless-v2
RUN cd painless-v2 && make -j4 && cd ..

################
FROM dpainless_base AS dpainless_liaison
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt install -y awscli python3 mpi
COPY --from=builder /painless-v2 /painless-v2
ADD make_combined_hostfile.py supervised-scripts/make_combined_hostfile.py
RUN chmod 755 supervised-scripts/make_combined_hostfile.py
ADD mpi-run.sh supervised-scripts/mpi-run.sh
USER dpainless
CMD ["/usr/sbin/sshd", "-D", "-f", "/home/dpainless/.ssh/sshd_config"]
CMD supervised-scripts/mpi-run.sh

