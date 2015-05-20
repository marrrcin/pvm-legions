#include "def.h"
#include "data.h"
#include "utils.h"
#include <unistd.h>
#include <glib.h>

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


void handleAllowMessage(int *acceptedResponses)
{
    int accept;
    pvm_upkint(&accept,1,1);
    ++(*acceptedResponses);
}

void sendAllowMessage(int id)
{
    int msg = ACCEPT;
    pvm_initsend(PvmDataDefault);
    pvm_pkint(&msg,1,1,);
    pvm_send(id,MSG_ALLOW);
}

void handleRequestMessage(long ticket, long *maxTicket, int myId, int myResource, bool wantsToEnter, GList **blockedProcesses,Process **processes,int numOfProc)
{
    int id,resource;
    long requestTicket;
    pvm_upkint(&id,1,1);
    pvm_upklong(&requestTicket,1,1);
    pvm_upkint(&resource,1,1);

    *maxTicket = requestTicket > *maxTicket ? requestTicket : *maxTicket;

    //MOST IMPORTANT THING IN THIS CODE
    if(wantsToEnter == false || resource != myResource || (requestTicket<ticket) || (requestTicket == ticket && id < myId))
    {
        sendAllowMessage(id);
    }
    else
    {
        int idx = getPorocessIdx(id,processes,numOfProc);
        processes[idx]->ticketNumber = requestTicket;
        *blockedProcesses = g_list_append(*blockedProcesses,processes[idx]);
    }
}


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#pragma ide diagnostic ignored "OCDFAInspection"
int main()
{
	//SYNC WITH MASTER
	int myId,masterId,numberOfResources,numberOfProcesses,i;

	Resource **resources;
	Process **processes;
    GList *blockedProcesses;
    int *processIds;
	myId=pvm_mytid();
    srand((unsigned int) myId);
	syncWithMaster(&numberOfResources,&resources,&numberOfProcesses,&processes,&masterId);
    syncConfirm(myId, masterId, numberOfResources, numberOfProcesses);
    extractProcessIds(processes,numberOfProcesses,&processIds);
    bool wantsToEnter = false;
    long ticket = 0,maxTicket = 0;
    int resourceRequested = 0, acceptedResponses = 0,status;
    while(1==1)
    {
        //PREPARE TO ENTRANCE
        wantsToEnter = true;
        ticket = maxTicket + 1;
        resourceRequested = rand() % numberOfResources;
        acceptedResponses = 0;
        broadcastEntryRequest(processIds, numberOfProcesses, myId, ticket, resourceRequested);
        while(acceptedResponses != numberOfProcesses - 1)
        {
            status = pvm_nrecv(ANY,MSG_ALLOW);
            if(status != 0)
            {
                handleAllowMessage(&acceptedResponses);
            }

            status = pvm_nrecv(ANY,MSG_REQUEST);
            if(status != 0)
            {
                handleRequestMessage(ticket,&maxTicket,myId, resourceRequested,wantsToEnter,&blockedProcesses,processes,numberOfProcesses);
            }

        }

        //CRITICAL SECTION
        ;

        //EXIT CRITICAL SECTION
        wantsToEnter = false;

        //LOCAL SECTION

    }

    freeMemory(resources,processes,numberOfProcesses,numberOfResources,NULL);
	pvm_exit();
	
	
}
#pragma clang diagnostic pop


