#!/usr/bin/env bash

# First created: 2018 Apr 04
# Last modified: 2018 Apr 04

# Author: Yuankun Fu 
# email: qoofyk@gmail.com

for i in `seq $a $b`
do    
    less /pylon5/ac561jp/fli5/data_broker_adios/${i}/results/producer.log | grep 'Producer' | cut -d ':' -f 2 | sed 's/[^0-9.]//g' 
done

for i in `seq $a $b`
do
    less /pylon5/ac561jp/fli5/data_broker_adios/${i}/results/consumer.log | grep 'Consumer end' | cut -d ':' -f 2 | sed 's/[^0-9.]//g' 
done
