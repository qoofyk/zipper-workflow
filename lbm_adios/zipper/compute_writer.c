#include "concurrent.h"

char* producer_ring_buffer_get(GV gv, LV lv, int* num_avail_elements){
	char* pointer;
	ring_buffer *rb = gv->producer_rb_p;
	int writer_on = gv->producer_rb_p->bufsize * gv->writer_prb_thousandth / 1000;

	pthread_mutex_lock(rb->lock_ringbuffer);
	while(1) {
		if(gv->flag_sender_get_finalblk==1){
			pthread_mutex_unlock(rb->lock_ringbuffer);
			return NULL;
		}

		// if (rb->num_avail_elements > 0) {
		if ( (rb->num_avail_elements>0) && (rb->num_avail_elements >= writer_on) ) {
		  pointer = rb->buffer[rb->tail];
		  rb->tail = (rb->tail + 1) % rb->bufsize;
		  *num_avail_elements = --rb->num_avail_elements;
		  pthread_cond_signal(rb->full);
		  pthread_mutex_unlock(rb->lock_ringbuffer);
		  return pointer;
		}
		else {
		  lv->wait++;
		  pthread_cond_wait(gv->writer_on, rb->lock_ringbuffer);
		}
	}
}

void comp_write_blk_per_file(GV gv, LV lv, int blk_id, char* buffer, int nbytes){
	char file_name[128];
	FILE *fp=NULL;
	double t0=0, t1=0;
	int i=0;
	//"/N/dc2/scratch/fuyuan/LBMconcurrent/LBMcon%03dvs%03d/cid%03d/2lbm_cid%03dblk%d.d"
#ifndef WRITE_ONE_FILE
	sprintf(file_name, ADDRESS, gv->compute_process_num, gv->analysis_process_num, gv->rank[0], gv->rank[0], blk_id);
#endif //WRITE_ONE_FILE
	// printf("%d %d %d %d %d %d \n %s\n", gv->num_compute_nodes, gv->num_analysis_nodes, gv->rank[0], gv->rank[0], lv->tid, blk_id,file_name);
	// fflush(stdout);

	while((fp==NULL) && (i<TRYNUM)){
	    fp=fopen(file_name,"wb");
	    if(fp==NULL){
	    	if(i==TRYNUM-1){
	    		printf("Fatal Error: Comp_Proc%d Writer%d write empty file last_gen_rank=%d, blk_id=%d\n",
			        gv->rank[0], lv->tid, gv->rank[0], blk_id);
			    fflush(stdout);
	    	}
	      i++;
	      usleep(OPEN_USLEEP);
	    }
	}

	t0 = MPI_Wtime();
	fwrite(buffer, nbytes, 1, fp);
	t1 = MPI_Wtime();
	lv->only_fwrite_time += t1 - t0;

	fclose(fp);
}


void comp_write_one_big_file(GV gv, LV lv, int blk_id, char* buffer, int nbytes, FILE *fp){
	double t0=0, t1=0;
	int error=-1;
	int i=0;
	long int offset;

	offset = (long)blk_id * (long)gv->block_size;

	i=0;
	error=-1;
	while(error!=0){
		error=fseek(fp, offset, SEEK_SET);
  		i++;
        // usleep(OPEN_USLEEP);
		if(i>TRYNUM){
			printf("Comp_Proc%d Writer fseek error block_id=%d, fp=%p\n",
				gv->rank[0], blk_id, (void*)fp);
			fflush(stdout);
			break;
		}

  	}

	t0 = MPI_Wtime();
	error=fwrite(buffer, nbytes, 1, fp);
	fflush(fp);

	if(ferror(fp)){
		perror("Comp_Write error:");
		fflush(stdout);
	}

	t1 = MPI_Wtime();
	lv->only_fwrite_time += t1 - t0;
}

