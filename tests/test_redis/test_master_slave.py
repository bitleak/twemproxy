from common import *

def test_slave_readonly():
    nc = redis.Redis(nc_servers['redis-ms']['host'], nc_servers['redis-ms']['port'])
    slave = redis.Redis(redis_servers['redis-slave']['host'], redis_servers['redis-slave']['port'])
    nc.execute_command('AUTH', redis_passwd)
    nc.set('k', 'v')
    assert(nc.get('k') == 'v')
    slave.execute_command('AUTH', redis_passwd)
    assert_fail('You can\'t write against a read only slave', slave.set, 'k', 'v') 


def test_master_slave_choose_server():
    nc = redis.Redis(nc_servers['redis-ms']['host'], nc_servers['redis-ms']['port'])
    slave = redis.Redis(redis_servers['redis-slave']['host'], redis_servers['redis-slave']['port'])
    nc.execute_command('AUTH', redis_passwd)
    for k, v in default_kv.items():
        nc.set(k, v)

    slave.execute_command('AUTH', redis_passwd)
    for k, expected in default_kv.items():
        assert_equal(slave.get(k), expected)
