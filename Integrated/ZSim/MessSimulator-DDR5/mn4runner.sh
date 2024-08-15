#!/bin/bash
module purge
module load gcc/7.2.0 bsc/1.0

module load intel/2017.4
module load parallel_studio/2017.4

module load mkl
module load python/3.8.2

module load impi/2018.4

#module load python/3.8.2
RWRATIO_MIN=80
RWRATIO_MAX=100
RWRATIO_STEP=2
# PAUSES="0 2 4 5 6 7 9 10 15 20 25 30 35 40 45 65 200"
# 2000
# PAUSES="100 120 140 160 200 300 70 80 90 0 1 2 3 4 5 6 7 8 9 10 15 20 25 30 35 40 45 50 55 65"


# PAUSES="0 5 10 15 20 25 30 35 40 45 50 55 60 65 70 80 90 100 110 120 130 140 150 160 170 180 190 200 210 220 240 260 280 300"

# new
PAUSES="3000 5000 10000 20000 2000 1000 0 5 10 15 20 25 30 35 40 45 50 55 60 65 70 80 90 100 110 120 130 140 150 160 170 180 190 200 210 220 240 260 280 300 340 380 450 550 600 700 800 900"


# PAUSES="2000 1000 0 5 10 15 20 25 30 35 40 45 50 55 60 65 70 80 90 100 120 140 160 180 200 220 260 300 340 380 450 550 600 700 800 900"
# PAUSES="0 5 10 15 20 25 30 35 40 45 50 55 60 65 70 80 90 100 120 140 160 180 200"

# 1500 2000 3000
# PAUSES="0 3 5 10 15 20 25 30 35 40 45 50 55 60 65 70 80 90 100 120 140 160 180 200 220 260 300 340 380 450 550 600 700 800 900 1000"

# PAUSES="3000 5000 10000 20000"

# 2000
SMOOTH_SAVGOL_WINDOW_LENGTH=0
SMOOTH_SAVGOL_POLYORDER=3


# cd ./src/stream_mpi/
# make
# cd ../../

# rm -r measurment_*

# exit 0



# for ((rd_percentage=RWRATIO_MIN; rd_percentage<=RWRATIO_MAX; rd_percentage+=RWRATIO_STEP)); do
#     for pause in ${PAUSES}; do


# 	    # to avoid submitting too much jobs to slurm
# 	    # jobsWAitingInTheQueue=$(squeue | wc -l)
# 	    # while [ $jobsWAitingInTheQueue -gt 300 ]
# 	    # do
# 	    #     sleep 200
# 	    #     jobsWAitingInTheQueue=$(squeue | wc -l)
# 	    #     echo 'waiting for the queues...'
# 		# done
		
	            
# 			export rd_percentage
# 			export pause

# 			echo "*********** Iteration: rd_percentage=${rd_percentage} pause=${pause} "


# 			mkdir measurment_${rd_percentage}_${pause}
# 			cd measurment_${rd_percentage}_${pause}
# 			cp ../sb.cfg ./

# 			sed -i "s/rd_percentage/${rd_percentage}/g" sb.cfg 
# 			sed -i "s/pause/${pause}/g" sb.cfg 

# 			# cp ../src/ptr_chase/ptr_chase ./
# 			# cp ../src/ptr_chase/array.dat ./

# 			cp ../submit.batch ./

# 			sbatch submit.batch

# 			cd ../

#     done




#     # to avoid submitting too much jobs to slurm
#     jobsWAitingInTheQueue=$(squeue | wc -l)
#     while [ $jobsWAitingInTheQueue -gt 250 ]
#     do
#         sleep 300
#         jobsWAitingInTheQueue=$(squeue | wc -l)
#         echo 'waiting for the queues...'
# 	done


# done


export ROOT_DIR="$(pwd)"
export ATE="$(date +"%d-%m-%Y-%H_%M_%S")"
export OUTPUT_DIR="${ROOT_DIR}/measuring/"



###################
# Post-processing #
###################
echo "start processing data..."


PROCESSING_DIR="${ROOT_DIR}/processing/"
${PROCESSING_DIR}/main.py ${OUTPUT_DIR}











