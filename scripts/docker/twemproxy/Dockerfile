FROM centos:centos7

ENV REDIS_MASTER redis_master:6379
ENV REDIS_SLAVE  redis_slave:6379

ENV REDIS_SHARD1 redis_shard1:6379
ENV REDIS_SHARD2 redis_shard2:6379
ENV REDIS_SHARD3 redis_shard3:6379

ENV MC_SHARD1 mc_shard1:11211
ENV MC_SHARD2 mc_shard2:11211

EXPOSE 32121
EXPOSE 32122
EXPOSE 32123

WORKDIR /opt
COPY nutcracker.tmpl /opt/nutcracker.tmpl
COPY render_nutcracker_conf.sh /usr/bin/render_nutcracker_conf
COPY twemproxy.tar.gz /opt/

RUN  chmod +x /usr/bin/render_nutcracker_conf \
     && render_nutcracker_conf /opt/nutcracker.tmpl /opt/nutcracker.yml \
     && mkdir /opt/twemproxy && tar -zxvf /opt/twemproxy.tar.gz -C /opt/twemproxy \
     && yum install -y automake make libtool unzip \
     && cd /opt/twemproxy && autoreconf -fvi &&  CFLAGS="-ggdb3 -O0" ./configure --enable-debug=full \
     && make && make install

CMD nutcracker -c /opt/nutcracker.yml
