#!/bin/bash

make -f PC_Makefile_cpp test_cpp TEST_NAME=$1 EGILIB=static
