#include "concurrent.h"

char* analysis_writer_ring_buffer_read_tail(GV gv, LV lv, int* source_p, int* block_id_p, int* writer_state_p, int* dump_lines_in_this_blk_p){
	char* pointer;

	ring_buffer *rb = gv->consumer_rb_p;

	pthread_mutex_lock(rb->lock_ringbuffer);

	while(1) {

		if(gv->ana_writer_exit==1){
			pthread_mutex_unlock(rb->lock_ringbuffer);
			return NULL;
		}

		if (rb->num_avail_elements > 0) {
			pointer = rb->buffer[rb->tail];
			*source_p = ((int*)pointer)[0];
			*block_id_p = ((int*)pointer)[1];
			*writer_state_p = ((int*)pointer)[2];
			*dump_lines_in_this_blk_p = ((int*)pointer)[5];

// #ifdef DEBUG_PRINT
// 			printf("Ana_Proc%d: Writer%d ****GET a TAIL**** rb->num_avail_elements=%d, rb->tail=%d\n",
			// gv->rank[0], lv->tid, rb->num_avail_elements, rb->tail);
// 			fflush(stdout);
// #endif //DEBUG_PRINT

			pthread_mutex_unlock(rb->lock_ringbuffer);
			return pointer;
		 }
		else {

#ifdef DEBUG_PRINT
			printf("Ana_Proc%d: Writer%d ****Prepare to Sleep!**** rb->num_avail_elements=%d, rb->tail=%d\n", gv->rank[0], lv->tid, rb->num_avail_elements, rb->tail);
			fflush(stdout);
#endif //DEBUG_PRINT

			lv->wait++;
			pthread_cond_wait(rb->empty, rb->lock_ringbuffer);

#ifdef DEBUG_PRINT
			printf("Ana_Proc%d: Writer%d Wake up! rb->num_avail_elements=%d, rb->tail=%d\n", gv->rank[0], lv->tid, rb->num_avail_elements, rb->tail);
			fflush(stdout);
#endif //DEBUG_PRINT
		}
	}
}

void analysis_write_blk_per_file(GV gv, LV lv, int source, int blk_id, char* buffer, int nbytes){
	char file_name[128];
	FILE *fp = NULL;
	double t0 = 0, t1 = 0;
	int i = 0;
	//"/N/dc2/scratch/fuyuan/LBMconcurrentstore/LBMcon%03dvs%03d/cid%03d/2lbm_cid%03dblk%d.d"
#ifndef WRITE_ONE_FILE
	sprintf(file_name, ADDRESS, gv->compute_process_num, gv->analysis_process_num, source, source, blk_id);
	// printf("%d %d %d %d %d %d \n %s\n", gv->compute_process_num, gv->analysis_process_num, gv->rank[0], gv->rank[0], lv->tid, blk_id,file_name);
	// fflush(stdout);
#endif //WRITE_ONE_FILE

	while ((fp == NULL) && (i<TRYNUM)){
		fp = fopen(file_name, "wb");
		if (fp == NULL){
			if (i == TRYNUM - 1){
				printf("Warning: Ana_Proc%d Writer%d write empty file last_gen_rank=%d, blk_id=%d\n",
					gv->rank[0], lv->tid, source, blk_id);
				fflush(stdout);
			}

			i++;
			usleep(10000);
		}
	}

	if (fp != NULL){
		t0 = MPI_Wtime();
		fwrite(buffer, nbytes, 1, fp);
		t1 = MPI_Wtime();
		lv->only_fwrite_time += t1 - t0;
	}
	else{
		printf("Ana_Proc%d: Writer%d try to write a MISSING file last_gen_rank=%d, blk_id=%d !!!!!!!!!!!!!!!!!!!!!\n",
			gv->rank[0], lv->tid, source, blk_id);
		fflush(stdout);
	}

	fclose(fp);
}

void analysis_write_one_file(GV gv, LV lv, int source, int blk_id, char* buffer, int nbytes, FILE *fp){
	double t0=0,t1=0;
	int error=-1;
	int i=0;
	long int offset;

	offset = (long)blk_id * (long)gv->block_size;

	// i=0;
	// error=-1;
	while(error!=0){
		error=fseek(fp, offset, SEEK_SET);
		i++;
        // usleep(OPEN_USLEEP);
		if(i>TRYNUM){
			printf("Ana_Proc%04d: Writer Fatal Error--fseek error block_id=%d, offset=%ld,*fp=%p\n",
				gv->rank[0], blk_id, offset, (void*)fp);
			fflush(stdout);
			break;
		}
	}


	t0 = MPI_Wtime();
	error=fwrite(buffer, nbytes, 1, fp);
	fflush(fp);

	if(ferror(fp)){
		perror("Ana_Write error:");
		fflush(stdout);
	}

	t1 = MPI_Wtime();
	lv->only_fwrite_time += t1 - t0;
}

