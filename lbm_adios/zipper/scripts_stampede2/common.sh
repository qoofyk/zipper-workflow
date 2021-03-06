####################################################
# common commands for all experiments

date

mkdir -pv ${EMPTY_DIR}

if [ x"$HAS_TRACE" = "x" ];then
    BUILD_DIR=${PBS_O_WORKDIR}/build

elif [ x"$HAS_TRACE" = "xitac" ]; then
    NSTOP=10
    export LD_PRELOAD=libVT.so
    echo "itac ENABLED, use 10 steps"
    export BUILD_DIR=${PBS_O_WORKDIR}/build_itac
    echo "use itac"
    export VT_LOGFILE_PREFIX=${SCRATCH_DIR}/trace
    mkdir -pv $VT_LOGFILE_PREFIX
else
    echo "TRACE ENABLED, use 10 steps"
    NSTOP=10
    export BUILD_DIR=${PBS_O_WORKDIR}/build_tau
    #enable trace
    export TAU_TRACE=1
    # set trace dir
    export ALL_TRACES=${SCRATCH_DIR}/trace
    mkdir -pv $ALL_TRACES/app0
    mkdir -pv $ALL_TRACES/app1
    mkdir -pv $ALL_TRACES/app2

    if [ -z $TAU_MAKEFILE ]; then
        module load tau
        echo "LOAD TAU!"
    fi

fi

BIN1=${BUILD_DIR}/bin/lbm_concurrent_nokeep_onefile
BIN2=${BUILD_DIR}/bin/lbm_concurrent_nokeep_sep_file


#prepare output result directory
PBS_RESULTDIR=${SCRATCH_DIR}/results
mkdir -pv ${PBS_RESULTDIR}
lfs setstripe --stripe-size 1m --stripe-count ${tune_stripe_count} ${PBS_RESULTDIR}

# echo "if use sepfile, mkdir new"
# for ((m=0;m<$num_comp_proc; m++)); do
# 	mkdir -pv $(printf "${PBS_RESULTDIR}/cid%04g " $m)
# 	# lfs setstripe --stripe-size 1m --count 4 $(printf "${SCRATCH_DIR}/cid%04g" $m)
# done

#generate hostfile
# HOST_DIR=$SCRATCH_DIR/hosts
# mkdir -pv $HOST_DIR
# rm -f $HOST_DIR/hostfile*
# #all tasks run the following command
# srun -o $HOST_DIR/hostfile-dup hostname
# cat $HOST_DIR/hostfile-dup | sort | uniq | sed "s/$/:${nproc_per_mac}/" >$HOST_DIR/hostfile-all
# cd ${PBS_RESULTDIR}

#SET TOTAL MPI PROC
export SLURM_NTASKS=$total_proc

#MPI_Init_thread(Multiple level)
export MV2_ENABLE_AFFINITY=0

#Turn on Debug Info on Bridges
# export PGI_ACC_NOTIFY=3


# find number of threads for OpenMP
# find number of MPI tasks per node
echo "SLURM_TASKS_PER_NODE=$SLURM_TASKS_PER_NODE"
# TPN=`echo $SLURM_TASKS_PER_NODE | cut -d '(' -f 1`
# echo "TPN=$TPN"
# # find number of CPU cores per node
echo "SLURM_JOB_CPUS_PER_NODE=$SLURM_JOB_CPUS_PER_NODE"
PPN=`echo $SLURM_JOB_CPUS_PER_NODE | cut -d '(' -f 1`
echo "PPN=$PPN, nproc_per_mac=$nproc_per_mac"
# THREADS=$(( PPN / TPN ))
# export OMP_NUM_THREADS=$THREADS

# echo "TPN=$TPN, PPN=$PPN, THREADS=$THREADS, n_moments=$n_moments, SLURM_NTASKS=$SLURM_NTASKS"

#
export OMP_NUM_THREADS=$(( PPN / nproc_per_mac ))
# export OMP_NUM_THREADS=2
echo "OMP_NUM_THREADS=$OMP_NUM_THREADS, n_moments=$n_moments, SLURM_NTASKS=$SLURM_NTASKS"

