from common import *

def test_bitop_in_master_slave():
    nc = get_redis_conn(is_ms=True)
    nc.execute_command('AUTH', redis_passwd)
    nc.mset({'a': '111', 'b': '222', 'c': '333', 'd': '444'})
    assert(nc.bitop('and', 'new_key', 'a', 'b', 'c', 'd'))
    assert(nc.bitop('or', 'new_key', 'a', 'b', 'c'))
    assert(nc.bitop('xor', 'new_key', 'a', 'b'))
    assert(nc.bitop('not', 'new_key', 'a'))

def test_bitop_in_shards():
    nc = get_redis_conn(is_ms=False)
    nc.mset({'a': '111', 'b': '222', 'c': '333', 'd': '444'})
    assert_fail('unknown command', nc.bitop, 'and', 'new-key', 'a', 'b', 'c', 'd')
