#include <iostream>
#include <ctime>
#include <math.h>

#include <eigen/Eigen/Dense>
#include <cstdlib>
#include <random>
#define EIGEN_DONT_VECTORIZE
#define EIGEN_DISABLE_UNALIGNED_ARRAY_ASSERT

using Eigen::MatrixXd;
using Eigen::MatrixXi;
using Eigen::VectorXd;
using Eigen::VectorXi;

void translationSSA(double* kelong, double* t_array, int Nt, double kbind, double kcompl, int* SSA_intensity, int N, int FRAP, int Inhibitor, double inhibit_time, int seed, int fNt, int* frap_result,  int* x0, int r_footprint, int* SSA_probe, int Ncolor)
{
    // Declare the variables
	
	/*
	Inputs explaination: 
	
	kelong - a double 1xGeneLength vector of each codon at postition rate
	t_array - a time array of when to record the SSA_result
	Nt - the size of the t_array int
	kbind - ki the rate of initation
	kcompl - ktermination the rate of unbinding / release
	SSA_result - the result of the ssa, preallocated from python (N time points x 200 ribosomes x N trajectories)
	
	N - number trajectories
	FRAP - true false to do frap at inhibit time
	
	Inhibitor - True false to inhibit k_in at inhibit time
	inhibit_time - time of inhibition
	
	
	seed - RNG seed passed from python to ensure it doesnt use the same seeds for groups of trajectories - preallocated by python random vector
	rib_times - an array to be filled with the times of a Nribs to watch how long they take to go through the simulation
	nribs - the number of ribosomes to watch, this is to get around a dynamic array; basically the code will just watch "X" amount of ribosomes and record their data
	        instead of trying to do a dynamic array
	ribtimesize - the size of the ribosome watching array
	
	so to do the frap the simulation just returns the section that underwent frap, then the python wrapper will do the logic to bleach that section, as it gets complex 
	with ribosomes that were partially bleached
	
	fNt - the number of frap times, basically we need to record the section of the simulation that undergoes frap to manually "bleach it later" as the logic is complex
	frap_result - the area of the ssa result that undergoes frap 
	
	cNt - the size of the collision recording array 
	col_result - records collisions
	
	colNp - number of collisions to record as well - vectorized
	col_x - posistions of collisions
	col_t - time of said collision
	
	
	
	
	*/
	
	
	
    int R = r_footprint; // ribosome exclusion.
    int N_rib = 200; // maximum number of ribosomes. 
	//std::cout << "-------nrib=" << N_rib << "-------" << std::endl;
	
	
	srand(seed);
    int it = 0;
	int number_ribs = 0;
	int fit = 0;
	
	
    // int N = 10; // gene length
    //bool Inhibitor = 0;
    //bool FRAP = 0;
    double Inhibit_condition=1;
    bool inhibit_off=1;
    //double inhibit_time = 0;
	
	bool inhibitor_pres=0;
	bool FRAP_pres =0;
	
	if (Inhibitor==1){
		inhibitor_pres =1;
	}
	if (FRAP==1){
		FRAP_pres =1;
	}	
    int NR = 0;
	int old_NR = 0;
//	N = 10;
    double a0, r1, r2, t, tf;
    t=t_array[0];
    tf = t_array[Nt-1];
    int ind = 0;
    //srand (1537478928);  
	//std::cout << time(NULL) << std::endl;
    // print a test random number
    // Define the state vector
    MatrixXi X(1,N_rib);	
	
	
    X.setZero(1,N_rib);
	for(int i=0; i <= N_rib; i++)	{
		X(0,i) = x0[i];
	}
	
	

    // Create an eigen matrix that stores the results. 
    Eigen::Map<Eigen::VectorXi> X_array(SSA_intensity,Nt,Ncolor);

	
	
	
	Eigen::Map<Eigen::MatrixXi> frap_array(frap_result,fNt,N_rib);
	
	
	
	MatrixXi probe(Ncolor,N);
	probe.setZero(Ncolor,N);
	for (int i=0; i < Ncolor; i++){
		for (int j=0; j < N; j++){
			probe(i,j) = SSA_probe[i*N + j ]; 
			//std::cout << i << "," << j << " " << i*N + j << " " << SSA_probe[i*N + j ]  << std::endl;
		}
	}

    while( t < tf)
    {
        //std::cout << "-------t=" << t << "-------" << std::endl;
        // Determine inhibitor stuff
        if (inhibitor_pres) {
            if (t>=inhibit_time){
                inhibit_off = 0;
                Inhibit_condition = 0;
            } else {
                inhibit_off = 1; 
                Inhibit_condition=1;
            }}
        else { 
            Inhibit_condition=1;
        }


		
        // Update the number of ribosomes, always making sure to have
        // a zero on the right side of X (space for a new ribosome) 
		
        int NR = 0;
        while (X(0,NR)>0){
            NR+=1;
        }
		




        old_NR = 0;
        while (X(0,old_NR)>0){
            old_NR+=1;
			
        }
			
		
				
        //std::cout << "X: " << X << std::endl;
        MatrixXi Sn(NR+1,NR+1);
        Sn.setIdentity(NR+1,NR+1);
        //std::cout << "Stoichiometry Matrix: \n" << Sn << std::endl;
        Eigen::VectorXd wn;
        wn.setZero(NR+1);
        // for loop instead of "where"       
        // also, account for the exclusion here.
        for(int i=0; i <= NR; i++)
        {
            if( X(0,i) > 0 ){
                wn(i) = kelong[X(0,i)-1];
            }
            if( i>0){
                if( X(0,i-1)-X(0,i)<=R ){
					
                    wn(i) = 0;
                }
            }
        }

        // If a nascent protein reaches full length
        // add in the completion rate
//       std::cout  << X << std::endl;
        if( X(0,0) == N ){
            // shift everyone if completing
            for(int i=0; i <= NR-1; i++)
            {
                Sn(0,i) = X(0,i+1) - X(0,i); 
            }
            Sn(0,NR-1) = -X(0,NR-1);
            wn(0) = kcompl;
//            std::cout << "Updated propensity: \n" << wn << std::endl;
//            std::cout << "Updated stoichiometry: \n " << Sn << std::endl;
        }

        // include the initiation condition
        if( (NR==0) || ( X(0,NR-1)>R) ){
            wn(NR) = kbind*Inhibit_condition;
        }
        // Update the propensity
        a0 = wn.sum();

        // Generate some random numbers.
		
        r1 =  ((double) rand()/ (RAND_MAX));
        r2 =  ((double) rand() / (RAND_MAX));
		
		
		// if rand() gets a 0 resample, since ln(0) = -inf 
		if((r1==0)){
			//std::cout << r1 << " " << r1a << " " << t <<  std::endl;
			
			r1 =  ((double) rand() / (RAND_MAX));			
		}
		
		

        // Update the time vector
//        std::cout << "TIME " << t << std::endl;
        t -= log(r1)/a0;
		//std::cout << t << std::endl;
/* 		if((t > inhibit_time) && (t < inhibit_time+30)){
			std::cout << "TIME " << t << std::endl;
			std::cout << wn << std::endl;
			std::cout << X << std::endl;
		}
		 */
		
        //std::cout << "TIME " << t << std::endl;
  //      std::cout << "-------------" << std::endl;
        // Store the X vector
        //while( ((it<=Nt-1) || (t>t_array[it])) ){
        while( (it<=Nt-1) && (t>t_array[it])) {

			for(int j = 0; j < Ncolor; j++){

				int intensity = 0;
				for(int i=0; i < NR; i++){


					
					intensity += probe(j,X(0,i)-1);

				}

				X_array(it,j) = intensity;
				
				
				if (FRAP_pres){
					
					if ( (t>= inhibit_time) && (t< inhibit_time+20)) {
						frap_array.row(fit) = X.row(0);
						fit +=1;
					}
				}			
			}
            it+=1;
			//std::cout << it << std::endl;
			
         }
		 

		
        // update the state
        ind = 1;
        while (wn.head(ind).sum() < r2*a0)
        {	
            ind +=1;
        }
		
        //std::cout << "Stoichiometry of reaction: " << Sn.row(ind-1) << std::endl;
        
		
		
        X.topLeftCorner(1,NR+1) = X.topLeftCorner(1,NR+1) + Sn.row(ind-1);
		//std::cout << "rxn "<< Sn.row(ind-1).sum() << std::endl;
		
		//std::cout << "oneiter" << std::endl;

    }

	
        
}
 
/*
int main()
{
    //double k[2][5] = {{1, 2, 3, 4, 5},{6,7,8,9,10}};
    //double k[1][10] = {{3.1,3.2,3.3,3.4,3.5,3.1,3.2,3.3,3.4,3.5}};
    double k[10] = {3.1,3.2,3.3,3.4,3.5,3.1,3.2,3.3,3.4,3.5};
    double t_array[3] = {0, 50,100};
    double kbind = 9.0; 
    double kcompl = 15.0; 
	double inhibit_time = 10.0;
	int FRAP = 0;
	int Inhibitor = 1;
    int Nt;
    int result[10];
	double ribtimes[50];
	int seed;
    //seed = srand (time(NULL));
    Nt = (sizeof(t_array)/sizeof(*t_array));
    for(int kk=0; kk<3; kk++){
        translationSSA(k, t_array, 3, kbind, kcompl, result, 10, FRAP, Inhibitor, inhibit_time,15,ribtimes);
    }
}   

*/