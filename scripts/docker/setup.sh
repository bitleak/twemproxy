#!/bin/sh
branch=`git rev-parse --abbrev-ref HEAD`
cd ../../ && git archive ${branch} --format tar.gz -o twemproxy.tar.gz
mv twemproxy.tar.gz scripts/docker/twemproxy && cd -
docker-compose -p twemproxy build
docker-compose -p twemproxy up -d --force-recreate
