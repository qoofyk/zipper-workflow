/*
 * @author Feng Li, IUPUI
 * @date   2017
 */

#include "adios_adaptor.h"

#ifdef V_T
#include <VT.h>
int class_id;
int adios_open_id, adios_write_id, adios_read_id,adios_close_id;
#endif


status_t adios_adaptor_init_client(char *xmlfile, MPI_Comm comm){
#ifdef V_T
      //VT_initialize(NULL, NULL);
      PINF("[adios_adaptor]: trace enabled and initialized\n");
      VT_classdef( "ADIOS", &class_id );
      VT_funcdef("AD-OP", class_id, &adios_open_id);
      VT_funcdef("AD-WR", class_id, &adios_write_id);
      VT_funcdef("AD-RD", class_id, &adios_read_id);
      VT_funcdef("AD-CL", class_id, &adios_close_id);
#endif


    if(adios_init (xmlfile, comm) != 0){
        PERR("adios init err\n");
        PERR("%s\n", adios_get_last_errmsg());
        TRACE();
        return S_FAIL;
    }
    else{
        return S_OK;
    }
}


/*
 * sender update the index file 
 *
 * this will be called after each mpiio insertion, only for mpiio
 */
status_t adios_adaptor_update_avail_version(MPI_Comm comm, char * step_index_file, int timestep, int nsteps){
    int fd; 
    int time_stamp;
    int rank;

    MPI_Comm_rank (comm, &rank);
    if(rank == 0){
        if(timestep == nsteps - 1){
           time_stamp = -2;
        }else{
           time_stamp = timestep;
        }// flag read from producer


        PINF("step index file in %s", step_index_file);

        fd = open(step_index_file, O_WRONLY|O_CREAT|O_SYNC, S_IRWXU);
        if(fd < 0){
            perror("indexfile not opened");
            TRACE();
            return S_FAIL;
        }
        else{
            flock(fd, LOCK_EX);
            write(fd,  &time_stamp,  sizeof(int));
            flock(fd, LOCK_UN);
            PINF("write stamp %d at %lf", time_stamp, MPI_Wtime());
            close(fd);
        }
    }
    // wait until all process finish writes and 
#ifdef BARRIER_STAMP
    MPI_Barrier(comm);
#endif
    return S_OK;
}

/*
 * return true if producer is still outputing
 * this will set the maxversion the reader can read
 *
 *
 * this will be called only for mpiio
 */

status_t adios_adaptor_get_avail_version(MPI_Comm comm, char *step_index_file, int *p_max_version,int nstop){
    int fd;
    int rank;

    MPI_Comm_rank (comm, &rank);
    if(rank ==0){
        fd = open(step_index_file, O_RDONLY);
        if(fd == -1){
            perror("indexfile not here wait for 1 s \n");
   
        }
        else{
            flock(fd, LOCK_SH);
            read(fd,  p_max_version,  sizeof(int));
            flock(fd, LOCK_UN);
            close(fd);
        }

        // if produer is ready to terminate
        if(*p_max_version  == -2){
            PINF("producer  terminate\n");
            *p_max_version= nstop-1;
            // run this gap then exit
        }

    }
    // broadcast stamp
    MPI_Bcast(p_max_version, 1, MPI_INT, 0, comm);
#ifdef BARRIER_STAMP
    MPI_Barrier(comm);
#endif

    PDBG("all get stamp %d at %lf", *p_max_version, MPI_Wtime());

/*    if(rank ==0 && time_stamp!= time_stamp_old){*/
            /*PINF("set stamp as %d at %lf\n", time_stamp, MPI_Wtime());*/
    /*}*/

    return S_OK;
}


void insert_into_adios(char * file_path, char *var_name,int timestep, int n, int size_one, double * buf, const char* mode,  MPI_Comm *pcomm){
    char        filename [256];
    int         rank, size;
    uint64_t         NX;
    
    //int size_one = SIZE_ONE;
    // prepare sending buffer
    //double * t =  buf;
    //double      t[NX*SIZE_ONE];

    /* ADIOS variables declarations for matching gwrite_temperature.ch */
    //uint64_t    adios_groupsize, adios_totalsize;
    int64_t     adios_handle;

    MPI_Comm    comm = *pcomm;
    MPI_Comm_rank (comm, &rank);
    MPI_Comm_size (comm, &size);

    // nlines of all processes
    NX = EVAL(size)*EVAL(n);

    PDBG("NX=%lu or %ld, size=%d, n=%d", NX, NX, size, n);
    
    // lower bound of my line index
    uint64_t lb;
    lb = EVAL(rank)*EVAL(n);

    if(timestep <0){
        sprintf(filename, "%s/%s.bp", file_path, var_name);
    }
    else{
        sprintf(filename, "%s/%s_%d.bp", file_path, var_name, timestep);
    }

    //printf("rank %d: start to write\n", rank);
    

#ifdef V_T
      VT_begin(adios_open_id);
#endif
    adios_open (&adios_handle, var_name, filename, mode, comm);
#ifdef V_T
      VT_end(adios_open_id);
#endif
#ifdef debug
    printf("rank %d: file %s opened\n", rank, filename);
#endif
    /* generated by gpp.py*/
    //#include "gwrite_atom.ch"
    /*
    adios_groupsize = 4 \
                    + 4 \
                    + 4 \
                    + 4 \
                    + 8 * (n) * (size_one);
    adios_group_size (adios_handle, adios_groupsize, &adios_totalsize);
    */
#ifdef V_T
      VT_begin(adios_write_id);
#endif
    //PDBG("NX=%lu, lb=%lu, n= %d,size_one=%d", NX, lb, n, size_one); 
    adios_write (adios_handle, "NX", &NX);
    adios_write (adios_handle, "lb", &lb);
    adios_write (adios_handle, "n", &n);
    adios_write (adios_handle, "nprocs_prod", &size);
    adios_write (adios_handle, "size_one", &size_one);
    adios_write (adios_handle, var_name, buf);
#ifdef V_T
      VT_end(adios_write_id);
#endif

#ifdef V_T
      VT_begin(adios_close_id);
#endif
    adios_close (adios_handle);
#ifdef V_T
      VT_end(adios_close_id);
#endif

    /*
    if(rank ==0){
        printf("groupsize = %ld, adios totalsize = %ld\n",adios_groupsize, adios_totalsize);
    }
    */
}

