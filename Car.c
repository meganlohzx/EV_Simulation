#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>
#include <unistd.h>
#include <pthread.h>

#define SIMULATION_TIME 5 // 30 second simulation time. 
#define NUMBER_OF_CARS 7 // number of processes simulating cars 


//////////////////////////////////////////////////////////////////
// MPI Tag Legend 
// Car --> Wireless Network
// 0 -- request for information 
// 1 -- Reply a request from another car
// 2 -- releasing all nodes in the reply list.

// Wireless --> Car
// 0 -- Cars available in the network (wait for their replies)
// 1 -- Replies from other cars (permitting this car)
// 2 -- Requests from other cars (to permit other cars)
//////////////////////////////////////////////////////////////////

//Status of the car
enum Status {
  NA = 0, 
  WAITING = 1,
  PASSING = 2,
  EXITING = 3
};

struct threadData{
    int myRank;
    int WNRank;
    int arrLane;
    int* replyQueue;
    int* replyVehicles;
};

// Lanes are represented by numbers 
// 0 -- NF      4 -- SF
// 1 -- NL      5 -- SL
// 2 -- EF      6 -- WF
// 3 -- EL      7 -- WL
enum Status status = NA; 
int lamportTime = 0; // lamport timestamp
int timeOfArrival; // time that car arrived at intersection, based on lamport's timestamp
int laneTable[8][8] = {{0, 0, 1, 0, 0, 1, 1, 1}, // table of conflicting/non-conflicting lanes
                       {0, 0, 1, 1, 1, 0, 0, 1},
                       {1, 1, 0, 0, 1, 0, 0, 1},
                       {0, 1, 0, 0, 1, 1, 1, 0},
                       {0, 1, 1, 1, 0, 0, 1, 0},
                       {1, 0, 0, 1, 0, 0, 1, 1},
                       {1, 0, 0, 1, 1, 1, 0, 0},
                       {1, 1, 1, 0, 0, 1, 0, 0}};

int isExited(MPI_Request req){
    int dataReceived = 0;
    while (status != 3 && dataReceived == 0) {
        MPI_Test(&req, &dataReceived, MPI_STATUS_IGNORE); //test again
    }
    if(!dataReceived){
        return 1;
    }
    return 0; 
}

int isConflict(int arrLane, int otherLane){
    if(laneTable[arrLane][otherLane] == 1){
        return 1;
    }else{
        return 0;
    }
}

