#include "concurrent.h"

void recv_ring_buffer_put(GV gv, LV lv, char* buffer, int* num_avail_elements){

  ring_buffer *rb = gv->consumer_rb_p;

  pthread_mutex_lock(rb->lock_ringbuffer);
  while(1) {
    if (rb->num_avail_elements < rb->bufsize) {
      rb->buffer[rb->head] = buffer;

#ifdef DEBUG_PRINT
    printf("Ana_Proc%d: Receiver%d ****Put-a-pointer-in-CRB**** src=%d, block_id=%d, rb->num_avail_elements=%d, @ rb->head=%d\n",
      gv->rank[0], lv->tid, ((int*)buffer)[0], ((int*)buffer)[1], rb->num_avail_elements, rb->head);
    fflush(stdout);
#endif //DEBUG_PRINT

      rb->head = (rb->head+1) % rb->bufsize;
      *num_avail_elements = ++rb->num_avail_elements;

      pthread_cond_broadcast(rb->empty);
      pthread_mutex_unlock(rb->lock_ringbuffer);
      return;
    } else {

#ifdef DEBUG_PRINT
    printf("Ana_Proc%d: Receiver%d Prepare to Sleep! rb->num_avail_elements=%d, rb->head=%d\n", gv->rank[0], lv->tid, rb->num_avail_elements, rb->head);
    fflush(stdout);
#endif //DEBUG_PRINT

      lv->wait++;
      pthread_cond_wait(rb->full, rb->lock_ringbuffer);

#ifdef DEBUG_PRINT
    printf("Ana_Proc%d: Receiver%d Wake up! rb->num_avail_elements=%d, rb->head=%d\n", gv->rank[0], lv->tid, rb->num_avail_elements, rb->head);
    fflush(stdout);
#endif //DEBUG_PRINT
    }
  }
}


// void copy_msg_int(int* temp1, int* temp2, int num_int){
//   int i;
//   for(i=0;i<num_int;i++)
//     temp1[i]=temp2[i];
// }

// int copy_msg_double(double* p1, double* p2, int num_double){
//   int i;
//   for(i=0; i<num_double; i++){
//     p1[i]=p2[i];
//     // printf("p2[%d]=%f\n", i, p2[i]);
//     // fflush(stdout);
//   }
//   printf("p2[%d]=%f\n", i-1, p2[i-1]);
//   fflush(stdout);
//   return i;
// }

void make_prefetch_id(GV gv, int src, int num_int, int* tmp_int_ptr){
  int i;
  int* temp1 = (int*) gv->prefetch_id_array;
  int* temp2 = tmp_int_ptr;
  for(i=0;i<num_int;i++){
    temp1[gv->recv_head]=src;
    temp1[gv->recv_head+1]=temp2[i];
    // printf("Receiver MIX: written id= %d\n", temp2[i]);
    // fflush(stdout);
    // if(temp2[i]>gv->ana_total_blks){
    //   printf("Error! Compute %d Receiver get temp2[i]=%d\n", temp2[i], gv->rank[0]);
    //   fflush(stdout);
    // }
    gv->recv_head+=2;
    gv->recv_avail+=2;
  }
  //printf("After copy short_msg, Ana Node %d Receive thread recv_tail = %d\n", gv->rank[0], gv->recv_tail);
  // fflush(stdout);
}


