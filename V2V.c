#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "WirelessNetwork.c"
#include "Car.c"
#include <mpi.h>


#define NUMBER_OF_CARS 7 // number of processes simulating cars 

int main(int argc, char *argv[]){

    int size, currentRank;
    int networkRank = NUMBER_OF_CARS;
    int provided;
    // initialise MPI environment
    MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &currentRank);

    if (currentRank == networkRank){
        wirelessSim();
    }
    else{
        carSim(currentRank, networkRank);
    }
    
    MPI_Finalize();


}