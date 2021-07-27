#!/bin/bash

make -f PC_Makefile test EGILIB=static TEST_NAME=$1
#make -f PC_Makefile test TEST_NAME=$1
