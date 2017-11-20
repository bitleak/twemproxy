#!/usr/bin/env python
#coding: utf-8

from common import *
from test_mget_mset import test_mget_mset as _mget_mset

#force to use large mbuf, we need to copy the setup/teardown here..

######################################################
def test_mget_binary_value(cnt=5):
    kv = {}
    for i in range(cnt):
        kv['kkk-%s' % i] = os.urandom(1024*1024*16+1024) #16M
    for i in range(cnt):
        kv['kkk2-%s' % i] = ''
    _mget_mset(kv)

