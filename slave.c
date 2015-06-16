#include "def.h"
#include "data.h"
#include "utils.h"
#include "logger.h"
#include <unistd.h>
#include <glib.h>
#include <time.h>
#include <string.h>

//MAX MILLIS IN SECTIONS
#define MILLIS_IN_CRITICALSECTION 6333
#define MILLIS_IN_LOCALSECTION 6777

void syncWithMaster(int *numberOfResources,Resource ***r, int *numberOfProcesses,Process ***p,int *masterId)
{
	Process **processes;
	Resource **resources;
	int *sizes,*ids;
	int i;

	pvm_recv( -1, MSG_SYNC);
	pvm_upkint(masterId, 1, 1);

	pvm_upkint(numberOfProcesses,1,1);
	sizes = (int*)malloc(*numberOfProcesses * sizeof(int));
	pvm_upkint(sizes,*numberOfProcesses,1);
	ids = (int*)malloc(*numberOfProcesses * sizeof(int));
	pvm_upkint(ids,*numberOfProcesses,1);
	processes = (Process**)malloc(*numberOfProcesses * sizeof(Process*));
	for(i=0;i<*numberOfProcesses;i++)
	{
		processes[i] = (Process*)malloc(sizeof(Process));
		processes[i]->id = ids[i];
		processes[i]->size = sizes[i];
        processes[i]->occupies = 0;
        processes[i]->resource = -1;
	}
	free(ids);
	free(sizes);

	pvm_upkint(numberOfResources,1,1);
	sizes = (int*)malloc(*numberOfResources * sizeof(int));
    pvm_upkint(sizes,*numberOfResources,1);
	resources = (Resource**)malloc(*numberOfResources * sizeof(Resource*));
	for(i=0;i<*numberOfResources;i++)
	{
		resources[i] = (Resource*)malloc(sizeof(Resource));
		resources[i]->id = i;
		resources[i]->size = sizes[i];
	}
	free(sizes);

	*r = resources;
	*p = processes;
}

void syncConfirm(int myId, int masterId, int numberOfResources, int numberOfProcesses) {
    char buffer[255];
    sprintf(buffer,"%d has received %d legions and %d resources",myId,numberOfProcesses,numberOfResources);
    pvm_initsend(PvmDataDefault);
    pvm_pkstr(buffer);
    pvm_send(masterId,MSG_SINGLE_STRING);
}

void broadcastEntryRequest(int *processIds, int numberOfProc, int myId, long ticket, int tract)
{
    //SEE MSG_REQUEST in data.h
    pvm_initsend(PvmDataDefault);
    pvm_pkint(&myId,1,1);
    pvm_pklong(&ticket,1,1);
    pvm_pkint(&tract,1,1);

    pvm_mcast(processIds,numberOfProc,MSG_REQUEST);
}

void broadcastRelease(int *processIds,int numberOfProc,int myId)
{
    int occupies = 0;
    pvm_initsend(PvmDataDefault);
    pvm_pkint(&myId,1,1);
    pvm_pkint(&occupies,1,1);
    pvm_mcast(processIds,numberOfProc,MSG_RELEASE);
}


void handleResponseMessage(int *acceptedResponses, int myId, Process **processes, int pCount)
{
    int state,sender;
    long senderTicket;
    pvm_upkint(&state,1,1);
    pvm_upkint(&sender,1,1);
    pvm_upklong(&senderTicket,1,1);

    int senderIdx = getProcessIdx(sender,processes,pCount);
    processes[senderIdx]->occupies = state;
    processes[senderIdx]->ticketNumber = senderTicket;

    ++(*acceptedResponses);

    char log[255];
    sprintf(log,"Received response message from #%d occupies = %d",sender,state);
    logEvent(log,myId);
}

void sendResponseMessage(int id, int occupies, int myId, long myTicket)
{
    char log[255];
    sprintf(log,"Sending response message to #%d occupies %d",id,occupies);
    logEvent(log,myId);
    pvm_initsend(PvmDataDefault);
    pvm_pkint(&occupies,1,1);
    pvm_pkint(&myId,1,1);
    pvm_pklong(&myTicket,1,1);
    pvm_send(id,MSG_RESPONSE);
}

