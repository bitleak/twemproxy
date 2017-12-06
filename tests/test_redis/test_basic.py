#!/usr/bin/env python
#coding: utf-8

from common import *

def test_setget():
    r = get_redis_conn(is_ms=False)

    rst = r.set('k', 'v')
    assert(r.get('k') == 'v')

def test_null_key():
    r = get_redis_conn(is_ms=False)
    rst = r.set('', 'v')
    assert(r.get('') == 'v')

    rst = r.set('', '')
    assert(r.get('') == '')

    kv = {'' : 'val', 'k': 'v'}
    ret = r.mset(**kv)
    assert(r.get('') == 'val')

def test_ping_quit():
    r = get_redis_conn(is_ms=False)
    assert(r.ping() == True)

    #get set
    rst = r.set('k', 'v')
    assert(r.get('k') == 'v')

    assert_fail('Socket closed|Connection closed', r.execute_command, 'QUIT')

def test_slow_req():
    r = get_redis_conn(is_ms=False)

    kv = {'mkkk-%s' % i : 'mvvv-%s' % i for i in range(500000)}

    pipe = r.pipeline(transaction=False)
    pipe.set('key-1', 'v1')
    pipe.get('key-1')
    pipe.hmset('xxx', kv)
    pipe.get('key-2')
    pipe.get('key-3')

    assert_fail('timed out', pipe.execute)

def test_issue_323():
    # do on twemproxy
    c = get_redis_conn(is_ms=False)
    assert([1, 'OK'] == c.eval("return {1, redis.call('set', 'x', '1')}", 1, 'tmp'))

def setup_and_wait():
    time.sleep(60*60)