void analysis_writer_thread(GV gv, LV lv) {

	int source=0, block_id=0, my_count=0, dump_lines_in_this_blk=0;;
	double t0=0, t1=0, t2=0, t3=0;
	char* pointer=NULL;
	int writer_state;
	int write_bytes=0;
	double read_tail_wait_time=0, change_state_time=0;

	ring_buffer *rb = gv->consumer_rb_p;

	// int dest = gv->rank[0]/gv->computer_group_size + gv->computer_group_size*gv->analysis_process_num;

	// printf("Ana_Proc%d: Writer%d is running!\n", gv->rank[0], lv->tid);
	// fflush(stdout);

	t2 = MPI_Wtime();

	while(1){

// #ifdef NOKEEP
// 		if(gv->analysis_writer_blk_num==0){
// 			break;
// 		}
// #endif //NOKEEP

		//get pointer from PRB
		t0 = MPI_Wtime();
    	pointer = analysis_writer_ring_buffer_read_tail(gv, lv, &source, &block_id, &writer_state, &dump_lines_in_this_blk);
    	t1 = MPI_Wtime();
    	read_tail_wait_time += t1-t0;

		if(pointer!=NULL){

#ifdef DEBUG_PRINT
			printf("Ana_Proc%04d: Writer%d ***GET A pointer*** write source=%d block_id=%d, my_count=%d\n",
				gv->rank[0], lv->tid, source, block_id, my_count);
			fflush(stdout);
#endif //DEBUG_PRINT


			if (block_id != EXIT_BLK_ID){

				if(writer_state==NOT_ON_DISK){

#ifdef KEEP
					t0 = MPI_Wtime();
#ifdef WRITE_ONE_FILE
					if(block_id>=0){
						write_bytes = dump_lines_in_this_blk*sizeof(double)*5 + sizeof(int)*2;

#ifdef DEBUG_PRINT
						printf("Ana_Proc%04d: Writer%d ~~~Write a block~~~ source=%d block_id=%d, my_count=%d\n",
							gv->rank[0], lv->tid, source, block_id, my_count);
						fflush(stdout);
#endif //DEBUG_PRINT

						analysis_write_one_file(gv, lv, source, block_id, pointer+sizeof(int)*4, write_bytes, gv->ana_write_fp[source%gv->computer_group_size]);
					}
					else{
						printf("Ana_Proc%04d: Writer%d ***GET A WRONG pointer*** write source=%d block_id=%d, my_count=%d\n",
							gv->rank[0], lv->tid, source, block_id, my_count);
						fflush(stdout);
					}
#else
					analysis_write_blk_per_file(gv, lv, source, block_id, pointer+sizeof(int)*4, write_bytes);
#endif //WRITE_ONE_FILE
					t1 = MPI_Wtime();
					lv->write_time += t1 - t0;
#endif //KEEP
					my_count++;


#ifdef DEBUG_PRINT
					printf("Ana_Proc%04d: Writer%d ***Prepare-Assign-State and Prepare to Sleep*** write source=%d block_id=%d, my_count=%d\n",
						gv->rank[0], lv->tid, ((int*)pointer)[0], ((int*)pointer)[1], my_count);
					fflush(stdout);
#endif //DEBUG_PRINT

					t0 = MPI_Wtime();
					pthread_mutex_lock(rb->lock_ringbuffer);
					((int*)pointer)[2] = ON_DISK;
					pthread_cond_wait(rb->new_tail, rb->lock_ringbuffer);
					pthread_mutex_unlock(rb->lock_ringbuffer);
					t1 = MPI_Wtime();
    				change_state_time += t1-t0;

#ifdef DEBUG_PRINT
					if(my_count%WRITER_COUNT==0)
						printf("Ana_Proc%04d: Writer%d my_count %d\n", gv->rank[0], lv->tid, my_count);
#endif //DEBUG_PRINT

#ifdef DEBUG_PRINT
					printf("Ana_Proc%04d: Writer%d ***Finish-Write-state and Wake up*** source=%d block_id=%d, my_count=%d\n",
						gv->rank[0], lv->tid, source, block_id, my_count);
					fflush(stdout);
#endif //DEBUG_PRINT

				}
				else{// this block has already been written on disk, then writer continue read tail

// #ifdef DEBUG_PRINT
// 					printf("Ana_Proc%d: Writer%d ***Get a ON_DISK block and Prepare to sleep*** source=%d block_id=%d, my_count=%d\n",
// 						gv->rank[0], lv->tid, source, block_id, my_count);
// 					fflush(stdout);
// #endif //DEBUG_PRINT

// #ifdef DEBUG_PRINT
// 					printf("Ana_Proc%d: Writer%d ***Wake up*** source=%d block_id=%d, my_count=%d\n",
// 						gv->rank[0], lv->tid, source, block_id, my_count);
// 					fflush(stdout);
// #endif //DEBUG_PRINT

				}
			}
			else{// writer get the final block EXIT_BLK_ID, then continue read tail

// #ifdef DEBUG_PRINT
// 					printf("Ana_Proc%d: Writer%d ***Get a EXIT_BLK_ID and Prepare to sleep*** source=%d block_id=%d, my_count=%d\n",
// 						gv->rank[0], lv->tid, source, block_id, my_count);
// 					fflush(stdout);
// #endif //DEBUG_PRINT


// #ifdef DEBUG_PRINT
// 					printf("Ana_Proc%d: Writer%d ***Wake up*** source=%d block_id=%d, my_count=%d\n",
// 						gv->rank[0], lv->tid, source, block_id, my_count);
// 					fflush(stdout);
// #endif //DEBUG_PRINT

			}

		}
		else{//get a NULL pointer means: A_consumer got the final block and require A_writer to exit

#ifdef DEBUG_PRINT
			printf("Ana_Proc%04d: A_Writer%d get NULL to exit\n", gv->rank[0], lv->tid);
			fflush(stdout);
#endif //DEBUG_PRINT

			break;
		}

		if (gv->ana_writer_exit==1) {
			break;
		}
	}

	t3 = MPI_Wtime();

	printf("Ana_Proc%04d: Writer%d, T_total=%.3f, T_ana_write=%.3f, T_fwrite=%.3f, T_rd_tail=%.3f, T_change_state=%.3f, cnt=%d, empty_wait=%d\n",
		gv->rank[0], lv->tid, t3-t2, lv->write_time, lv->only_fwrite_time, read_tail_wait_time, change_state_time, my_count, lv->wait);
	fflush(stdout);

}
