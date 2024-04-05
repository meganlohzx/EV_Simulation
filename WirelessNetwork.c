#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <pthread.h>

#define NUMBER_OF_CARS 7 // number of processes simulating cars 


// MPI Tag Legend 
// Car --> Wireless Network
// 0 -- request for information 
// 1 -- Reply a request from another car
// 2 -- releasing all nodes in the reply list.

// Wireless --> Car
// 1 -- Replies from other cars (permitting this car)
// 2 -- Requests from other cars (to permit other cars)

int isAllDisconnected = 0; 
// 0 -- never entered the network before
// 1 -- in the network
// 2 -- left the network 

int numberOfConnectedCars = 0;


int isDisconnected(MPI_Request req){
    int dataReceived = 0;
    while (isAllDisconnected == 0 && dataReceived == 0) {
        MPI_Test(&req, &dataReceived, MPI_STATUS_IGNORE); //test again
    }
    if(!dataReceived){
        return 1;
    }
    return 0; 
}

// vehicles telling that they are leaving the network
void checkTerminate(int* carConnection){

    // check if any cars have disconnected from the network
    int* reply = (int*)malloc(sizeof(int)*2);
    MPI_Request req2;
    int* releaseList = (int*)malloc(sizeof(int)*NUMBER_OF_CARS);
    for(int i = 0; i < NUMBER_OF_CARS; i++){
        int probeFlag;

        // are any vehicles terminating?
        MPI_Iprobe(i, 2, MPI_COMM_WORLD, &probeFlag, MPI_STATUS_IGNORE);
        if(probeFlag){
            MPI_Recv(releaseList, NUMBER_OF_CARS, MPI_INT, i, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            carConnection[i] = 2; 

            //release 
            for(int j = 0; j < NUMBER_OF_CARS; j++){
                if(releaseList[j] != -1){
                    MPI_Send(&i, 1, MPI_INT, releaseList[j], 1, MPI_COMM_WORLD);
                } else{
                    break;
                }
            }
        }
    }

    // if all vehicles have disconnected from the network, we terminate.
    int allDisconnectedAux = 1;
    for(int i = 0; i < NUMBER_OF_CARS; i++){
        if(carConnection[i] != 2){
            allDisconnectedAux = 0; 
            break;
        }
    }
    if(allDisconnectedAux){
        isAllDisconnected = 1;
    }
}

// vehicles which want to request 
// also shows they are connected to the network
void* broadcastFunc(void *pArg){

    int* carConnection = pArg;

    while(!isAllDisconnected){
        for(int i = 0; i < NUMBER_OF_CARS; i++){
            int reqInfoFlag;

            //is any vehicle requesting for information?
            MPI_Iprobe(i, 0, MPI_COMM_WORLD, &reqInfoFlag, MPI_STATUS_IGNORE);
            if(reqInfoFlag){
                int* requestMsg = (int*)malloc(sizeof(int)*5);
                MPI_Recv(requestMsg, 5, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                // connected vehicles to the network
                for(int j = 0; j < NUMBER_OF_CARS; j++){
                    if(carConnection[j] == 1){
                        MPI_Send(requestMsg, 5, MPI_INT, j, 2, MPI_COMM_WORLD);
                    }
                }

                // send requests to other vehicles
                MPI_Send(carConnection, NUMBER_OF_CARS, MPI_INT, i, 0, MPI_COMM_WORLD);
                carConnection[i] = 1; 
            }
        }

        checkTerminate(carConnection);

    }
}

// replies received from other vehicles, sent back to the original vehicles
void* replyFunc(){
    while(!isAllDisconnected){

        for(int i = 0; i < NUMBER_OF_CARS; i++){
        int probeReply;
        int reply[2];

        // check if any vehicles are replying to any others
        MPI_Iprobe(i, 1, MPI_COMM_WORLD, &probeReply, MPI_STATUS_IGNORE);
            if(probeReply){
                MPI_Recv(reply, 2, MPI_INT, i, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Send(&reply[0], 1, MPI_INT, reply[1], 1, MPI_COMM_WORLD);
            }
        }
    }
}

int wirelessSim(){
    pthread_t tid[2];
    int* carConnection = (int*)calloc(NUMBER_OF_CARS, sizeof(int)); 

    // Fork		
    pthread_create(&tid[0], 0, broadcastFunc, carConnection);
    pthread_create(&tid[1], 0, replyFunc, NULL);

	// Join
	for(int i = 0; i < 2; i++)
	{
	    pthread_join(tid[i], NULL);
	}

    return 0;


}
