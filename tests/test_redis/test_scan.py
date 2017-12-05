from common import *

def test_scan_in_master_slave():
    nc = get_redis_conn(is_ms=True)
    nc.execute_command('AUTH', redis_passwd)
    assert(nc.scan(0, "*", 5))

def test_scan_in_shards():
    nc = get_redis_conn(is_ms=False)
    assert_fail('unknown command', nc.scan, '0')
