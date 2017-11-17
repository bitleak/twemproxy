#!/usr/bin/env python
#coding: utf-8

from common import *

default_kv = {'kkk-%s' % i : 'vvv-%s' % i for i in range(10)}

'''

cases:


redis       proxy       case
1           1           test_auth_basic
1           bad         test_badpass_on_proxy
1           0           test_nopass_on_proxy
0           0           already tested on other case
0           1

'''

def test_auth_basic():
    # we hope to have same behavior when the server is redis or twemproxy
    conns = [
         redis.Redis(redis_servers['redis-master']['host'], redis_servers['redis-master']['port']),
         redis.Redis(nc_servers['redis-ms']['host'], nc_servers['redis-ms']['port']),
    ]

    for r in conns:
        assert_fail('NOAUTH|operation not permitted', r.ping)
        assert_fail('NOAUTH|operation not permitted', r.set, 'k', 'v')
        assert_fail('NOAUTH|operation not permitted', r.get, 'k')

        # bad passwd
        assert_fail('invalid password', r.execute_command, 'AUTH', 'badpasswd')

        # everything is ok after auth
        r.execute_command('AUTH', redis_passwd)
        r.set('k', 'v')
        assert(r.ping() == True)
        assert(r.get('k') == 'v')

        # auth fail here, should we return ok or not => we will mark the conn state as not authed
        assert_fail('invalid password', r.execute_command, 'AUTH', 'badpasswd')

        assert_fail('NOAUTH|operation not permitted', r.ping)
        assert_fail('NOAUTH|operation not permitted', r.get, 'k')

def test_nopass_on_proxy():
    r = redis.Redis(nc_servers['redis-shards']['host'], nc_servers['redis-shards']['port'])

    # if you config pass on redis but not on twemproxy,
    # twemproxy will reply ok for ping, but once you do get/set, you will get errmsg from redis
    assert(r.ping() == True)

    # proxy has no pass, when we try to auth
    assert_fail('Client sent AUTH, but no password is set', r.execute_command, 'AUTH', 'anypasswd')
    pass

def test_badpass_on_proxy():
    r = redis.Redis(nc_servers['redis-ms']['host'], nc_servers['redis-ms']['port'])

    assert_fail('NOAUTH|operation not permitted', r.ping)
    assert_fail('NOAUTH|operation not permitted', r.set, 'k', 'v')
    assert_fail('NOAUTH|operation not permitted', r.get, 'k')
    # we can auth with bad pass (twemproxy will say ok for this)
    assert_fail('invalid password', r.execute_command, 'AUTH', 'badpasswd')
