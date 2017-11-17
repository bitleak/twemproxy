#!/usr/bin/env python
#coding: utf-8

import os
import sys
import redis

PWD = os.path.dirname(os.path.realpath(__file__))
WORKDIR = os.path.join(PWD,'../')
sys.path.append(os.path.join(WORKDIR,'conf/'))
sys.path.append(os.path.join(WORKDIR,'lib/'))
 
from conf import *
from utils import *

large = int(getenv('T_LARGE', 1000))
default_kv = {'kkk-%s' % i : 'vvv-%s' % i for i in range(10)}

def get_redis_conn(is_ms):
    if is_ms:
        host = nc_servers['redis-ms']['host']
        port = nc_servers['redis-ms']['port']
    else:
        host = nc_servers['redis-shards']['host']
        port = nc_servers['redis-shards']['port']

    r = redis.Redis(host, port)
    return r