void analysis_receiver_thread(GV gv, LV lv){
  int recv_int=0, block_id=0, source=0;
  double t0=0, t1=0, t2=0, t3=0, t4=0, t5=0, mkidarr_time=0;
  double receive_time=0;
  MPI_Status status;
  int errorcode, long_msg_id=0, mix_msg_id=0, disk_id=0;
  int* tmp_int_ptr;
  char* new_buffer=NULL;
  int num_exit_flag = 0;
  int num_avail_elements=0, full=0, prog=0;

  // printf("Ana_Node %d Receiveing thread %d Start receive!\n",gv->rank[0], lv->tid);
  // fflush(stdout);

  t0 = MPI_Wtime();
  while(1){

    // if ((num_exit_flag >= gv->computer_group_size) && (prog>=gv->ana_total_blks)) {
    //   gv->ana_reader_done=1;
    //   break;
    // }

    // #ifdef DEBUG_PRINT
    // printf("Prepare to receive!\n");
    // fflush(stdout);
    // #endif //DEBUG_PRINT

    t2 = MPI_Wtime();
    errorcode = MPI_Recv(gv->org_recv_buffer, gv->compute_data_len, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    if(errorcode!= MPI_SUCCESS){
        printf("Ana_Proc%d: Error MPI receive!\n",gv->rank[0]);
        fflush(stdout);
        exit(1);
    }
    t3 = MPI_Wtime();
    receive_time += t3-t2;

    // #ifdef DEBUG_PRINT
    // printf("Already received!\n");
    // fflush(stdout);
    // #endif //DEBUG_PRINT

    if(status.MPI_TAG==MPI_MSG_TAG){
#ifdef DEBUG_PRINT
      printf("Enter MPI MSG!\n");
      fflush(stdout);
#endif //DEBUG_PRINT
      /***************************receive a long_msg***************************************/
      // pthread_mutex_lock(&gv->lock_recv_disk_id_arr);
      // gv->prefetch_counter++;
      // pthread_mutex_unlock(&gv->lock_recv_disk_id_arr);

      // if(long_messageind%3000==0)
      //   printf("LONG!____Node %d Receive thread %d mpi_recv_progress_counter=%ld, long_messageind= %ld, short_messageind=%ld\n", gv->rank[0], lv->tid, gv->mpi_recv_progress_counter,long_messageind,short_messageind);

      prog++;
      long_msg_id++;

#ifdef DEBUG_PRINT
      int count;
      MPI_Get_count(&status, MPI_CHAR, &count);

      printf("Ana_Proc%d: Receiver *LONG MSG* --src=%d-- num_long_msg=%d, gv->mpi_recv_prog_cnt=%d, get_cnt=%d\n",
        gv->rank[0], status.MPI_SOURCE, long_msg_id, prog, count);
      fflush(stdout);
      printf("Ana_Proc%d: Receiver *LONG MSG* --src=%d block_id=%d-- num_long_msg=%d, gv->mpi_recv_prog_cnt=%d, get_cnt=%d, num_double=%d, step=%d, CI=%d, CJ=%d, CK=%d\n",
        gv->rank[0], status.MPI_SOURCE, ((int *)gv->org_recv_buffer)[0], long_msg_id, prog,
        count, gv->cubex*gv->cubey*gv->cubez*2, ((int *)gv->org_recv_buffer)[1], ((int *)gv->org_recv_buffer)[2], ((int *)gv->org_recv_buffer)[3], ((int *)gv->org_recv_buffer)[4]);
      fflush(stdout);
#endif //DEBUG_PRINT


      new_buffer = (char *) malloc(gv->analysis_data_len);
      tmp_int_ptr = (int*)new_buffer;
      check_malloc(new_buffer);

      source = status.MPI_SOURCE;
      tmp_int_ptr[0] = source;
      block_id = ((int *)gv->org_recv_buffer)[0];
      tmp_int_ptr[1] = block_id;
      tmp_int_ptr[2] = NOT_ON_DISK;
      tmp_int_ptr[3] = NOT_CALC;
      tmp_int_ptr[4] = ((int *)gv->org_recv_buffer)[1]; //step
      tmp_int_ptr[5] = ((int *)gv->org_recv_buffer)[2]; //CI
      tmp_int_ptr[6] = ((int *)gv->org_recv_buffer)[3]; //CJ
      tmp_int_ptr[7] = ((int *)gv->org_recv_buffer)[4]; //CK

#ifdef DEBUG_PRINT
      if(block_id<0){
        printf("Ana_Proc%d: Receiver%d Before memcpy, src=%d, block_id=%d, write=%d, calc=%d, step=%d, CI=%d, CJ=%d, CK=%d\n",
          gv->rank[0], lv->tid, tmp_int_ptr[0], tmp_int_ptr[1], tmp_int_ptr[2], tmp_int_ptr[3], tmp_int_ptr[4], tmp_int_ptr[5], tmp_int_ptr[6], tmp_int_ptr[7]);
        fflush(stdout);
      }
#endif //DEBUG_PRINT

      memcpy(new_buffer+sizeof(int)*8, gv->org_recv_buffer+5*sizeof(int), gv->cubex*gv->cubey*gv->cubez*2*sizeof(double));
      // tmp=copy_msg_double( (double*)(new_buffer+sizeof(int)*8), (double*)(gv->org_recv_buffer+sizeof(int)*5), gv->cubex*gv->cubey*gv->cubez*2);

#ifdef DEBUG_PRINT
      printf("Ana_Proc%d: Receiver%d pass memcpy src=%d blkid=%d\n", gv->rank[0], lv->tid, tmp_int_ptr[0], tmp_int_ptr[1]);
      fflush(stdout);
#endif //DEBUG_PRINT

      t4 = MPI_Wtime();
      recv_ring_buffer_put(gv, lv, new_buffer, &num_avail_elements);
      t5 = MPI_Wtime();
      lv->ring_buffer_put_time += t5-t4;

      if(num_avail_elements == gv->consumer_rb_p->bufsize)
        full++;
    }

    else if (status.MPI_TAG == MIX_MPI_DISK_TAG) {

#ifdef DEBUG_PRINT
      printf("Enter MIX_MPI_DISK_TAG MSG!\n");
      fflush(stdout);
#endif //DEBUG_PRINT

      /***************************RECEIVE DISK INDEX ARRAY***************************************/
      tmp_int_ptr = (int*)(gv->org_recv_buffer + sizeof(int) + sizeof(char)*gv->block_size);
      recv_int=*(int *)(tmp_int_ptr);
      disk_id += recv_int;
      mix_msg_id++;
      prog += recv_int+1;

      // #ifdef DEBUG_PRINT
      // printf("mix_msg_id:- recv_int=%d,prog=%d,tmp_int_ptr[1]=%d\n",
      //   recv_int,prog,tmp_int_ptr[1]);
      // fflush(stdout);
      // #endif //DEBUG_PRINT

      // statistic !!!!!!!!
      t4 = MPI_Wtime();

      // tell reader block ids on disk
      pthread_mutex_lock(&gv->lock_recv_disk_id_arr);
      // gv->prefetch_counter++;
      make_prefetch_id(gv, status.MPI_SOURCE, recv_int, tmp_int_ptr+1);
      //wake up reader
      pthread_cond_signal(gv->reader_on);
      pthread_mutex_unlock(&gv->lock_recv_disk_id_arr);
      t5 = MPI_Wtime();
      mkidarr_time += t5-t4;

      new_buffer = (char *) malloc(gv->analysis_data_len);
      check_malloc(new_buffer);
      tmp_int_ptr = (int*)new_buffer;

      tmp_int_ptr[0] = status.MPI_SOURCE;
      block_id =*((int *)(gv->org_recv_buffer));
      tmp_int_ptr[1] = block_id;
      tmp_int_ptr[2] = NOT_ON_DISK;
      tmp_int_ptr[3] = NOT_CALC;

      // #ifdef DEBUG_PRINT
      // printf("Analysis Process %d Receiver %d Get a MIX msg! long_messageind=%d, prog=%d, block_id=%d\n",
      //   gv->rank[0], lv->tid, mix_msg_id,prog,block_id);
      // fflush(stdout);
      // #endif //DEBUG_PRINT

      memcpy(new_buffer+sizeof(int)*4, gv->org_recv_buffer+sizeof(int), gv->block_size);
      // copy_msg_int(tmp_int_ptr+4,(int*)(),gv->block_size/sizeof(int));

      t4 = MPI_Wtime();
      recv_ring_buffer_put(gv, lv, new_buffer, &num_avail_elements);
      t5 = MPI_Wtime();
      lv->ring_buffer_put_time += t5-t4;

      if(num_avail_elements == gv->consumer_rb_p->bufsize)
        full++;
      //printf("Node 2 RECEIVE thread %d using recv_progress_counter = %d\n", lv->tid, gv->recv_progress_counter);

    }
    else if(status.MPI_TAG == DISK_TAG){

      MPI_Get_count(&status, MPI_CHAR, &recv_int);
      recv_int=recv_int/sizeof(int);
      prog += recv_int;

      printf("Ana_Proc%04d: pure_disk_msg:- recv_int=%d, prog=%d\n",
        gv->rank[0], recv_int, prog);
      fflush(stdout);

      tmp_int_ptr=(int*)gv->org_recv_buffer;

      t4 = MPI_Wtime();
      pthread_mutex_lock(&gv->lock_recv_disk_id_arr);
      make_prefetch_id(gv, status.MPI_SOURCE, recv_int, tmp_int_ptr);
      //wake up reader
      pthread_cond_signal(gv->reader_on);
      pthread_mutex_unlock(&gv->lock_recv_disk_id_arr);
      t5 = MPI_Wtime();
      mkidarr_time += t5-t4;

    }
    else if (status.MPI_TAG == EXIT_MSG_TAG){

      num_exit_flag++;

      new_buffer = (char *)malloc(gv->analysis_data_len);
      check_malloc(new_buffer);

      ((int*)new_buffer)[0] = status.MPI_SOURCE;
      ((int*)new_buffer)[1] = EXIT_BLK_ID;
      ((int*)new_buffer)[2] = ON_DISK;
      ((int*)new_buffer)[3] = CALC_DONE;

#ifdef DEBUG_PRINT
      printf("Ana_Proc%04d: Receiver%d get a *EXIT_MSG_TAG* from src=%d with block_id=%d, num_exit_flag=%d\n",
        gv->rank[0], lv->tid, ((int*)new_buffer)[0], ((int*)new_buffer)[1], num_exit_flag);
      fflush(stdout);
#endif //DEBUG_PRINT

      if(num_exit_flag==gv->computer_group_size){

#ifdef DEBUG_PRINT
        printf("Ana_Proc%04d: Receiver%d Ready to put the last EXIT, num_exit_flag=%d\n",
          gv->rank[0], lv->tid, num_exit_flag);
        fflush(stdout);
#endif //DEBUG_PRINT

        gv->recv_exit = 1;
        pthread_cond_signal(gv->reader_on);
        //wait till reader finish reading all blk_id in the disk_id_arr
        while(gv->reader_exit==0);

        t4 = MPI_Wtime();
        recv_ring_buffer_put(gv, lv, new_buffer, &num_avail_elements);
        t5 = MPI_Wtime();
        lv->ring_buffer_put_time += t5-t4;

        break;
      }

      t4 = MPI_Wtime();
      recv_ring_buffer_put(gv, lv, new_buffer, &num_avail_elements);
      t5 = MPI_Wtime();
      lv->ring_buffer_put_time += t5-t4;

      if(num_avail_elements == gv->consumer_rb_p->bufsize)
        full++;

    }
    else{
      printf("Ana_Proc%d: receive error!!!!! Get a Tag=%d\n", gv->rank[0], status.MPI_TAG);
      fflush(stdout);
      exit(1);
    }
  }
  t1 = MPI_Wtime();

  printf("Ana_Proc%04d: Receiver%d T_total=%.3f, prog=%d, \
T_recv_wait=%.3f, T_put=%.3f, T_mkidarr=%.3f, M_long=%d, M_mix=%d, disk=%d, full_wait=%d, #_exit=%d\n",
     gv->rank[0], lv->tid, t1-t0, prog,
     receive_time, lv->ring_buffer_put_time, mkidarr_time, long_msg_id, mix_msg_id, disk_id, lv->wait, num_exit_flag);
  fflush(stdout);
}