echo "Block size starting from 64KB,128KB,256KB,512KB,1MB,2MB,4MB,8MB"
echo "Block size input =	   1   ,2    ,4    ,8    ,16 ,32 ,64 ,128"

# my_run_exp1="mpirun -genv OMP_NUM_THREADS $OMP_NUM_THREADS -genv MV2_ENABLE_AFFINITY 0 -genv SLURM_NTASKS $total_proc -genvall -n $total_proc $EXE1"
# my_run_exp2="mpirun -genv OMP_NUM_THREADS $OMP_NUM_THREADS -genv MV2_ENABLE_AFFINITY 0 -genv KMP_AFFINITY verbose,granularity=core,compact,1,0 -np $total_proc $EXE2"

# LAUNCHER="mpirun_rsh"
# my_run_exp1="$LAUNCHER -export -hostfile $HOST_DIR/hostfile-all -np $total_proc $BIN1"
# LAUNCHER="mpirun"
# my_run_exp1="$LAUNCHER -np $total_proc $BIN1"
LAUNCHER="ibrun" #"remora ibrun"
my_run_exp1="$LAUNCHER $BIN1"
my_del_exp2="time rsync -a --delete-before ${EMPTY_DIR} "
env|grep '^OMP'
env|grep '^MV2'

echo "remove all subdirectories"
date
echo "-----------Start Deleting files-------------"
# for ((m=0;m<$num_comp_proc;m++)); do
# 	ls -1 $(printf "${PBS_RESULTDIR}/cid%04g" $m) | wc -l
#     $my_del_exp2 $(printf "${PBS_RESULTDIR}/cid%04g" $m)
# done
echo "-----------End Delete files-------------"
date




echo
echo
echo "####### Simulate $num_comp_proc Compute Parallel Write vs $num_ana_proc Analysis Parallel Read ########"

echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
echo "***********$num_comp_proc Compute Write*******************************"
echo "Block size starting from 128KB,256KB,512KB,1MB,2MB,4MB,8MB, 16MB"
echo "cubex =	   			   16  ,16   ,16   ,32   ,32 ,32 ,64 ,64"
echo "cubez =	   			   16  ,32   ,64   ,32   ,64 ,128,64 ,128"
echo "*********************$num_ana_proc Analysis Read**********************"
echo "Block size starting from 64KB,128KB,256KB,512KB,1MB,2MB,4MB,8MB"
echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"


echo "~~~~~~~~~~~~~~~~~~Write_one_file~~~~~~~~~~~~~~~~~~~~~~~"
for((i=0;i<${#cubex[@]};i++));do
	val=$((${cubex[i]} * ${cubex[i]} * ${cubez[i]} * $each_point_size / 1024))

	for ((k=0; k<${#writer_thousandth[@]}; k++)); do

		echo
		echo
		echo "*************************************************************************************"
		echo "---case=$CASE_NAME $val KB, $((${writer_thousandth[k]}/10))% PRB Use Disk, Writer_PRB=${writer_prb_thousandth[k]} /1000------"
		echo "*************************************************************************************"

		for ((p=0; p<$maxp; p++)); do
			echo "=============Loop $p==============="
			echo "setstripe $tune_stripe_count"
			echo "n_moments=${n_moments}"
			echo "-----------------------------------"

			export TRACEDIR=$(printf "${MYTRACE}/EXP%04g" $k)
			real_run="$my_run_exp1 $compute_generator_num $compute_writer_num $analysis_reader_num $analysis_writer_num ${writer_thousandth[k]} $compute_group_size $num_ana_proc ${cubex[i]} ${cubez[i]} $NSTOP $n_moments ${writer_prb_thousandth[k]}"
			# $real_run &>> ${PBS_RESULTDIR}/log

			$real_run

			echo "real_run=$real_run"
			sleep 5
			echo "-----------Start Deleting files-------------"
			# for ((m=0;m<$num_comp_proc;m++)); do
			#     $my_del_exp2 $(printf "${PBS_RESULTDIR}/cid%04g" $m)
			# done
			echo "-----------End Delete files-------------"
		done
	done

done

# $my_del_exp2 ${PBS_RESULTDIR}

date


