##################parameter setting#################################################
SLURM_JOBID=111
directory="0008v0004"

CASE_NAME=LBM_DataBroker_concurrent_NoKeep

FILESIZE2PRODUCE=256 # 64*64*256*2*8 = 16MB per proc
compute_generator_num=1
compute_writer_num=1
analysis_reader_num=1
analysis_writer_num=1
# writer_thousandth=300
compute_group_size=2
num_comp_proc=8
num_ana_proc=$((${num_comp_proc} / ${compute_group_size}))
total_proc=$((${num_comp_proc} + ${num_ana_proc}))

maxp=1 #control how many times to run each configuration
lp=4   #n_moment
NSTOP=100 # how many steps


# Block size starting from 64KB,128KB,256KB,512KB,1MB, 2MB, 4MB, 8MB
# cubex =	   			   16  ,16   ,16   ,32   ,32 , 32 , 64 , 64
# cubez =	   			   16  ,32   ,64   ,32   ,64 , 128, 64 , 128

cubex=(32 32 64 64)
cubez=(64 128 64 128)
writer_thousandth=(0 999) #upper_limit_hint
writer_prb_thousandth=(1000 1000)

tune_stripe_count=-1
####################################################################################

echo "-----------case=$CASE_NAME---------------"
echo "datasize=$FILESIZE2PRODUCE nstops=$NSTOP num_comp_proc=$num_comp_proc num_ana_proc=$num_ana_proc n_moments=$lp NSTOP=$NSTOP stripe_count=$tune_stripe_count"

PBS_O_HOME=$HOME
PBS_O_WORKDIR=$(pwd)

# comet
# SCRATCH_DIR=/oasis/scratch/comet/qoofyk/temp_project/lbm

# bridges
groupname=$(id -Gn)
export SCRATCH_DIR=/pylon5/cc4s86p/qoofyk/lbm/${SLURM_JOBID}
EMPTY_DIR=/pylon5/cc4s86p/qoofyk/empty/

BIN1=${PBS_O_HOME}/General_Data_Broker/lbm_concurrent_store/build_nokeep/lbm_concurrent_nokeep_onefile
BIN2=${PBS_O_HOME}/General_Data_Broker/lbm_concurrent_store/build_nokeep/lbm_concurrent_nokeep_sep_file

source ${PBS_O_WORKDIR}/scripts_bridges/common.sh
