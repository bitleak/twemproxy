from common import *

def test_msetnx_in_master_slave():
    nc = get_redis_conn(is_ms=True) 
    nc.execute_command('AUTH', redis_passwd)
    nc.delete(*default_kv.keys())
    assert(nc.msetnx(default_kv))

def test_msetnx_in_shards():
    nc = get_redis_conn(is_ms=False) 
    nc.delete(*default_kv.keys());
    assert_fail('unknown command', nc.msetnx, default_kv)
