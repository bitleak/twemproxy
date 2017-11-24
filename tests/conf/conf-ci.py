#coding: utf-8

import os
import sys

redis_passwd = "foobared"
nc_servers = {
        'redis-ms': {'host': 'twemproxy',  'port': 32121},
        'redis-shards': {'host': 'twemproxy',  'port': 32122},
        'mc-shards': {'host': 'twemproxy',  'port': 32123}
        }

redis_servers = {
        'redis-master': {'host': 'redis_master', 'port': 6379},
        'redis-slave': {'host': 'redis_slave', 'port': 6379},
        'redis-shard1': {'host': 'redis_shard1', 'port': 6379},
        'redis-shard2': {'host': 'redis_shard2', 'port': 6379},
        'redis-shard3': {'host': 'redis_shard3', 'port': 6379}
        }

mc_servers = {
        'mc-shard1': {'host': 'mc_shard1', 'port': 11211},
        'mc-shard2': {'host': 'mc_shard2', 'port': 11211}
        }