// Algorithm 1
void requestMsg(int id, int arrLane, int* replyVehicles, int WNRank){
    
    int* requestMsg = (int*)malloc(sizeof(int)*5);
    requestMsg[0] = id;
    requestMsg[1] = arrLane;
    requestMsg[2] = -arrLane; // destination lane 

    status = WAITING;
    lamportTime = lamportTime + 1;
    timeOfArrival = lamportTime;

    requestMsg[3] = timeOfArrival; // arrival time
    requestMsg[4] = lamportTime; // timestamp

    MPI_Sendrecv(requestMsg, 5, MPI_INT, WNRank, 0, replyVehicles, 
    NUMBER_OF_CARS, MPI_INT, WNRank, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

// Algorithm 2
void* replyMsg(void *pArg){

    struct threadData *replyThreadData;
    replyThreadData = (struct threadData *) pArg;
    int myRank = replyThreadData->myRank;
    int WNRank = replyThreadData->WNRank;
    int arrLane = replyThreadData->arrLane;
    int* replyQueue = replyThreadData->replyQueue;
    int* replyVehicles = replyThreadData->replyVehicles;


    int replyQueueIndex = 0;


    while(status != 3){
        int requestingNodeInfo[5];
        MPI_Request req;
        MPI_Irecv(&requestingNodeInfo, 5, MPI_INT, WNRank, 2, MPI_COMM_WORLD, &req);

        if(isExited(req)){
            break;
        }
        int* reply = (int*)malloc(sizeof(int)*2);
        reply[0] = myRank; 
        reply[1] = requestingNodeInfo[0];
        lamportTime = (requestingNodeInfo[4] > lamportTime ? 
        requestingNodeInfo[4]  :  lamportTime) + 1;

        // if passing (i.e., core of intersection)
        if(status == 2){
            if(isConflict(arrLane,requestingNodeInfo[1])){
                replyQueue[replyQueueIndex] = requestingNodeInfo[0];
                replyQueueIndex += 1;
            }
            else{
                MPI_Send(reply, 2, MPI_INT, WNRank, 1, MPI_COMM_WORLD);
            }

        } else {
            if(!isConflict(arrLane,requestingNodeInfo[1])){
                MPI_Send(reply, 2, MPI_INT, WNRank, 1, MPI_COMM_WORLD);
            } else {
                if(timeOfArrival > requestingNodeInfo[3]){
                    MPI_Send(reply, 2, MPI_INT, WNRank, 1, MPI_COMM_WORLD);
                } else {
                    replyQueue[replyQueueIndex] = requestingNodeInfo[0];
                    replyQueueIndex += 1;
                }
            }
        }   
    }
} 

void replyAll(int currentRank, int* replyVehicles, int WNRank){
    int numberOfVehicles = 0;
    int replyID; 

    for (int i = 0; i< NUMBER_OF_CARS; i++){
        if(replyVehicles[i] == 1){
            numberOfVehicles += 1;
        }
    }
    
    MPI_Request allReq[numberOfVehicles];
    for (int i = 0; i < numberOfVehicles; i++){
        MPI_Irecv(&replyID, 1, MPI_INT, WNRank, 1, MPI_COMM_WORLD, &allReq[i]);
    }

    MPI_Waitall(numberOfVehicles, allReq, MPI_STATUSES_IGNORE);
    
}

// Algorithm 3
void* releaseMsg(void *pArg){
    
    struct threadData *myThreadData;
    myThreadData = (struct threadData *) pArg;
    int myRank = myThreadData->myRank;
    int WNRank = myThreadData->WNRank;
    int arrLane = myThreadData->arrLane;
    int* replyQueue = myThreadData->replyQueue;
    int* replyVehicles = myThreadData->replyVehicles;

    // waiting for REPLYALL
    replyAll(myRank, replyVehicles, WNRank);

    // passing into core of intersection
    if(status != 2){
        status = PASSING;
        
        time_t t3 = time(NULL);
        struct tm timeinfo3 = *localtime(&t3);
        printf("Car %d crossed the intersection at: %d: %d: %d\n", myRank, 
        timeinfo3.tm_hour, timeinfo3.tm_min, timeinfo3.tm_sec);

        sleep(3); // simulates the time to pass the intersection
    }

    // Has finished passing, is now releasing queue
    MPI_Ssend(replyQueue, NUMBER_OF_CARS, MPI_INT, WNRank, 2, MPI_COMM_WORLD);
    time_t t2 = time(NULL);
    struct tm timeinfo2 = *localtime(&t2);
    printf("Car %d exited the intersection at: %d: %d: %d\n", myRank, 
    timeinfo2.tm_hour, timeinfo2.tm_min, timeinfo2.tm_sec);

    status = EXITING;
}



int carSim(int myRank, int networkRank){
    int numberOfVehicles;
    int* replyVehicles = (int*)malloc(sizeof(int)*NUMBER_OF_CARS);
    int* replyQueue = (int*)malloc(sizeof(int)*NUMBER_OF_CARS);
    for(int i = 0; i < NUMBER_OF_CARS; i++){
        replyQueue[i] = -1;
    }
    int replyQueueIndex = 0;
    
    // randomly assign a lane for the car to be in 
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    srand(spec.tv_nsec);
    int arrLane = rand() % 7;

    // randomly assign a time for the car to enter the intersection
    int sensorTime = rand() % SIMULATION_TIME; 
    sleep(sensorTime); 

    time_t t = time(NULL);
    struct tm timeinfo = *localtime(&t);
    printf("Car %d in lane %d entered the intersection at: %d: %d: %d\n", myRank, arrLane, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    // vehicles arrives at the intersection
    requestMsg(myRank, arrLane, replyVehicles, networkRank);

    // prepare thread data 
	pthread_t tid[2];
    struct threadData thread;
    thread.myRank = myRank;
    thread.WNRank = networkRank;
    thread.arrLane = arrLane;
    thread.replyQueue = replyQueue;
    thread.replyVehicles = replyVehicles;
    thread.replyQueue = replyQueue;

    // Fork		
    pthread_create(&tid[0], 0, replyMsg, &thread);
    pthread_create(&tid[1], 0, releaseMsg, &thread);

	// Join
	for(int i = 0; i < 2; i++)
	{
	    pthread_join(tid[i], NULL);
	}

    return 0;

}