void compute_writer_thread(GV gv, LV lv) {

	int block_id=0, my_count=0;
	double t0=0, t1=0, t2=0, t3=0;
	char* buffer=NULL;
	char my_exit_flag=0;
	int num_avail_elements=0, overlap=0;

	ring_buffer *rb = gv->producer_rb_p;

	// int dest = gv->rank[0]/gv->computer_group_size + gv->computer_group_size*gv->analysis_process_num;

	// printf("Compute Process %d Writer thread %d is running!\n",gv->rank[0],lv->tid);

	t2 = MPI_Wtime();

	if (gv->writer_blk_num == 0) {

		if(gv->rank[0]==0 || gv->rank[0]==(gv->compute_process_num-1)){
			printf("Comp_Proc%d: Writer%d is turned off\n", gv->rank[0], lv->tid);
			fflush(stdout);
		}

		// pthread_mutex_lock(&gv->lock_writer_exit);
		gv->writer_exit=1;
		// pthread_mutex_unlock(&gv->lock_writer_exit);
	}
	else{
		while(1){

			//get pointer from PRB
    		buffer = producer_ring_buffer_get(gv, lv, &num_avail_elements);

			if(buffer != NULL){

				block_id = ((int*)buffer)[0];

				if (block_id != EXIT_BLK_ID){

					if(num_avail_elements>0) overlap++;

#ifdef DEBUG_PRINT
					printf("Comp_Proc%d: Writer%d start to write block_id=%d\n", gv->rank[0], lv->tid, block_id);
					fflush(stdout);
#endif //DEBUG_PRINT

					t0 = MPI_Wtime();
#ifdef WRITE_ONE_FILE
					comp_write_one_big_file(gv, lv, block_id, buffer+sizeof(int), gv->block_size, gv->fp);
#else
					comp_write_blk_per_file(gv, lv, block_id, buffer+sizeof(int), gv->block_size);
#endif //WRITE_ONE_FILE
					t1 = MPI_Wtime();
					lv->write_time += t1 - t0;
					my_count++;

#ifdef DEBUG_PRINT
					if(my_count%100==0){
						printf("Comp_Proc%d: Writer%d has written block_id=%d\n", gv->rank[0], lv->tid, my_count);
						fflush(stdout);
					}
#endif //DEBUG_PRINT

					//add to disk_id_array
					pthread_mutex_lock(&gv->lock_disk_id_arr);
					gv->written_id_array[gv->send_tail] = block_id;
					gv->send_tail++;
					pthread_mutex_unlock(&gv->lock_disk_id_arr);

					free(buffer);
				}
				else{
					// Get exit flag msg and quit

					printf("Comp_Proc%04d: Writer%d Get --EXIT-- flag msg and quit!\n",
						gv->rank[0], lv->tid);
					fflush(stdout);

					pthread_mutex_lock(rb->lock_ringbuffer);
					gv->flag_writer_get_finalblk = 1;
					rb->tail = (rb->tail + 1) % rb->bufsize;
      				rb->num_avail_elements--;
					pthread_cond_signal(rb->empty);
					pthread_mutex_unlock(rb->lock_ringbuffer);

					free(buffer);

					my_exit_flag=1;
				}

				if (my_count >= gv->writer_blk_num) {

					printf("Comp_Proc%d: Writer%d Exceed PreSet percentange blks and quit!\n",
						gv->rank[0], lv->tid);
					fflush(stdout);

					my_exit_flag=1;
				}
			}
			else{

#ifdef DEBUG_PRINT
			printf("Comp_Proc%04d: Writer%d Know that Sender Get exit flag msg and let it quit!\n",
					gv->rank[0], lv->tid);
			fflush(stdout);
#endif //DEBUG_PRINT

				my_exit_flag=1;
			}

			if(my_exit_flag==1){

				// printf("Comp_Proc%04d: Writer%d EXITING...\n", gv->rank[0], lv->tid);
				// fflush(stdout);

				// /*In case at last: sender has exit and writer never get lock_disk_id_arr */
				// if(gv->flag_sender_get_finalblk==1){

				// 	int remain_disk_id=0, errorcode=0;
				// 	int dest = gv->rank[0]/gv->computer_group_size + gv->compute_process_num;

				// 	pthread_mutex_lock(&gv->lock_disk_id_arr);
				// 	if (gv->send_tail>0){
				// 		remain_disk_id = gv->send_tail;
				// 		gv->send_tail=0;
				// 	}
				// 	pthread_mutex_unlock(&gv->lock_disk_id_arr);

				// 	if(remain_disk_id>0){
				// 		errorcode = MPI_Send(gv->written_id_array, remain_disk_id*sizeof(int), MPI_CHAR, dest, DISK_TAG, MPI_COMM_WORLD);
				// 		check_MPI_success(gv, errorcode);
				// 		printf("Comp_Proc%04d: Writer%d send remain_disk_id=%d\n",
				// 			gv->rank[0], lv->tid, remain_disk_id);
				// 		fflush(stdout);
				// 	}
				// }


				gv->writer_exit=1;

				break;
			}

		}
	}

	t3 = MPI_Wtime();

	printf("Comp_Proc%04d: Writer%d T_total=%.3f, T_comp_write=%.3f, T_fwrite=%.3f, cnt=%d, overlap=%d, wait=%d, exit=%d\n",
		gv->rank[0], lv->tid, lv->write_time, lv->only_fwrite_time, t3-t2, my_count, overlap, lv->wait, gv->writer_exit);
	fflush(stdout);
	// printf("Comp_Proc%04d: Writer%d T_total=%.3f, T_comp_write=%.3f, T_fwrite=%.3f, cnt=%d, overlap=%d, wait=%d\n",
	// 	gv->rank[0], lv->tid, lv->write_time, lv->only_fwrite_time, t3-t2, my_count, overlap, lv->wait);
	// fflush(stdout);

}
