from common import *

def test_rename_in_master_slave():
    nc = redis.Redis(nc_servers['redis-ms']['host'], nc_servers['redis-ms']['port'])
    nc.execute_command('AUTH', redis_passwd)
    nc.delete('new_key')
    nc.set('rename-key', '1')
    assert(nc.rename('rename-key', 'new_key'))

def test_rename_in_shards():
    nc = redis.Redis(nc_servers['redis-shards']['host'], nc_servers['redis-shards']['port'])
    nc.delete('new_key')
    nc.set('rename-key', '1')
    assert_fail('unknown command', nc.rename,'rename-key', 'new_key')
