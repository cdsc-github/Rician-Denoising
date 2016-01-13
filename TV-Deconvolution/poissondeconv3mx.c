/*========================================================================
 *
 * POISSONDECONV3MX.C  3D TV minimization for Poisson deconvolution
 *
 * u = poissondeconv3mx(f,K,lambda,NumIter,dt) performs deconvolution on a
 * 3D volume f with Poisson noise.  The deblurred image u is found as the
 * minimizer of 
 *
 *         /                      / 
 *    min  | |grad u| dx + lambda | [ K*u - f log K*u ] dx.
 *     u   /                      / 
 *
 * Parameter lambda >= 0 determines the strength of the denoising: smaller
 * lambda implies stronger denoising.  NumIter specifies the number of 
 * iterations and dt specifies the timestep.  Parameter dt must be
 * sufficiently small for stability (typically on the order of 0.0001).
 *
 * Pascal Getreuer 2009
 *
 *======================================================================*/
#include <math.h>
#include <string.h>
#include "mex.h"

#include "gaussianblur.c"

/* Method Parameters */
#define DEFAULT_NUMITER  10
#define DEFAULT_DT       0.0001
#define EPSILON          1.0E-10
#define EPSILON2         1.0E-5

/* Macro functions */
#define	ISSCALAR(P)	(mxIsDouble(P) && mxGetM(P) == 1 && mxGetN(P) == 1)
#define SQR(x) ((x)*(x))

/* Macros for referring to pixel neighbors */
#define CENTER   (n[0]+Size[0]*(n[1]+Size[1]*n[2]))
#define RIGHT    (n[0]+Size[0]*(n[1]+Size[1]*n[2])+Size[0])
#define LEFT     (n[0]+Size[0]*(n[1]+Size[1]*n[2])-Size[0])
#define DOWN     (n[0]+Size[0]*(n[1]+Size[1]*n[2])+1)
#define UP       (n[0]+Size[0]*(n[1]+Size[1]*n[2])-1)
#define ZOUT     (n[0]+Size[0]*(n[1]+Size[1]*n[2]+Size[1]))
#define ZIN      (n[0]+Size[0]*(n[1]+Size[1]*n[2]-Size[1]))

static void poissondeconv3(double *u, const double *f, const int Size[3],
double Ksigma, double lambda, int NumIter, double dt)
{
    const int NumEl = Size[0]*Size[1]*Size[2];
    double *g;       /* Array storing 1/|grad u| approximation */
    double *conv;    /* Array storing convolutions */
    int n[3];
    int Iter;    
    
    
    /* Allocate temporary work arrays */        
    g = mxCalloc(NumEl, sizeof(double)); 
    conv = mxCalloc(NumEl, sizeof(double)); 
        
    /*** Main gradient descent loop ***/
    
    for(Iter = 1; Iter <= NumIter; Iter++)
    {               
        /* Approximate g = 1/|grad u| */
        for(n[2] = 1; n[2] < Size[2]-1; ++n[2])
            for(n[1] = 1; n[1] < Size[1]-1; ++n[1])
                for(n[0] = 1; n[0] < Size[0]-1; ++n[0])
                    g[CENTER] = 1.0/sqrt( EPSILON
                       + SQR(u[CENTER] - u[RIGHT])
                       + SQR(u[CENTER] - u[LEFT])
                       + SQR(u[CENTER] - u[DOWN])
                       + SQR(u[CENTER] - u[UP])
                       + SQR(u[CENTER] - u[ZOUT])
                       + SQR(u[CENTER] - u[ZIN]));        

        memcpy(conv,u,NumEl*sizeof(double));
        GaussianBlur(conv,Size,Ksigma);
        
        for(n[2] = 0; n[2] < Size[2]; ++n[2])
            for(n[1] = 0; n[1] < Size[1]; ++n[1])
                for(n[0] = 0; n[0] < Size[0]; ++n[0])
                    conv[CENTER] = f[CENTER]/(EPSILON2 + conv[CENTER]) - 1.0;
        
        GaussianBlur(conv,Size,Ksigma);
        
        /* Update u by a sem-implict step */
        for(n[2] = 1; n[2] < Size[2]-1; n[2]++)
            for(n[1] = 1; n[1] < Size[1]-1; n[1]++)
                for(n[0] = 1; n[0] < Size[0]-1; n[0]++)
                {
                    u[CENTER] = ( u[CENTER] + dt*(u[RIGHT]*g[RIGHT]
                       + u[LEFT]*g[LEFT] + u[DOWN]*g[DOWN] + u[UP]*g[UP]
                       + u[ZOUT]*g[ZOUT] + u[ZIN]*g[ZIN] 
                       + lambda*conv[CENTER]) ) /
                    (1.0 + dt*(g[RIGHT] + g[LEFT] 
                    + g[DOWN] + g[UP] + g[ZOUT] + g[ZIN]));
                }        
    }
            
    /* Free temporary arrays */    
    mxFree(conv);
    mxFree(g);
    return;
}


/* MEX gateway function, interface between C and MATLAB */
void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray*prhs[])
{ 
    /* Input and ouput arguments */
    #define	F_IN	    prhs[0]
    #define	K_IN	    prhs[1]    
    #define	LAMBDA_IN	prhs[2]
    #define	NUMITER_IN	prhs[3]
    #define	DT_IN	    prhs[4]
    #define	U_OUT	    plhs[0]
    double Ksigma, lambda, NumIter, dt;
    double *u;
    const int *Size;
    
       
    /* Input checking */
    if(nrhs < 3)
        mexErrMsgTxt("Three input arguments required.");
    else if(nlhs > 1)
        mexErrMsgTxt("Too many output arguments.");
    if(!mxIsDouble(F_IN) || mxIsComplex(F_IN) || 
    mxGetNumberOfDimensions(F_IN) !=3 || mxIsSparse(F_IN))                
        mexErrMsgTxt("Image f must be a 3D real double array.");
    if(!ISSCALAR(K_IN))
        mexErrMsgTxt("Invalid input.");    
    if(!ISSCALAR(LAMBDA_IN) || !ISSCALAR(NUMITER_IN))
        mexErrMsgTxt("Invalid input.");
    
    Ksigma = mxGetScalar(K_IN);
    lambda = mxGetScalar(LAMBDA_IN);
    
    if(nrhs < 4)
        NumIter = DEFAULT_NUMITER;
    else if(!ISSCALAR(NUMITER_IN))
        mexErrMsgTxt("Invalid input.");    
    else 
        NumIter = mxGetScalar(NUMITER_IN);
    
    if(nrhs < 5)
        dt = DEFAULT_DT;
    else if(!ISSCALAR(DT_IN))
        mexErrMsgTxt("Invalid input.");    
    else 
        dt = mxGetScalar(DT_IN);
    
    if(Ksigma < 0 || lambda < 0 || dt <= 0)
        mexErrMsgTxt("Invalid input.");
    
    Size = mxGetDimensions(F_IN);
    /* Create output matrix */ 
    U_OUT = mxCreateDoubleMatrix(0,0,mxREAL);
    mxSetDimensions(U_OUT,Size,3);
    mxSetPr(U_OUT,u = mxMalloc(mxGetNumberOfElements(F_IN)*sizeof(double)));    
    memcpy(u,mxGetPr(F_IN),mxGetNumberOfElements(F_IN)*sizeof(double));
    
    /* Call the main denoising routine */
    poissondeconv3(u, mxGetPr(F_IN),
       Size, Ksigma, lambda, NumIter, dt);
    return;
}
