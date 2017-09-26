# Build
#   docker build . -t fwup_dev
# Run
#   docker run --rm --privileged -it -v myvolume:/workdir fwup_dev 
FROM travisci/ci-garnet:packer-1499451976

WORKDIR /tmp
USER root

RUN dpkg-divert --local --rename --add /sbin/initctl &&\
    ln -s /bin/true /sbin/initctl &&\
    ln -s /bin/true /etc/init.d/systemd-logind

ADD *.sh scripts/

RUN apt-get update &&\
    apt-get -y install ruby ruby-dev 

USER travis

ENV TRAVIS_OS_NAME=linux
# Select which mode to build the container for
#ENV MODE=static
#ENV MODE=dynamic
ENV MODE=windows
RUN ./scripts/ci_install_deps.sh

# Note Windows still needs binfmts setup to run fwup.exe
# sudo update-binfmts --enable wine
WORKDIR /workdir
ENTRYPOINT /bin/bash