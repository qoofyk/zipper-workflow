/*
 * helper functions when you want each process operates on a large chunk buffer
 *
 * Feng Li 
 * Jan 2018
 */
#ifndef LBM_BUFFER_H
#define LBM_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * example of  io routine
 */


status_t lbm_io_template(MPI_Comm *pcomm, double *buffer, size_t nlocal, size_t size_one);

status_t lbm_alloc_buffer(MPI_Comm *pcomm, size_t nlines, size_t size_one, double **pbuffer);
status_t lbm_get_buffer(double *buffer);
status_t lbm_free_buffer(MPI_Comm *pcomm, double *buffer);

#ifdef __cplusplus
}
#endif

#endif
