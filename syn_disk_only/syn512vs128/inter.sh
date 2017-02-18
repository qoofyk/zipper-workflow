directory="syn080vs080"
echo "----------------------Start Time----------------------"
date
echo "------------------------------------------------------"

generator=1
producer=1
prefetcher=1
each_computer_group_size=1
num_analysis_nodes=80
num_compute_nodes=80
cid_dir_num=0
total_nodes=160
# total_blks=(1000 5000 10000 50000 100000)
cpt_total_blks=10000
maxp=3
lp=70

echo "------syn_disk_only_store 080vs080_1M8P---------------"
echo "lp=$lp"
echo "Usage: %s generator_num, producer#, prefetcher#, each_computer_group_size,num_analysis_nodes,block_size,total_blks,lp"
echo "Total File to produce ?GB, each compute node produce ?GB, Testing parallel write and read, each compute node generate $total_blks blocks"
# my_run_exp1='ccmrun mpirun -np 2 -machinefile ccmrunnodefile --map-by ppr:1:node /N/home/f/u/fuyuan/BigRed2/Openfoam/20150701_test/Exp1_debug/synthetic_app'
my_run_exp2="aprun -n $total_nodes -N 8 -d 4 /N/u/fuyuan/BigRed2/Openfoam/20160201_test/syn_disk_only/synthetic_app"
# my_run_exp1='ccmrun mpirun -np 2 -machinefile ccmrunnodefile --map-by ppr:1:node /N/home/f/u/fuyuan/BigRed2/Openfoam/20150701_test/Exp1_debug/synthetic_app'
# my_run_exp2='ccmrun mpirun -np 64 -machinefile ccmrunnodefile --npernode 1 /N/home/f/u/fuyuan/BigRed2/Openfoam/20150701_test/Exp2/synthetic_app'
# my_del_exp1='time rsync -a --delete-before  /N/dc2/scratch/fuyuan/empty/ /N/dc2/scratch/fuyuan/1cid/'

mkdir /N/dc2/scratch/fuyuan/syn
mkdir /N/dc2/scratch/fuyuan/syn/$directory/
mkdir /N/dc2/scratch/fuyuan/syn/$directory/exp2/

my_del_exp2='time rsync -a --delete-before  /N/dc2/scratch/fuyuan/empty/ '
# MODIFY {0..1}
echo "remove all subdirectories"
echo "-----------Delete files-----------------"
for ((k=0;k<$num_compute_nodes;k++)); do
    $my_del_exp2 $(printf "/N/dc2/scratch/fuyuan/syn/$directory/exp2/cid%03g" $k)
done
echo "-----------End Delete files-------------"
# $my_del_exp2 /N/dc2/scratch/fuyuan/syn/$directory/exp2/
echo "mkdir new"
#mkdir $(printf "/N/dc2/scratch/fuyuan/syn/$directory/exp2/cid%03g " {0..$cid_dir_num})
for ((k=0;k<$num_compute_nodes; k++)); do
	mkdir $(printf "/N/dc2/scratch/fuyuan/syn/$directory/exp2/cid%03g " $k)
	lfs setstripe --count 4 -o -1 $(printf "/N/dc2/scratch/fuyuan/syn/$directory/exp2/cid%03g " $k)
done


# MODIFY k<2

val=0

echo "####### Synthetic Application $num_compute_nodes Compute Parallel Write vs $num_analysis_nodes Analysis Parallel Read ########"

echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
echo "***********$num_compute_nodes Compute Write*******************************"
echo "Each compute node has 1 thread, each thread will write according to num_blks"
echo "Block size starting from 64KB,128KB,256KB,512KB,1MB,2MB,4MB,8MB"
echo "Block size input =	   1   ,2    ,4    ,8    ,16 ,32 ,64 ,128"
echo "num_blks = 1, 10, 100, 200, 400, 800, 1000, 2000"
echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
echo
echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
echo "*********************$num_analysis_nodes Analysis Read**********************"
echo "Each Analysis node has 1 thread, each thread will read according to num_blks"
echo "Block size starting from 64KB,128KB,256KB,512KB,1MB,2MB,4MB,8MB"
echo "Block size input=		   1   ,2    ,4    ,8    ,16 ,32 ,64 ,128"
echo "num_blks = 1, 10, 100, 200, 400, 800, 1000, 2000"
echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"


block_size=(128 64 32 16 8 4 2)
cpt_total_blks=(1000 2000 4000 8000 16000 32000 64000)


for((i=0;i<${#block_size[@]};i++));do
	echo
	echo
	val=`expr ${block_size[i]} \* 64`
	echo "*************************************************************************************"
	echo "---syn_disk_only_store $val KB cpt_total_blks=${cpt_total_blks[i]}------"
	echo "*************************************************************************************"
	# if [ $val -eq 64 ] && [ $val -eq 128 ] && [ $val -eq 16384 ]
	#  		then
	#     		break
	#fi
	for ((p=0; p<$maxp; p++)); do
		echo "=============Loop $p==============="
		$my_run_exp2 $generator $producer $prefetcher $each_computer_group_size $num_analysis_nodes ${block_size[i]} ${cpt_total_blks[i]} $lp
		# echo "-----------Start Deleting files-------------"
		# for ((m=0;m<$num_compute_nodes;m++)); do
		#     $my_del_exp2 $(printf "/N/dc2/scratch/fuyuan/syn/$directory/exp2/cid%03g" $k)
		# done
		# echo "-----------End Delete files-------------"
		echo
	done
done

echo "-----------Start Deleting files-------------"
for ((m=0;m<$num_compute_nodes;m++)); do
    $my_del_exp2 $(printf "/N/dc2/scratch/fuyuan/syn/$directory/exp2/cid%03g" $k)
done
echo "-----------End Delete files-------------"
echo

echo
echo "----------------------End Time----------------------"
date
echo "------------------------------------------------------"

