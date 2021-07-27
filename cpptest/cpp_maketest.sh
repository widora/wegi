#!/bin/bash

make -f Makefile_cpp test_cpp TEST_NAME=$1 EGILIB=static
