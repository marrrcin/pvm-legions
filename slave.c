#include "def.h"
#include "data.h"
#include "utils.h"
#include "logger.h"
#include <unistd.h>
#include <glib.h>
#include <time.h>
#include <string.h>

#define MILLIS_IN_CRITICALSECTION 1000
#define MILLIS_IN_LOCALSECTION 1000

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
	}
	free(ids);
	free(sizes);

	pvm_upkint(numberOfResources,1,1);
	sizes = (int*)malloc(*numberOfResources * sizeof(int));
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


void handleAllowMessage(int *acceptedResponses,Resource *resource)
{
    int state;
    pvm_upkint(&state,1,1);
    resource->state = state;
    ++(*acceptedResponses);
}

void sendAllowMessage(int id,int currentState)
{
    pvm_initsend(PvmDataDefault);
    pvm_pkint(&currentState,1,1);
    pvm_send(id,MSG_ALLOW);
}

void handleRequestMessage(long ticket, long *maxTicket, int myId, Resource *currentResource, bool wantsToEnter, GList **blockedProcesses,Process **processes,int numOfProc)
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

    //MOST IMPORTANT THING IN THIS CODE
    if(wantsToEnter == false || resource != currentResource->id || (requestTicket<ticket) || (requestTicket == ticket && id < myId))
    {
        sendAllowMessage(id,currentResource->state);
        logEvent("Allowed",myId);
    }
    else
    {
        int idx = getProcessIdx(id, processes, numOfProc);
        processes[idx]->ticketNumber = requestTicket;
        *blockedProcesses = g_list_append(*blockedProcesses,processes[idx]);
        logEvent("Blocked",myId);
    }
}

void unblockOneWaitingProcess(Resource *currentResource,GList **blockedProcesses)
{
    GList *min;
    Process *blocked;
    min = findProcessWithMinTicket(*blockedProcesses);
    blocked = (Process*)min->data;
    if(blocked->size + currentResource->state <= currentResource->size)
    {
        //currentResource->state += blocked->size;
        sendAllowMessage(blocked->id,currentResource->state);
        removeProcess(blocked->id,blockedProcesses);
        currentResource->state += blocked->size;
    }
}

void unblockAllWaitingProcesses(GList **blockedProcesses)
{
    GList *l;
    Process *current;
    for(l=*blockedProcesses;l != NULL; l = l->next)
    {
        current = (Process*)l->data;
        sendAllowMessage(current->id,-1);
    }
    g_list_free(*blockedProcesses);
    *blockedProcesses = NULL;
}

void blockedToString(GList *blockedProcesses,char *buff)
{
    GList *l;
    Process *p;
    for(l=blockedProcesses;l != NULL;l=l->next)
    {
        p = (Process*)l->data;
        char tmp[64];
        sprintf(tmp,"%d (ticket %ld),",p->id,p->ticketNumber);
        strcat(buff,tmp);
    }
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
    GList *blockedProcesses;
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

        currentResource = resources[getResourceIdx(resourceRequested,resources,numberOfResources)];
        while(acceptedResponses != numberOfProcesses - 1)
        {
            logEvent("Waiting for critical section",myId);
            status = pvm_nrecv(ANY,MSG_ALLOW);
            if(status != 0)
            {
                handleAllowMessage(&acceptedResponses,currentResource);
                char buff[255];
                sprintf(buff,"Handled allow message, accepted == %d",acceptedResponses);
                logEvent(buff,myId);
            }

            status = pvm_nrecv(ANY,MSG_REQUEST);
            if(status != 0)
            {
                handleRequestMessage(ticket,&maxTicket,myId, currentResource,wantsToEnter,&blockedProcesses,processes,numberOfProcesses);
                char buff[255];
                sprintf(buff,"Handled request message, blocked count == %d",(int)g_list_length(blockedProcesses));
                logEvent(buff,myId);
            }

        }

        logEvent("Accepted by every process!",myId);


        //CRITICAL SECTION
        endTime = (clock_t) (clock() + CLOCKS_PER_SEC/1000.0 * MILLIS_IN_CRITICALSECTION);
        if(currentResource->state == -1)
        {
            currentResource->state = 0; //if state was -1 that means this is the first process that enters Critical Section
        }
        char tmp1[255];
        sprintf(tmp1,"Entered critical section!");
        logEvent(tmp1,myId);
        while(clock() < endTime)
        {
            //TODO
            //unblockOneWaitingProcess(currentResource,&blockedProcesses);

            status = pvm_nrecv(ANY,MSG_REQUEST);
            if(status != 0)
            {
                handleRequestMessage(ticket,&maxTicket,myId, currentResource,wantsToEnter,&blockedProcesses,processes,numberOfProcesses);
                char buff[255];
                sprintf(buff,"Handled request message, blocked count == %d",(int)g_list_length(blockedProcesses));
                logEvent(buff,myId);
            }
        }

        //EXIT CRITICAL SECTION
        wantsToEnter = false;
        unblockAllWaitingProcesses(&blockedProcesses);
        char tmp2[255];
        sprintf(tmp2,"Exited critical section && unblocked waiting processes!");
        logEvent(tmp2,myId);


        //LOCAL SECTION
        endTime = (clock_t) (clock() + CLOCKS_PER_SEC/1000.0 * MILLIS_IN_LOCALSECTION);
        while(clock() < endTime)
        {
            status = pvm_nrecv(ANY,MSG_REQUEST);
            if(status != 0)
            {
                handleRequestMessage(ticket,&maxTicket,myId, currentResource,wantsToEnter,&blockedProcesses,processes,numberOfProcesses);
                char buff[255];
                sprintf(buff,"Handled request message, blocked count == %d",(int)g_list_length(blockedProcesses));
                logEvent(buff,myId);
            }
        }
    }

    freeMemory(resources,processes,numberOfProcesses,numberOfResources,NULL);
    free(processIds);
	pvm_exit();
	
	
}
#pragma clang diagnostic pop


