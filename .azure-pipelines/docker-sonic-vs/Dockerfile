FROM docker-sonic-vs

ARG docker_container_name

ADD ["debs", "/debs"]

RUN dpkg --purge python-swsscommon
RUN dpkg --purge swss
RUN dpkg --purge libsairedis
RUN dpkg --purge libswsscommon
RUN dpkg --purge libsaimetadata
RUN dpkg --purge libsaivs
RUN dpkg --purge syncd-vs

RUN dpkg -i /debs/*.deb || apt-get install -f -y
