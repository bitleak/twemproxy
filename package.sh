#!/usr/bin/env bash
NAME="twemproxy"
./_build/bin/nutcracker -V 2> .version
VERSION=`head -1 .version|awk -F- '{printf substr($2, 0, length($2)-1);}'`
rm .version
STAGE=${STAGE:-release}
fpm -f -s dir -t rpm --prefix '/www/twemproxy'  -n ${NAME} --epoch 7 \
    --config-files /www/twemproxy/conf/nutcracker.yml \
    -v ${VERSION} --iteration ${CI_PIPELINE_ID}.${STAGE} -C ./_build \
    --verbose --category 'Meitu/Projects' --description 'twemproxy' \
    --url 'http://www.meitu.com' --license 'Commerial' -m 'linty@meitu.com'
