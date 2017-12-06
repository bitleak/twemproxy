from common import *

def test_rename_in_master_slave():
    nc = get_redis_conn(is_ms=True)
    nc.execute_command('AUTH', redis_passwd)
    nc.delete('new_key')
    nc.set('rename-key', '1')
    assert(nc.rename('rename-key', 'new_key'))
    nc.delete('new_key_2')
    assert(nc.renamenx('new_key', 'new_key_2'))

def test_rename_in_shards():
    nc = get_redis_conn(is_ms=False)
    nc.delete('new_key')
    nc.set('rename-key', '1')
    assert_fail('unknown command', nc.rename,'rename-key', 'new_key')
    nc.delete('new_key')
    assert_fail('unknown command', nc.renamenx, 'new_key', 'new_key_2')
