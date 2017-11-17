#coding: utf-8

import os
import sys

redis_passwd = "foobared"
nc_servers = {
        'redis-ms': {'host': '127.0.0.1',  'port': 32121},
        'redis-shards': {'host': '127.0.0.1',  'port': 32122},
        'mc-shards': {'host': '127.0.0.1',  'port': 32123}
        }

redis_servers = {
        'redis-master': {'host': '127.0.0.1', 'port': 2100},
        'redis-slave': {'host': '127.0.0.1', 'port': 2101},
        'redis-shard1': {'host': '127.0.0.1', 'port': 3100},
        'redis-shard2': {'host': '127.0.0.1', 'port': 3101},
        'redis-shard3': {'host': '127.0.0.1', 'port': 3102}
        }

mc_servers = {
        'mc-shard1': {'host': '127.0.0.1', 'port': 8100},
        'mc-shard2': {'host': '127.0.0.1', 'port': 8101}
        }
