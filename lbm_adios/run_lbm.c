#include "run_lbm.h"        
#include "adios_write_global.h"
#define SIZE_ONE (2)

#define debug

#define USE_ADIOS
void insert_into_Adios(char * filepath, int n, double * buf, MPI_Comm *pcomm){
    char        filename [256];
    int         rank, size, i, j;
    int         NX;
    
    int size_one = SIZE_ONE;
    // prepare sending buffer
    double * t =  buf;
    //double      t[NX*SIZE_ONE];

    /* ADIOS variables declarations for matching gwrite_temperature.ch */
    uint64_t    adios_groupsize, adios_totalsize;
    int64_t     adios_handle;

    MPI_Comm    comm = *pcomm;
    MPI_Comm_rank (comm, &rank);
    MPI_Comm_size (comm, &size);

    // nlines of all processes
    NX = size*n;
    
    // lower bound of my line index
    int lb;
    lb = rank*n;

    strcpy (filename, filepath);
    strcat(filename, "/adios_global.bp");

    //printf("rank %d: start to write\n", rank);
    
    adios_open (&adios_handle, "atom", filename, "w", comm);
#ifdef debug

    printf("rank %d: file %s opened\n", rank, filename);
#endif
#ifdef debug_1
    printf("lb = %d, n = %d, NX = %d, size = %d, rank = %d \n",lb, n, NX, size, rank);
        for (i = 0; i < n ; i++) {
            printf ("rank %d: [%d ,%d:%d]", rank, lb , 0, n);
            for (j = 0; j < SIZE_ONE ; j++){
                printf (" %6.6g", * ((double *)buf + i * SIZE_ONE  + j ));
            }
            printf("|");
            printf ("\n");
        }
#endif

    #include "gwrite_atom.ch"
    printf("rank %d: try to close\n", rank);
    adios_close (adios_handle);
    printf("rank %d: file %s closed\n", rank, filename);
    //printf("rank %d: write completed\n", rank);
}


double get_cur_time() {
  struct timeval   tv;
  struct timezone  tz;
  double cur_time;

  gettimeofday(&tv, &tz);
  cur_time = tv.tv_sec + tv.tv_usec / 1000000.0;
  //printf("%f\n",cur_time);

  return cur_time;
}

void check_malloc(void * pointer){
  if (pointer == NULL) {
    perror("Malloc error!\n");
    fprintf (stderr, "at %s, line %d.\n", __FILE__, __LINE__);
    exit(1);
  }
}


