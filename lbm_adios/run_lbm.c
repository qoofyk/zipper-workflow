#include "run_lbm.h"        
//#include "adios_write_global.h"
#include "adios_adaptor.h"
#include "utility.h"
#include "adios_error.h"
#include "ds_adaptor.h"

#include "transports.h"
static transport_method_t transport;

#define debug

/*
 * Native staging need to use these
 */
static char var_name[STRING_LENGTH];
static size_t elem_size=sizeof(double);
    



void run_lbm(char * filepath, int step_stop, int dims_cube[3], MPI_Comm *pcomm)
{

// aditional timer for staging
#ifdef ENABLE_TIMING
    double t7=0, t8=0;
    double t_write=0;
    // actual time by dspaces put
    double t_put=0;
    double t_buffer =0;
#endif
        int gi, gj, gk, nx,ny,nz;

        int nprocs, rank;
        int n;
        MPI_Comm comm = *pcomm;
		MPI_Comm_size(comm,&nprocs);
		MPI_Comm_rank(comm,&rank);

        nx = dims_cube[0];
        ny = dims_cube[1];
        nz = dims_cube[2];
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


        /*
         * get transport method
         */
        uint8_t transport_major = get_major(transport);
        uint8_t transport_minor = get_minor(transport);


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

		t2 = MPI_Wtime();

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

		int step=0;

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


		t4=MPI_Wtime();
		init_lbm_time=t4-t2;
		only_lbm_time+=t4-t2;
		if(myid==0){
			printf("Init LBM Time=%f\n", init_lbm_time);
			fflush(stdout);
		}

		//int blk_id=0;

		while (step < step_stop)

		{

		t5=MPI_Wtime();

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

		t6=MPI_Wtime();
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

        t7 = MPI_Wtime();
		t_buffer+=t7-t6;


        
        /*******************************
         *          ADIOS              *
         *******************************/
        // n can be large (64*64*256)


       

    if(transport_major == ADIOS_STAGING){
        // for staging, each time write to same file
        //
        insert_into_adios(filepath, "atom",-1, n, SIZE_ONE , buffer,"w", &comm);
        /*
        if(step ==0){
            insert_into_adios(filepath, "atom",-1, n, SIZE_ONE , buffer,"w", &comm);
        }
        else{

            insert_into_adios(filepath, "atom",-1, n, SIZE_ONE , buffer,"a", &comm);
        }
        */
    }
    else if(transport_major == ADIOS_DISK){
        // for mpiio, each time write different files
        insert_into_adios(filepath, "atom", step, n, SIZE_ONE , buffer,"w", &comm);
        /**** use index file to keep track of current step *****/
        int fd; 
        char step_index_file[256];
        int time_stamp;
        
        if(rank == 0){
            if(step == step_stop - 1){
               time_stamp = -2;
            }else{
               time_stamp = step;
            }// flag read from producer

            sprintf(step_index_file, "%s/stamp.file", filepath);

            printf("step index file in %s \n", step_index_file);

            fd = open(step_index_file, O_WRONLY|O_CREAT|O_SYNC, S_IRWXU);
            if(fd < 0){
                perror("indexfile not opened");
                exit(-1);
            }
            else{
                flock(fd, LOCK_EX);
                write(fd,  &time_stamp,  sizeof(int));
                flock(fd, LOCK_UN);
                printf("write stamp %d at %lf", time_stamp, MPI_Wtime());
                close(fd);
            }
        }
        // wait until all process finish writes and 
        MPI_Barrier(comm);
    }

    else if(transport_major == NATIVE_STAGING){
        int bounds[6] = {0};
        double time_comm;

        // xmin
        bounds[1]=n*rank;
        // ymin
        bounds[0]=0;

        // xmax
        bounds[4]=n*(rank+1)-1 ;
        // ymax
        bounds[3]=1;

        put_common_buffer(transport_minor, step,2, bounds,rank , var_name, (void **)&buffer, elem_size, &time_comm);
        

        t_put+=time_comm;
     }

        free(buffer);

#ifdef ENABLE_TIMING
        t8 = MPI_Wtime();
        printf("rank %d: Step %d t_lbm %lf, t_write %lf, time %lf\n", rank, step, t6-t5,t8-t7, t8);
        t_write += t8-t7;
#endif

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

/*
         * dimes needs to flush last step
         */
        if(transport_minor == DIMES){
            char lock_name[STRING_LENGTH];
            snprintf(lock_name, STRING_LENGTH, "%s_lock", var_name);
            dspaces_lock_on_write(lock_name, &comm);
            dimes_put_sync_all();
            dspaces_unlock_on_write(lock_name, &comm);
            printf("rank %d: step %d last step flushed\n", rank, step);

        }

        double global_t_cal=0;
        double global_t_write=0;
        double global_t_put=0;
        MPI_Reduce(&only_lbm_time, &global_t_cal, 1, MPI_DOUBLE, MPI_SUM, 0, comm);
        MPI_Reduce(&t_write, &global_t_write, 1, MPI_DOUBLE, MPI_SUM, 0, comm);
        MPI_Reduce(&t_put, &global_t_put, 1, MPI_DOUBLE, MPI_SUM, 0, comm);

        if(rank == 0){
            //printf("t_prepare:%f s, t_cal %f s,t_buffer = %f, t_write %f s, t_put %f s\n", rank,init_lbm_time, only_lbm_time,t_buffer, t_write, t_write_2);
            printf("t_prepare:%f s, t_cal %f s,t_buffer = %f, t_write %f s, t_put %f s\n", init_lbm_time, global_t_cal/nprocs ,t_buffer, global_t_write/nprocs, global_t_put/nprocs);
        }

		// MPI_Barrier(comm1d);
		t3= MPI_Wtime();

		MPI_Comm_free(&comm1d);
		MPI_Type_free(&newtype);
		MPI_Type_free(&newtype_bt);
		MPI_Type_free(&newtype_fr);
}

int main(int argc, char * argv[]){
    if(argc !=4){
        printf("run_lbm nstop total_file_size scratch_path\n");
        exit(-1);
    }
    char filepath[256];
    int nstop; //run how many steps

    nstop = atoi(argv[1]);
    int filesize2produce = atoi(argv[2]);
    int dims_cube[3] = {filesize2produce/4,filesize2produce/4,filesize2produce};
    strcpy(filepath, argv[3]);

	MPI_Init(&argc, &argv);

  MPI_Comm comm = MPI_COMM_WORLD;
  int         rank, nprocs;
  MPI_Comm_rank (comm, &rank);
  MPI_Comm_size (comm, &nprocs);
  char nodename[256];
  int nodename_length;
    MPI_Get_processor_name(nodename, &nodename_length );

    /*
     * get transport method from env variable
     */
    transport = get_current_transport();
    uint8_t transport_major = get_major(transport);
    uint8_t transport_minor = get_minor(transport);
    printf("%s:I am rank %d of %d, tranport code %x-%x\n",
            nodename, rank, nprocs,
            get_major(transport), get_minor(transport) );


  if(transport_major == ADIOS_DISK || transport_major == ADIOS_STAGING){

      char xmlfile[256], trans_method[256];

      if(transport_major == ADIOS_DISK){
        strcpy(trans_method, "mpiio");
      }
      else{
          if(transport_minor == DSPACES)
               strcpy(trans_method, "dataspaces");
          else if(transport_minor == DIMES)
               strcpy(trans_method, "dimes");

          else if(transport_minor == FLEXPATH)
               strcpy(trans_method, "flexpath");
      }

      sprintf(xmlfile,"adios_xmls/dbroker_%s.xml", trans_method);
      printf("[r%d] try to init with %s\n", rank, xmlfile);
      if(adios_init (xmlfile, comm) != 0){
        printf("[r%d] ERROR: adios init err with %s\n", rank, trans_method);
        printf("[r%d] ERR: %s\n", rank, adios_get_last_errmsg());
        return -1;
      }
      else{
          //if(rank ==0)
            printf("rank %d : adios init complete with %s\n", rank, trans_method);
      }
      MPI_Barrier(comm);
  } //use ADIOS_DISK or ADIOS_STAGING

  else  if(transport_major == NATIVE_STAGING){
        char msg[STRING_LENGTH];
        int ret = -1;
        printf("trying init dspaces for %d process\n", nprocs);
        ret = dspaces_init(nprocs, 1, &comm, NULL);

        printf("dspaces init successfuly \n");

        if(ret == 0){
            sprintf(msg, "dataspaces init successfully");
            my_message(msg, rank, LOG_CRITICAL);
        }else{
            sprintf(msg, "dataspaces init error");
            my_message(msg, rank, LOG_CRITICAL);
            exit(-1);
        }

        /*
        * set bounds and dspaces variables
        */
        sprintf(var_name, "atom");


        // data layout
//#ifdef FORCE_GDIM
        int n = dims_cube[0]*dims_cube[1]*dims_cube[2];
        //uint64_t gdims[2] = {2, n*nprocs};
        //dspaces_define_gdim(var_name, 2,gdims);
//#endif
   }



  if(rank == 0 ){
      printf("output will be saved in %s\n", filepath);
  }

  MPI_Barrier(comm);
  double t_start = MPI_Wtime();
  if(rank == 0){
      printf("stat:Simulation start at %lf \n", t_start);
      printf("stat:FILE2PRODUCE=%d, NSTOP= %d \n", filesize2produce, nstop);
  }
  run_lbm(filepath, nstop, dims_cube, &comm);

  MPI_Barrier(comm);
  double t_end = MPI_Wtime();
  if(rank == 0){
      printf("stat:Simulation stop at %lf \n", t_end);
  }

if(transport_major == ADIOS_DISK || transport_major == ADIOS_STAGING){
  adios_finalize (rank);
  printf("rank %d: adios finalize complete\n", rank); 
}

else if(transport_major == NATIVE_STAGING){
    dspaces_finalize();
}

  MPI_Finalize();
  printf("rank %d: exit\n", rank);
  return 0;
}