void handleRequestMessage(long ticket, long *maxTicket, int myId, int mySize,Resource *currentResource, bool wantsToEnter)
{
    int id,resource;
    long requestTicket;
    pvm_upkint(&id,1,1);
    pvm_upklong(&requestTicket,1,1);
    pvm_upkint(&resource,1,1);

    char buff[255];
    sprintf(buff,"Request message: ticket %ld for resource %d from #%d",requestTicket,resource,id);
    logEvent(buff,myId);

    *maxTicket = requestTicket > *maxTicket ? requestTicket : *maxTicket;
    char buff2[64];
    sprintf(buff2,"Max ticket is now %ld",*maxTicket);
    logEvent(buff2,myId);

    int occupies = mySize;
    if(wantsToEnter == false || resource != currentResource->id)
    {
        occupies = 0;
    }

    sendResponseMessage(id, occupies, myId,ticket);
    logEvent("Sent response message",myId);
}

void handleReleaseMessage(Process **processes,int numOfProc)
{
    int state,sender;
    pvm_upkint(&sender,1,1);
    pvm_upkint(&state,1,1);
    int senderIdx = getProcessIdx(sender,processes,numOfProc);
    processes[senderIdx]->occupies = 0;
}

int sumSizeOfAllBeforeMe(Process **processes,int numOfProc,long myTicket,int myId)
{
    int sum = 0,i;
    for(i=0;i<numOfProc;i++)
    {
        if(processes[i]->ticketNumber < myTicket || (processes[i]->ticketNumber == myTicket && processes[i]->id < myId))
        {
            sum += processes[i]->occupies;
        }
    }
    return sum;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#pragma ide diagnostic ignored "OCDFAInspection"
int main()
{
	//SYNC WITH MASTER
	int myId,masterId,numberOfResources,numberOfProcesses,i,mySize;

	Resource **resources;
	Process **processes;
    Resource *currentResource;
    GList *blockedProcesses = NULL;
    int *processIds;
	myId=pvm_mytid();
    srand((unsigned int) myId);
	syncWithMaster(&numberOfResources,&resources,&numberOfProcesses,&processes,&masterId);
    syncConfirm(myId, masterId, numberOfResources, numberOfProcesses);
    extractProcessIds(processes,numberOfProcesses,&processIds);
    mySize = processes[getProcessIdx(myId,processes,numberOfProcesses)]->size;

    bool wantsToEnter = false;
    long ticket = 0,maxTicket = 0;
    int resourceRequested = 0, acceptedResponses = 0,status;
    clock_t endTime;
    while(1==1)
    {
        //PREPARE TO ENTRANCE
        wantsToEnter = true;
        ticket = maxTicket + 1;
        resourceRequested = rand() % numberOfResources;
        acceptedResponses = 0;
        char tmp0[255];
        sprintf(tmp0,"Sending request for resource %d, my size %d, ticket %ld",resourceRequested,mySize,ticket);
        logEvent(tmp0,myId);
        broadcastEntryRequest(processIds, numberOfProcesses, myId, ticket, resourceRequested);
        logEvent("Broadcasted",myId);

        currentResource = resources[resourceRequested];
        int emptyStatesReceived = 0;
        logEvent("Waiting for critical section",myId);
        while(acceptedResponses != numberOfProcesses - 1)
        {
            status = pvm_nrecv(ANY,MSG_RESPONSE);
            if(status != 0)
            {
                handleResponseMessage(&acceptedResponses,myId,processes,numberOfProcesses);
                char buff[255];
                sprintf(buff,"Handled allow message, accepted == %d, emptyStates counter == %d",acceptedResponses,emptyStatesReceived);
                logEvent(buff,myId);
            }

            status = pvm_nrecv(ANY,MSG_REQUEST);
            if(status != 0)
            {//(long ticket, long *maxTicket, int myId, int mySize,Resource *currentResource, bool wantsToEnter)
                handleRequestMessage(ticket,&maxTicket,myId, mySize,currentResource,wantsToEnter);
                char buff[255];
                sprintf(buff,"Handled request message");//, blocked count == %d",(int)g_list_length(blockedProcesses));
                logEvent(buff,myId);
            }

            status = pvm_nrecv(ANY,MSG_RELEASE);
            if(status != 0)
            {
                handleReleaseMessage(processes,numberOfProcesses);
                logEvent("Received release message",myId);
            }
        }
        char tmp4[255];
        sprintf(tmp4,"Collected all responses, sum of all before me = %d",sumSizeOfAllBeforeMe(processes,numberOfProcesses,ticket,myId));
        logEvent(tmp4,myId);

        while(sumSizeOfAllBeforeMe(processes,numberOfProcesses,ticket,myId) + mySize > currentResource->size)
        {
            status = pvm_nrecv(ANY,MSG_RELEASE);
            if(status != 0)
            {
                handleReleaseMessage(processes,numberOfProcesses);
                logEvent("Received release message",myId);
            }

            status = pvm_nrecv(ANY,MSG_REQUEST);
            if(status != 0)
            {
                handleRequestMessage(ticket,&maxTicket,myId,mySize,currentResource,wantsToEnter);
                char buff[255];
                sprintf(buff,"Handled request message");//, blocked count == %d",(int)g_list_length(blockedProcesses));
                logEvent(buff,myId);
            }
        }


        //CRITICAL SECTION
        endTime = (clock_t) (clock() + CLOCKS_PER_SEC/1000.0 * (1000.0 + rand()%MILLIS_IN_CRITICALSECTION));
//        if(emptyStatesReceived == acceptedResponses)
//        {
//            currentResource->state = mySize; //if state was -1 that means this is the first process that enters Critical Section
//            char buff[255];
//            sprintf(buff,"Entering as a first process in %d resource",resourceRequested);
//            logEvent(buff,myId);
//        }
        char tmp1[255];
        sprintf(tmp1,"Entered critical section no. %d!",resourceRequested);
        logEvent(tmp1,myId);
        while(clock() < endTime)
        {
            status = pvm_nrecv(ANY,MSG_RELEASE);
            if(status != 0)
            {
                handleReleaseMessage(processes,numberOfProcesses);
                logEvent("Received release message",myId);
            }

            status = pvm_nrecv(ANY,MSG_REQUEST);
            if(status != 0)
            {
                handleRequestMessage(ticket,&maxTicket,myId,mySize,currentResource,wantsToEnter);
                char buff[255];
                sprintf(buff,"Handled request message");//, blocked count == %d",(int)g_list_length(blockedProcesses));
                logEvent(buff,myId);
            }
        }

        //EXIT CRITICAL SECTION
        logEvent("Exiting critical section",myId);
        wantsToEnter = false;

        char tmp2[255];
        sprintf(tmp2,"Exited critical section");
        logEvent(tmp2,myId);

        broadcastRelease(processIds,numberOfProcesses,myId);
        logEvent("Broadcasted release message",myId);

        //LOCAL SECTION
        logEvent("Entered local section",myId);
        endTime = (clock_t) (clock() + CLOCKS_PER_SEC/1000.0 * (1000.0 + rand()%MILLIS_IN_LOCALSECTION));
        while(clock() < endTime)
        {
            status = pvm_nrecv(ANY,MSG_REQUEST);
            if(status != 0)
            {
                handleRequestMessage(ticket,&maxTicket,myId,mySize,currentResource,wantsToEnter);
                char buff[255];
                sprintf(buff,"Handled request message");//, blocked count == %d",(int)g_list_length(blockedProcesses));
                logEvent(buff,myId);
            }
        }
        logEvent("Exited local section",myId);
    }

    freeMemory(resources,processes,numberOfProcesses,numberOfResources,NULL);
    free(processIds);
	pvm_exit();
	
	
}
#pragma clang diagnostic pop


