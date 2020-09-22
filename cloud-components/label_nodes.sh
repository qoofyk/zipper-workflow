#!/usr/bin/env bash

# First created: 
# Last modified: 2018 Jan 26

# Author: Feng Li
# email: fengggli@yahoo.com

worker_nodes=($(kubectl get nodes -l  magnum.openstack.org/role=worker -o jsonpath={.items[*].metadata.name}))

i=0
for node in ${worker_nodes[*]}
do
echo setting for node ${i}}
#kubectl label nodes ${worker_nodes[i]}  minion-idx=${i}
kubectl label --overwrite nodes $node  minion-idx=${i}
i=$((i+1))
done
