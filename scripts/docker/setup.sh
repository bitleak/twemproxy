#!/bin/sh
cd ../../ && git archive develop --format tar.gz -o twemproxy.tar.gz
mv twemproxy.tar.gz scripts/docker/twemproxy && cd -
docker-compose -p twemproxy up -d
