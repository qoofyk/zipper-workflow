#export  I_MPI_JOB_RESPECT_PROCESS_PLACEMENT=0

env|grep '^HAS' # trace enabled?
module list
echo "case=$CASE_NAME datasize=$FILESIZE2PRODUCE nstops=$NSTOP"
echo "procs is \[ ${procs_this_app[*]}\], nodes is \[${nodes_this_app[*]}\]"

BUILD_DIR=${PBS_O_WORKDIR}/build

BIN_PRODUCER=${BUILD_DIR}/bin/run_lbm;
BIN_CONSUMER=${BUILD_DIR}/bin/adios_disk_read;

#This job runs with 3 nodes  
#ibrun in verbose mode will give binding detail  
#BUILD=${PBS_O_WORKDIR}/build_dspaces/bin
PBS_RESULTDIR=${SCRATCH_DIR}/results 


mkdir -pv ${PBS_RESULTDIR}
tune_stripe_count=-1
lfs setstripe --stripe-size 1m --stripe-count ${tune_stripe_count} ${PBS_RESULTDIR}
mkdir -pv ${SCRATCH_DIR}

cd ${SCRATCH_DIR}
cp -R ${PBS_O_WORKDIR}/adios_xmls ${SCRATCH_DIR}


# this scripts is avaliable at
GENERATE_HOST_SCRIPT=${HOME}/Workspaces/General_Data_Broker/lbm_adios/scripts/generate_hosts.sh
#GENERATE_HOST_SCRIPT=${HOME}/Downloads/LaucherTest/generate_hosts.sh
if [ -a $GENERATE_HOST_SCRIPT ]; then
    source $GENERATE_HOST_SCRIPT
else
    echo "generate_hosts.sh should downloaded from:"
    echo "https://github.iu.edu/lifen/LaucherTest/blob/master/generate_hosts.sh"
fi

LAUNCHER="mpiexec.hydra"


export MV2_ENABLE_AFFINITY=0 
export MV2_USE_BLOCKING=1


#Use ibrun to run the MPI job. It will detect the MPI, generate the hostfile
# and doing the right binding. With no options ibrun will use all cores.
#export OMP_NUM_THREADS=1
CMD_PRODUCER="$LAUNCHER -np ${procs_this_app[0]} -machinefile $HOST_DIR/machinefile-app0  ${BIN_PRODUCER} ${NSTOP} ${FILESIZE2PRODUCE}"
$CMD_PRODUCER  &> ${PBS_RESULTDIR}/producer.log &
echo "producer applciation lauched: $CMD_PRODUCER"

CMD_CONSUMER="$LAUNCHER -np ${procs_this_app[1]} -machinefile $HOST_DIR/machinefile-app1 ${BIN_CONSUMER} ${NSTOP}"
$CMD_CONSUMER  &> ${PBS_RESULTDIR}/consumer.log &
echo " consumer applciation lauched $CMD_CONSUMER"

## Wait for the entire workflow to finish
wait
