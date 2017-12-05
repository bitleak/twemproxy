#!/usr/bin/env bash
NAME="twemproxy"
./_build/bin/nutcracker -V 2> .version
VERSION=`head -1 .version|awk -F- '{printf substr($2, 0, length($2)-1);}'`
rm .version
STAGE=${STAGE:-release}

# setup fake root
mkdir -p _root/www/twemproxy/logs
mkdir -p _root/etc/logrotate.d
cp -r _build/* _root/www/twemproxy/
cp rpm/logrotate _root/etc/logrotate.d/twemproxy

fpm -f -s dir -t rpm -n ${NAME} --epoch 7 -C _root \
    --config-files /www/twemproxy/conf/nutcracker.yml \
    -v ${VERSION} --iteration ${CI_PIPELINE_ID}.${STAGE} \
    --verbose --category 'Meitu/Projects' --description 'twemproxy' \
    --url 'http://www.meitu.com' --license 'Commerial' -m 'linty@meitu.com' \
    --rpm-init ./rpm/twemproxy.init --after-install ./rpm/after-install --before-remove ./rpm/before-remove \
    www/twemproxy etc/logrotate.d/twemproxy
