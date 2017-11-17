#!/usr/bin/env python
#coding: utf-8

import os
import sys
import redis
import memcache

PWD = os.path.dirname(os.path.realpath(__file__))
WORKDIR = os.path.join(PWD,  '../')
sys.path.append(os.path.join(WORKDIR, 'lib/'))
sys.path.append(os.path.join(WORKDIR, 'conf/'))
from conf import *

from utils import *

large = int(getenv('T_LARGE', 1000))

def getconn():
    host_port = '%s:%s' % (nc_servers['mc-shards']['host'], nc_servers['mc-shards']['port'])
    return memcache.Client([host_port])

def test_basic():
    conn = getconn()
    conn.set('k', 'v')
    assert('v' == conn.get('k'))

    conn.set("key", "1")
    for i in range(10):
        conn.incr("key")
        assert(str(i+2) == conn.get('key'))

    conn.delete("key")
    assert(None == conn.get('key'))

default_kv = {'kkk-%s' % i :'vvv-%s' % i for i in range(10)}
def test_mget_mset(kv=default_kv):
    conn = getconn()
    conn.set_multi(kv)
    keys = sorted(kv.keys())

    assert(conn.get_multi(keys) == kv)
    assert(conn.gets_multi(keys) == kv)

    #del
    conn.delete_multi(keys)
    #mget again
    vals = conn.get_multi(keys)
    assert({} == vals)

def test_mget_mset_large():
    for cnt in range(179, large, 179):
        #print 'test', cnt
        kv = {'kkk-%s' % i :'vvv-%s' % i for i in range(cnt)}
        test_mget_mset(kv)

def test_mget_mset_key_not_exists(kv=default_kv):
    conn = getconn()
    conn.set_multi(kv)

    keys = kv.keys()
    keys2 = ['x-'+k for k in keys]
    keys = keys + keys2
    random.shuffle(keys)

    for i in range(2):
        #mget to check
        vals = conn.get_multi(keys)
        for i, k in enumerate(keys):
            if k in kv:
                assert(kv[k] == vals[k])
            else:
                assert(k not in vals)

    #del
    conn.delete_multi(keys)
    #mget again
    vals = conn.get_multi(keys)
    assert({} == vals)