void run_lbm(char * filepath, int step_stop, int dims_cube[3], MPI_Comm *pcomm)
{
        int gi, gj, gk;

        int nprocs, rank;
        int n;
        MPI_Comm comm = *pcomm;
		MPI_Comm_size(comm,&nprocs);
		MPI_Comm_rank(comm,&rank);

        n = nx*ny*nz;

        // original code
        MPI_Status status;
		double df1[nx][ny][nz][19],df2[nx][ny][nz][19],df_inout[2][ny][nz][19];
		double rho[nx][ny][nz],u[nx][ny][nz],v[nx][ny][nz],w[nx][ny][nz];
		double c[19][3],dfeq,pow_2;
		int i,j,k,m,n1,n2,n3,ii,dt,time1,time2,time3;
		//int x_mid,y_mid,z_mid,step_wr;
		double width,length,nu_e,tau,rho_e,u_e, Re,time_stop;
		//double cs;
		double length_r,time_r,s1, s2, s3,s4, nu_lb, u_lb, rho_lb, rho_r, u_r;
		//double time_restart;
		double p_bb,depth;

		MPI_Comm comm1d;
		MPI_Datatype newtype,newtype_bt,newtype_fr;
		int nbleft,nbright,left_most,right_most,middle;
		int num_data,s,e,myid;
		//int num_data1,nj;
		int np[1],period[1];
		int *fp_np,*fp_period;
		// FILE *fp_u, *fp_v, *fp_out;
		FILE *fp_out;
		double t2=0, t3=0,t4=0,t5=0,t6=0,only_lbm_time=0,init_lbm_time=0;
		int errorcode;


		n1=nx-1;/* n1,n2,n3 are the last indice in arrays*/
		n2=ny-1;
		n3=nz-1;

		num_data=ny*nz;/* number of data to be passed from process to process */
		//num_data1=num_data*19;
		
		np[0]=nprocs;period[0]=0;
		fp_np=np;
		fp_period=period;

		MPI_Type_vector(num_data,1,19,MPI_DOUBLE,&newtype);
		MPI_Type_commit(&newtype);

		MPI_Type_vector(ny,1,nz*19,MPI_DOUBLE,&newtype_bt);
		MPI_Type_commit(&newtype_bt);

		MPI_Type_vector(nz,1,19,MPI_DOUBLE,&newtype_fr);
		MPI_Type_commit(&newtype_fr);


		errorcode=MPI_Cart_create(comm,1,fp_np,fp_period,1,&comm1d);
		if(errorcode!= MPI_SUCCESS){
		  printf("Error cart create!\n");
		  exit(1);
		}


		MPI_Comm_rank(comm1d,&myid);
		errorcode=MPI_Cart_shift(comm1d,0,1,&nbleft,&nbright);
		if(errorcode!= MPI_SUCCESS){
		  printf("Error shift create!\n");
		  exit(1);
		}

		

		//---------------------------------------------BEGIN LBM -----------------------------------------------
		#ifdef DEBUG_PRINT
		printf("Compute Node %d Generator begin LBM!\n",rank);
		fflush(stdout);
		#endif //DEBUG_PRINT

		t2 = get_cur_time();

		// x_mid=1+(nx-4)/2;

		// y_mid=1+(ny-4)/2;

		// z_mid=1+(nz-4)/2;

		left_most=0;right_most=nprocs-1;middle=nprocs/2;

		// if( ((float)nprocs-(float)middle*2.0) < 1.0e-3) x_mid=2;

		s=2; /* the array for each process is df?[0:n1][][][], the actual part is [s:e][][][] */

		e=n1-2;



		p_bb=0.03;

		depth=3.0e-5; /*30 microns */

		width=3.0e-2;  /* unit: cm i.e. 300 micron*/

		length=width*2.0;

		u_e=0.616191358; /* cm/sec */

		rho_e=0.99823; /* g/cm^3 */

		nu_e=1.007e-2; /* cm^2/sec */

		Re=u_e*depth/nu_e;



		tau=0.52;

		nu_lb=(2.0*tau-1.0)/6.0;

		u_lb=Re*nu_lb/(nz-4);

		rho_lb=0.3;



		/* real quantities in units cm*g*sec = reference quantities x quantities in simulation */

		/*depth-dx/(nz-4-1)=dx,solve it get the following length_r*/

		length_r=depth/(nz-4);  /* cm */

		time_r=depth*u_lb/((nz-4)*u_e);  /* sec, note that (nz-4), the # of nodes used to represent the depth of wall

		                  is the correct length scale L_lb in LBM */

		rho_r=rho_e/rho_lb;    /* g/cm^3 */

		u_r=u_e/u_lb;  /* cm/sec */



		/*nu_lb=u_lb*n2*nu_e/(u_e*width);*/

		/*nu_lb=u_lb*n3/Re;

		tau=(6.0*nu_lb+1.0)*0.5;*/



		dt=1;



		// cs=1.0/(sqrt(3));  /* non-dimensional */

		time_stop=depth/u_e*1.0; /* 10 times of the characteric time, which can be defined to be depth/u_e */

		//step_stop=(int)(time_stop/time_r) + 1;


		// time_restart=400000;

		time1=dt;

		time2=dt;

		time3=dt;

		int step=1;

		if (myid==middle) {

		fp_out=fopen("output_p.d","w");

		fprintf (fp_out,"the # of CPUs used in x-direction is %d   \n",nprocs);

		fprintf(fp_out,"the kinematic viscosity of the fluid is %e cm^2/sec\n",nu_e);

		fprintf(fp_out,"the mass density of fluid in channel is %e g/cm^3\n",rho_e);

		fprintf(fp_out,"the inlet speed is %e cm/sec\n",u_e);

		fprintf(fp_out,"the channel length is %e cm\n",length);

		fprintf(fp_out,"the channel width is %e cm\n", width);

		fprintf(fp_out,"the channel depth is %e cm\n", depth);

		fprintf(fp_out,"the Re number of the flow is %e \n",Re);

		fprintf(fp_out,"the number of lattice nodes in x-direction is %d \n",nx-4);

		fprintf(fp_out,"the number of lattice nodes in y-direction is %d \n",ny-4);

		fprintf(fp_out,"the number of lattice nodes in z-direction is %d \n",nz-4);

		fprintf(fp_out,"the inlet speed in simulation is %e \n",u_lb);

		fprintf(fp_out,"the mass density in simulation is %e \n",rho_lb);

		fprintf(fp_out,"the reference length is %e cm\n",length_r);

		fprintf(fp_out,"the reference time is %e sec\n",time_r);

		fprintf(fp_out,"the reference mass density is %e g/cm^3\n",rho_r);

		fprintf(fp_out,"the reference speed is %e cm/sec\n",u_r);

		fprintf(fp_out,"the kinematic viscosity in simulation is %e cm^2/sec\n",nu_lb);

		fprintf(fp_out,"the relaxation time tau in simulation is %e\n",tau);

		fprintf(fp_out,"the total time run is %e sec\n", time_stop);

		fprintf(fp_out,"the total step run is %d steps\n", step_stop);

		fclose(fp_out);

		}


		c[0][0]=0.0;c[0][1]=0.0;c[0][2]=0;



		c[1][0]=1.0;c[1][1]=0.0;c[1][2]=0;

		c[2][0]=-1.0;c[2][1]=0.0;c[2][2]=0.0;

		c[3][0]=0.0;c[3][1]=0.0;c[3][2]=1;

		c[4][0]=0.0;c[4][1]=0.0;c[4][2]=-1;

		c[5][0]=0.0;c[5][1]=-1.0;c[5][2]=0;

		c[6][0]=0.0;c[6][1]=1.0;c[6][2]=0;



		c[7][0]=1.0;c[7][1]=0.0;c[7][2]=1;

		c[8][0]=-1.0;c[8][1]=0.0;c[8][2]=-1;

		c[9][0]=1.0;c[9][1]=0.0;c[9][2]=-1.0;

		c[10][0]=-1.0;c[10][1]=0.0;c[10][2]=1.0;

		c[11][0]=0.0;c[11][1]=-1.0;c[11][2]=1.0;

		c[12][0]=0.0;c[12][1]=1.0;c[12][2]=-1.0;

		c[13][0]=0.0;c[13][1]=1.0;c[13][2]=1.0;

		c[14][0]=0.0;c[14][1]=-1.0;c[14][2]=-1.0;

		c[15][0]=1.0;c[15][1]=-1.0;c[15][2]=0.0;

		c[16][0]=-1.0;c[16][1]=1.0;c[16][2]=0.0;

		c[17][0]=1.0;c[17][1]=1.0;c[17][2]=0.0;

		c[18][0]=-1.0;c[18][1]=-1.0;c[18][2]=0.0;



		if(myid==left_most) s=1;

		if(myid==right_most) e=n1-1;

		// #ifdef DEBUG_PRINT
		// printf("Compute Node %d Generator LBM Here 1.1 !\n",gv->rank[0]);
		// fflush(stdout);
		// #endif //DEBUG_PRINT

		/* initialization of rho and (u,v), non-dimensional */

		/*for (i=1;i<=n1-1;i++)*/

		for (i=s;i<=e;i++)

		 for (j=1;j<=n2-1;j++)

		for (k=1;k<=n3-1;k++)

		    {

		    rho[i][j][k]=rho_lb;

		    u[i][j][k]=0.0;

		    v[i][j][k]=0.0;

		        w[i][j][k]=0.0;

		    }

		// #ifdef DEBUG_PRINT
		// printf("Compute Node %d Generator LBM Here 1.2 !\n",gv->rank[0]);
		// fflush(stdout);
		// #endif //DEBUG_PRINT

		if (myid==left_most) {

		/* inlet speed */

		i=2;

		for (j=1;j<=n2-1;j++)

		 for (k=1;k<=n3-1;k++)

		{u[i][j][k]=u_lb;}

		}



		if (myid==right_most) {

		/* outlet speed */

		i=n1-2;

		for (j=1;j<=n2-1;j++)

		 for(k=1;k<=n3-1;k++)

		{u[i][j][k]=u_lb;}

		}


		// #ifdef DEBUG_PRINT
		// printf("Compute Node %d Generator LBM Here 1.3 !\n",gv->rank[0]);
		// fflush(stdout);
		// #endif //DEBUG_PRINT

		// df1[2][1][1][0]=0;
		// #ifdef DEBUG_PRINT
		// printf("Compute Node %d Generator LBM Here 1.31 !\n",gv->rank[0]);
		// fflush(stdout);
		// #endif //DEBUG_PRINT

		/* equilibrium distribution function df0 */

		/*for (i=1;i<=n1-1;i++)*/

		for (i=s;i<=e;i++){
		// #ifdef DEBUG_PRINT
		// printf("Compute Node %d Generator LBM Here 1.32 i=%d!\n",gv->rank[0],i);
		// fflush(stdout);
		// #endif //DEBUG_PRINT

		for(j=1;j<=n2-1;j++)

		    for(k=1;k<=n3-1;k++){

		  df1[i][j][k][0]=1.0/3.0*rho[i][j][k]*(1.0-1.5*(u[i][j][k]*u[i][j][k]

		    +v[i][j][k]*v[i][j][k]+w[i][j][k]*w[i][j][k]));

		    for (m=1;m<=18;m++)  {
		      pow_2=c[m][0]*u[i][j][k]+c[m][1]*v[i][j][k]+c[m][2]*w[i][j][k];

		        df1[i][j][k][m]=1.0/18.0*rho[i][j][k]*(1.0+3.0*(c[m][0]*u[i][j][k]+c[m][1]*v[i][j][k]

		          +c[m][2]*w[i][j][k])

		                /*+4.5*pow(c[m][0]*u[i][j][k]+c[m][1]*v[i][j][k]+c[m][2]*w[i][j][k],2)*/

		                +4.5*pow_2*pow_2

		            -1.5*(u[i][j][k]*u[i][j][k]+v[i][j][k]*v[i][j][k]+w[i][j][k]*w[i][j][k]));

		      }



		    for (m=7;m<=18;m++)
		      {df1[i][j][k][m]=0.5*df1[i][j][k][m];}

		 }
		}



		// #ifdef DEBUG_PRINT
		// printf("Compute Node %d Generator LBM Here 1.4 !\n",gv->rank[0]);
		// fflush(stdout);
		// #endif //DEBUG_PRINT


		ii=0;

		for (i=2;i<=n1-2;i+=n1-4)

		 {

		 if (i>3) ii=1;

		 for(j=1;j<=n2-1;j++)

		    for(k=1;k<=n3-1;k++)

		       for (m=0;m<=18;m++)

		    {

		    df_inout[ii][j][k][m]=df1[i][j][k][m];

		    }



		}/*end of computing the inlet and outlet d.f. */


		t4=get_cur_time();
		init_lbm_time=t4-t2;
		only_lbm_time+=t4-t2;
		if(myid==0){
			printf("Init LBM Time=%f\n", init_lbm_time);
			fflush(stdout);
		}

		//int blk_id=0;

		while (step <= step_stop)

		{

		t5=get_cur_time();

		if (myid==0){
			printf("step = %d   of   %d   \n", step, step_stop);

			fflush(stdout);
		}





		s=2;e=n1-2;



		/* collision */

		/*for (i=2;i<=n1-2;i++)*/

		for (i=s;i<=e;i++)

		   for (j=2;j<=n2-2;j++) /* changing from n2-1 to n2-2 after using two walls */

		      for (k=2;k<=n3-2;k++)

		         for (m=0;m<=18;m++)

		  {



		    if (m==0)

		   {

		dfeq=1.0/3.0*rho[i][j][k]*(1.0-1.5*(u[i][j][k]*u[i][j][k]

		    +v[i][j][k]*v[i][j][k]+w[i][j][k]*w[i][j][k]));}



		else if (m>=1 && m<=6)

		      {

		               pow_2=c[m][0]*u[i][j][k]+c[m][1]*v[i][j][k]+c[m][2]*w[i][j][k];

		      dfeq=1.0/18.0*rho[i][j][k]*(1.0+3.0*(c[m][0]*u[i][j][k]+c[m][1]*v[i][j][k]

		      +c[m][2]*w[i][j][k])

		               /*+4.5*pow(c[m][0]*u[i][j][k]+c[m][1]*v[i][j][k]+c[m][2]*w[i][j][k],2)*/

		               +4.5*pow_2*pow_2

		         -1.5*(u[i][j][k]*u[i][j][k]+v[i][j][k]*v[i][j][k]+w[i][j][k]*w[i][j][k]));

		      }

		else

		      {

		               pow_2=c[m][0]*u[i][j][k]+c[m][1]*v[i][j][k]+c[m][2]*w[i][j][k];

		      dfeq=1.0/36.0*rho[i][j][k]*(1.0+3.0*(c[m][0]*u[i][j][k]+c[m][1]*v[i][j][k]

		      +c[m][2]*w[i][j][k])

		               /*+4.5*pow(c[m][0]*u[i][j][k]+c[m][1]*v[i][j][k]+c[m][2]*w[i][j][k],2)*/

		               +4.5*pow_2*pow_2

		         -1.5*(u[i][j][k]*u[i][j][k]+v[i][j][k]*v[i][j][k]+w[i][j][k]*w[i][j][k]));

		      }



		      df1[i][j][k][m]=df1[i][j][k][m]*(1-1/tau)+1/tau*dfeq;

		  }



		/* streaming */

		/*for (i=2;i<=n1-2;i++) */

		for (i=s;i<=e;i++)

		   for (j=2;j<=n2-2;j++)

		      for (k=2;k<=n3-2;k++)

		      {

		      df2[i][j][k][0]    =df1[i][j][k][0];



		      df2[i+1][j][k][1]=df1[i][j][k][1];

		      df2[i-1][j][k][2]=df1[i][j][k][2];

		      df2[i][j][k+1][3]=df1[i][j][k][3];

		      df2[i][j][k-1][4]=df1[i][j][k][4];

		      df2[i][j-1][k][5]=df1[i][j][k][5];

		      df2[i][j+1][k][6]=df1[i][j][k][6];



		      df2[i+1][j][k+1][7]=df1[i][j][k][7];

		      df2[i-1][j][k-1][8]=df1[i][j][k][8];

		      df2[i+1][j][k-1][9]=df1[i][j][k][9];

		      df2[i-1][j][k+1][10]=df1[i][j][k][10];

		      df2[i][j-1][k+1][11]=df1[i][j][k][11];

		      df2[i][j+1][k-1][12]=df1[i][j][k][12];

		      df2[i][j+1][k+1][13]=df1[i][j][k][13];

		      df2[i][j-1][k-1][14]=df1[i][j][k][14];

		      df2[i+1][j-1][k][15]=df1[i][j][k][15];

		      df2[i-1][j+1][k][16]=df1[i][j][k][16];

		      df2[i+1][j+1][k][17]=df1[i][j][k][17];

		      df2[i-1][j-1][k][18]=df1[i][j][k][18];

		      }


		#ifdef DEBUG_PRINT
		printf("Compute Node %d Generator start LBM data sending and receiving !\n", rank);
		fflush(stdout);
		#endif //DEBUG_PRINT

		/* data sending and receiving*/

		MPI_Sendrecv(&df2[e+1][0][0][1],1,newtype,nbright,1,&df2[s][0][0][1],1,newtype,nbleft,1,comm1d,&status);
		// MPI_Barrier(comm1d);
		MPI_Sendrecv(&df2[e+1][0][0][7],1,newtype,nbright,7,&df2[s][0][0][7],1,newtype,nbleft,7,comm1d,&status);
		// MPI_Barrier(comm1d);
		MPI_Sendrecv(&df2[e+1][0][0][9],1,newtype,nbright,9,&df2[s][0][0][9],1,newtype,nbleft,9,comm1d,&status);
		// MPI_Barrier(comm1d);
		MPI_Sendrecv(&df2[e+1][0][0][15],1,newtype,nbright,15,&df2[s][0][0][15],1,newtype,nbleft,15,comm1d,&status);
		// MPI_Barrier(comm1d);
		MPI_Sendrecv(&df2[e+1][0][0][17],1,newtype,nbright,17,&df2[s][0][0][17],1,newtype,nbleft,17,comm1d,&status);
		// MPI_Barrier(comm1d);


		MPI_Sendrecv(&df2[s-1][0][0][2],1,newtype,nbleft,2,&df2[e][0][0][2],1,newtype,nbright,2,comm1d,&status);
		// MPI_Barrier(comm1d);
		MPI_Sendrecv(&df2[s-1][0][0][8],1,newtype,nbleft,8,&df2[e][0][0][8],1,newtype,nbright,8,comm1d,&status);
		// MPI_Barrier(comm1d);
		MPI_Sendrecv(&df2[s-1][0][0][10],1,newtype,nbleft,10,&df2[e][0][0][10],1,newtype,nbright,10,comm1d,&status);
		// MPI_Barrier(comm1d);
		MPI_Sendrecv(&df2[s-1][0][0][16],1,newtype,nbleft,16,&df2[e][0][0][16],1,newtype,nbright,16,comm1d,&status);
		// MPI_Barrier(comm1d);
		MPI_Sendrecv(&df2[s-1][0][0][18],1,newtype,nbleft,18,&df2[e][0][0][18],1,newtype,nbright,18,comm1d,&status);
		// MPI_Barrier(comm1d);


		/*sending  and receiving data for boundary conditions for df1[][][][]u*/

		MPI_Sendrecv(&df1[e][0][2][9],1,newtype_bt,nbright,99,

		           &df1[s-1][0][2][9],1,newtype_bt,nbleft,99,comm1d,&status);
		// MPI_Barrier(comm1d);
		MPI_Sendrecv(&df1[e][0][n3-2][7],1,newtype_bt,nbright,77,

		           &df1[s-1][0][n3-2][7],1,newtype_bt,nbleft,77,comm1d,&status);
		// MPI_Barrier(comm1d);


		MPI_Sendrecv(&df1[e][2][0][15],1,newtype_fr,nbright,1515,

		           &df1[s-1][2][0][15],1,newtype_fr,nbleft,1515,comm1d,&status);
		// MPI_Barrier(comm1d);
		MPI_Sendrecv(&df1[e][n2-2][0][17],1,newtype_fr,nbright,1717,

		           &df1[s-1][n2-2][0][17],1,newtype_fr,nbleft,1717,comm1d,&status);
		// MPI_Barrier(comm1d);






		MPI_Sendrecv(&df1[s][0][2][8],1,newtype_bt,nbleft,88,

		           &df1[e+1][0][2][8],1,newtype_bt,nbright,88,comm1d,&status);
		// MPI_Barrier(comm1d);
		MPI_Sendrecv(&df1[s][0][n3-2][10],1,newtype_bt,nbleft,1010,

		           &df1[e+1][0][n3-2][10],1,newtype_bt,nbright,1010,comm1d,&status);
		// MPI_Barrier(comm1d);


		MPI_Sendrecv(&df1[s][2][0][18],1,newtype_fr,nbleft,1818,

		           &df1[e+1][2][0][18],1,newtype_fr,nbright,1818,comm1d,&status);
		// MPI_Barrier(comm1d);
		MPI_Sendrecv(&df1[s][n2-2][0][16],1,newtype_fr,nbleft,1616,

		           &df1[e+1][n2-2][0][16],1,newtype_fr,nbright,1616,comm1d,&status);
		// MPI_Barrier(comm1d);

		#ifdef DEBUG_PRINT
		printf("Compute Node %d Generator finish LBM data sending and receiving !\n",rank);
		fflush(stdout);
		#endif //DEBUG_PRINT

		if (myid==left_most) s=3;

		if (myid==right_most) e=n1-3;

		// MPI_Barrier(comm1d);

		/* boundary conditions */



		/* 1.1 bounce back and reflection condition on the bottom */

		k=2;

		/*for (i=3;i<=n1-3;i++) */

		for (i=s;i<=e;i++)

		   for (j=2;j<=n2-2;j++)

		{

		df2[i][j][k][3]=df1[i][j][k][4];

		df2[i][j][k][7]=p_bb*df1[i][j][k][8]+(1.0-p_bb)*df1[i-1][j][k][9];

		df2[i][j][k][10]=p_bb*df1[i][j][k][9]+(1.0-p_bb)*df1[i+1][j][k][8];

		df2[i][j][k][11]=p_bb*df1[i][j][k][12]+(1.0-p_bb)*df1[i][j+1][k][14];

		df2[i][j][k][13]=p_bb*df1[i][j][k][14]+(1.0-p_bb)*df1[i][j-1][k][12];

		}



		/* 1.2 bounce back and reflection condition on the top*/

		k=n3-2;

		/* for (i=3;i<=n1-3;i++)*/

		for (i=s;i<=e;i++)

		   for (j=2;j<=n2-2;j++)

		{

		df2[i][j][k][4]=df1[i][j][k][3];

		df2[i][j][k][8]=p_bb*df1[i][j][k][7]+(1.0-p_bb)*df1[i+1][j][k][10];

		df2[i][j][k][9]=p_bb*df1[i][j][k][10]+(1.0-p_bb)*df1[i-1][j][k][7];

		df2[i][j][k][12]=p_bb*df1[i][j][k][11]+(1.0-p_bb)*df1[i][j-1][k][13];

		df2[i][j][k][14]=p_bb*df1[i][j][k][13]+(1.0-p_bb)*df1[i][j+1][k][11];

		}



		/* 1.3 bounce back and reflection condition on the front*/

		j=2;

		/*for (i=3;i<=n1-3;i++) */

		for (i=s;i<=e;i++)

		   for (k=2;k<=n3-2;k++)

		  {

		  df2[i][j][k][6]=df1[i][j][k][5];

		        df2[i][j][k][12]=p_bb*df1[i][j][k][11]+(1.0-p_bb)*df1[i][j][k+1][14];

		  df2[i][j][k][13]=p_bb*df1[i][j][k][14]+(1.0-p_bb)*df1[i][j][k-1][11];

		  df2[i][j][k][16]=p_bb*df1[i][j][k][15]+(1.0-p_bb)*df1[i+1][j][k][18];

		  df2[i][j][k][17]=p_bb*df1[i][j][k][18]+(1.0-p_bb)*df1[i-1][j][k][15];

		  }





		/* 1.4 bounce back and reflection condition on the rear*/

		j=n2-2;

		/*for (i=3;i<=n1-3;i++)*/

		for (i=s;i<=e;i++)

		   for (k=2;k<=n3-2;k++)

		  {

		  df2[i][j][k][5]=df1[i][j][k][6];

		        df2[i][j][k][11]=p_bb*df1[i][j][k][12]+(1.0-p_bb)*df1[i][j][k-1][13];

		  df2[i][j][k][14]=p_bb*df1[i][j][k][13]+(1.0-p_bb)*df1[i][j][k+1][12];

		  df2[i][j][k][15]=p_bb*df1[i][j][k][16]+(1.0-p_bb)*df1[i-1][j][k][17];

		  df2[i][j][k][18]=p_bb*df1[i][j][k][17]+(1.0-p_bb)*df1[i+1][j][k][16];

		  }



		s=2;e=n1-2;

		// MPI_Barrier(comm1d);




		if(myid==left_most) s=3;

		if(myid==right_most) e=n1-3;

		// MPI_Barrier(comm1d);




		/* compute rho and (u,v) from distribution function */

		/* for (i=3;i<=n1-3;i++) */

		for (i=s;i<=e;i++)

		   for (j=2;j<=n2-2;j++)

		      for (k=2;k<=n3-2;k++)

		      {

		      s1=df2[i][j][k][0];

		      s2=c[0][0]*df2[i][j][k][0];

		      s3=c[0][1]*df2[i][j][k][0];

		      s4=c[0][2]*df2[i][j][k][0];



		      for (m=1;m<=18;m++)

		      {

		      s1+=df2[i][j][k][m];

		      s2+=c[m][0]*df2[i][j][k][m];

		      s3+=c[m][1]*df2[i][j][k][m];

		      s4+=c[m][2]*df2[i][j][k][m];

		      }



		          rho[i][j][k]=s1;

		          u[i][j][k]=s2/s1;

		          v[i][j][k]=s3/s1;

		      w[i][j][k]=s4/s1;

		      }





		if (myid==left_most) {

		/* 2. inlet and outlet conditions */

		/* inlet */

		i=2;

		for (j=1;j<=n2-1;j++)

		   for (k=1;k<=n3-1;k++)

		      for (m=0;m<=18;m++)

		  {

		   df2[i][j][k][m]=df_inout[0][j][k][m];

		      }

		}



		if (myid==right_most) {

		/* outlet */

		i=n1-2;

		for (j=1;j<=n2-1;j++)

		   for (k=1;k<=n3-1;k++)

		      for (m=0;m<=18;m++)

		{

		df2[i][j][k][m]=df_inout[1][j][k][m];

		}

		}



		for (i=s;i<=e;i++)

		   for (k=2;k<=n3-2;k++)

		      for (m=0;m<=18;m++)

		{df2[i][1][k][m]=df2[i][n2-3][k][m];

		 df2[i][n2-1][k][m]=df2[i][3][k][m];}





		/* along z-direction, z=1 & n3-1 */

		/*for (i=3;i<=n1-3;i++)*/

		for (i=s;i<=e;i++)

		   for (j=2;j<=n2-2;j++)

		      for (m=0;m<=18;m++)

		  {df2[i][j][1][m]=df2[i][j][n3-3][m];

		         df2[i][j][n3-1][m]=df2[i][j][3][m];}



		/* replacing the old d.f. values by the newly compuited ones */

		if (myid==left_most) s=1;

		if(myid==right_most) e=n1-1;

		/* for (i=1;i<=n1-1;i++) */

		for (i=s;i<=e;i++)

		   for (j=1;j<=n2-1;j++)

		      for (k=1;k<=n3-1;k++)

		         for (m=0;m<=18;m++)

		          {df1[i][j][k][m]=df2[i][j][k][m];}

		t6=get_cur_time();
		only_lbm_time+=t6-t5;

		#ifdef DEBUG_PRINT
		printf("Compute Node %d Generator start create_prb_element!\n",rank);
		fflush(stdout);
		#endif //DEBUG_PRINT

		/*create_prb_element*/
		double * buffer;
		// FILE *fp;
		// char file_name[64];
		int count=0;

        // if use adios, not partioning is required
        buffer = (double *) malloc(n*sizeof(double)*2);
        check_malloc(buffer);

        // fill the buffer, each line has two double data
        for(gi = 0; gi < nx; gi++){
            for(gj = 0; gj < ny; gj++){
                for(gk = 0; gk < nz; gk++){
		            *((double *)(buffer+count))=u_r*u[gi][gj][gk];
		            *((double *)(buffer+count+1))=u_r*v[gi][gj][gk];
#ifdef debug_1
                    printf("(%d %d %d), u_r=_%lf u=%lf v= %lf\n", gi, gj, gk, u_r, u[gi][gj][gk],v[gi][gj][gk]);
#endif
		            count+=2;
                }
            }
        }
        
        /*******************************
         *          ADIOS              *
         *******************************/
        // n can be large (64*64*256)
        insert_into_Adios(filepath, n, buffer, &comm);
        free(buffer);
		//free(buffer);
		#ifdef DEBUG_PRINT
		if(step%10==0)
		  printf("Node %d step = %d\n", rank, step);
		#endif //DEBUG_PRINT
		time1=0;

		step+=dt;

		time1+=dt;

		time2+=dt;

		time3+=dt;


		// MPI_Barrier(comm1d);



		}  /* end of while loop */

		// MPI_Barrier(comm1d);
		t3= get_cur_time();

		MPI_Comm_free(&comm1d);
		MPI_Type_free(&newtype);
		MPI_Type_free(&newtype_bt);
		MPI_Type_free(&newtype_fr);
}

int main(int argc, char * argv[]){
    if(argc !=2){
        printf("need to specify scratch path for i/o\n");
        exit(-1);
    }
    char filepath[256];
    strcpy(filepath, argv[1]);
    int dims_cube[3] = {4,4,4};

	MPI_Init(&argc, &argv);

  MPI_Comm comm = MPI_COMM_WORLD;
  int         rank, size, i, j;
  MPI_Comm_rank (comm, &rank);
  MPI_Comm_size (comm, &size);

#ifdef USE_ADIOS
  adios_init ("adios_global.xml", comm);
  printf("rank %d: adios init complete\n", rank);
  if(rank == 0 ){
      printf("output will be saved in %s\n", filepath);
  }
#endif

  run_lbm(filepath, 100, dims_cube, &comm);

#ifdef USE_ADIOS
  adios_finalize (rank);
  printf("rank %d: adios finalize complete\n", rank); 
#endif                                                      
  MPI_Finalize();

  printf("rank %d: exit\n", rank);

    return 0;
}

