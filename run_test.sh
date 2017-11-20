#!/bin/bash

docker_script_dir="`pwd`/scripts/docker"
tests_dir="`pwd`/tests"

cd $docker_script_dir && sh setup.sh
cd $tests_dir && nosetests -v 
cd $docker_script_dir && sh teardown.sh
