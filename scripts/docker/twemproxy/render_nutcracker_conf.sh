#!/bin/bash
sed -e "s/__redis_master__/$REDIS_MASTER/" \
    -e "s/__redis_slave__/$REDIS_SLAVE/" \
    -e "s/__redis_shard1__/$REDIS_SHARD1/" \
    -e "s/__redis_shard2__/$REDIS_SHARD2/" \
    -e "s/__redis_shard3__/$REDIS_SHARD3/" \
    -e "s/__mc_shard1__/$MC_SHARD1/" \
    -e "s/__mc_shard2__/$MC_SHARD2/" \
    $1 > $2
