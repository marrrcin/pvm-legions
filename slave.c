#include "def.h"
#include "data.h"
#include "utils.h"
#include "logger.h"

//MAX MILLIS IN SECTIONS
#define MILLIS_IN_CRITICALSECTION 13000
#define MILLIS_IN_LOCALSECTION 3000

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
    sprintf(buffer,"----- %d has received %d legions and %d resources",myId,numberOfProcesses,numberOfResources);
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

void handleAllowMessage(int *acceptedResponses,Resource *resource,int myId,int *emptyStatesReceived)
{
    int state,sender;
    pvm_upkint(&state,1,1);
    pvm_upkint(&sender,1,1);
    resource->state = state;
    if(state == -1)
    {
        ++(*emptyStatesReceived);
    }

    ++(*acceptedResponses);

    char log[255];
    sprintf(log,"----- Received allow message from #%d",sender);
    logEvent(log,myId);
}

void sendAllowMessage(int id,int currentState,int myId)
{
    char log[255];
    sprintf(log,"----- Sending allow message to #%d",id);
    logEvent(log,myId);
    pvm_initsend(PvmDataDefault);
    pvm_pkint(&currentState,1,1);
    pvm_pkint(&myId,1,1);
    pvm_send(id,MSG_ALLOW);
}

void handleRequestMessage(long ticket, long *maxTicket, int myId, Resource *currentResource, bool wantsToEnter, GList **blockedProcesses,Process **processes,int numOfProc, bool inCriticalSection)
{
    int id,resource;
    long requestTicket;
    pvm_upkint(&id,1,1);
    pvm_upklong(&requestTicket,1,1);
    pvm_upkint(&resource,1,1);

    char where[9];
    if (inCriticalSection == true)
        sprintf(where, "CRITICAL");
    else if (wantsToEnter == true)
        sprintf(where, "WAIT");
    else
        sprintf(where, "LOCAL");

    char buff[255];
    sprintf(buff,"[%s] Request message: ticket %ld for resource %d from #%d", where, requestTicket, resource, id);
    logEvent(buff,myId);

    *maxTicket = requestTicket > *maxTicket ? requestTicket : *maxTicket;
    char buff2[64];
    sprintf(buff2,"[%s] Max ticket is now %ld", where, *maxTicket);
    logEvent(buff2,myId);

    char buff3[64];
    //MOST IMPORTANT THING IN THIS CODE
    //get id of process that is requesting critical section
    int idx = getProcessIdx(id, processes, numOfProc);

    //send ALLOW msg if at least one of the following is true:
    //* current process is in local section
    //* other process is requesting for other resource than current one
    //* other process's ticket has higher priority than current one's
    //* tickets are equal and other process has smaller id
    if(wantsToEnter == false || resource != currentResource->id || (requestTicket<ticket) || (requestTicket == ticket && id < myId))
    {
        sendAllowMessage(id,-1,myId);
        sprintf(buff3, "[%s] Allowed", where);
        logEvent(buff3,myId);
    }
    // send ALLOW msg if all below are true:
    // * current process is in critical section
    // * other process requests for the same resource
    // * there's still some room left for this process
    else if (inCriticalSection == true && resource == currentResource->id && currentResource->size >= currentResource->state + processes[idx]->size)
    {
        sendAllowMessage(id,-1,myId);
        sprintf(buff3, "[%s] WE'LL FIT TOGETHER! Allowed", where);
        logEvent(buff3,myId);
    }
    // block requesting process
    else
    {
        processes[idx]->ticketNumber = requestTicket;
        GList *l = g_list_append(*blockedProcesses,processes[idx]);
        *blockedProcesses = l;
        sprintf(buff3, "[%s] Blocked", where);
        logEvent(buff3,myId);
    }
}

void unblockAllWaitingProcesses(GList **blockedProcesses,int myId)
{
    GList *l;
    Process *current;
    for(l=*blockedProcesses;l != NULL; l = l->next)
    {
        current = (Process*)l->data;
        sendAllowMessage(current->id,-1,myId);
    }
    g_list_free(*blockedProcesses);
    *blockedProcesses = NULL;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#pragma ide diagnostic ignored "OCDFAInspection"
int main()
{
	//SYNC WITH MASTER
	int myId, masterId, numberOfResources, numberOfProcesses, mySize;

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
    bool inCriticalSection = false;
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
        sprintf(tmp0,"[WAIT START] Sending request for resource %d, my size %d, ticket %ld",resourceRequested,mySize,ticket);
        logEvent(tmp0,myId);
        broadcastEntryRequest(processIds, numberOfProcesses, myId, ticket, resourceRequested);
        logEvent("[WAIT] Broadcasted",myId);

        currentResource = resources[resourceRequested];
        int emptyStatesReceived = 0;
        logEvent("[WAIT] ...",myId);
        while(acceptedResponses != numberOfProcesses - 1)
        {
            status = pvm_nrecv(ANY,MSG_ALLOW);
            if(status != 0)
            {
                handleAllowMessage(&acceptedResponses,currentResource,myId,&emptyStatesReceived);
                char buff[255];
                sprintf(buff,"[WAIT] Handled allow message, accepted == %d, emptyStates counter == %d",acceptedResponses,emptyStatesReceived);
                logEvent(buff,myId);
            }

            status = pvm_nrecv(ANY,MSG_REQUEST);
            if(status != 0)
            {
                handleRequestMessage(ticket,&maxTicket,myId, currentResource,wantsToEnter,&blockedProcesses,processes,numberOfProcesses,inCriticalSection);
                char buff[255];
                sprintf(buff,"[WAIT] Handled request message, blocked count == %d",(int)g_list_length(blockedProcesses));
                logEvent(buff,myId);
            }

        }

        logEvent("[WAIT STOP] Accepted by every process!",myId);

        //CRITICAL SECTION
        endTime = (clock_t) (clock() + CLOCKS_PER_SEC/1000.0 * (1000.0 + rand()%MILLIS_IN_CRITICALSECTION));
        if(emptyStatesReceived == acceptedResponses)
        {
            currentResource->state = mySize; //if state was -1 that means this is the first process that enters Critical Section
            char buff[255];
            sprintf(buff,"[CRITICAL START] Entering as a first process in %d resource",resourceRequested);
            logEvent(buff,myId);
        }
        inCriticalSection = true;
        char tmp1[255];
        sprintf(tmp1,"[CRITICAL START] Entered critical section no. %d! Resource state == %d",resourceRequested,currentResource->state);
        logEvent(tmp1,myId);

        // spend some time in critical section, sending ALLOW messages to all processes
        while(clock() < endTime)
        {
            status = pvm_nrecv(ANY,MSG_REQUEST);
            if(status != 0)
            {
                handleRequestMessage(ticket,&maxTicket,myId, currentResource,wantsToEnter,&blockedProcesses,processes,numberOfProcesses,inCriticalSection);
                char buff[255];
                sprintf(buff,"[CRITICAL] Handled request message, blocked count == %d",(int)g_list_length(blockedProcesses));
                logEvent(buff,myId);
            }
        }

        //EXIT CRITICAL SECTION
        logEvent("[CRITICAL] Exiting critical section",myId);
        inCriticalSection = false;
        wantsToEnter = false;
        unblockAllWaitingProcesses(&blockedProcesses,myId);
        char tmp2[255];
        sprintf(tmp2,"[CRTICAL STOP] Exited critical section && unblocked waiting processes!");
        logEvent(tmp2,myId);

        //LOCAL SECTION
        logEvent("[LOCAL START] Entered local section",myId);
        endTime = (clock_t) (clock() + CLOCKS_PER_SEC/1000.0 * (1000.0 + rand()%MILLIS_IN_LOCALSECTION));
        while(clock() < endTime)
        {
            status = pvm_nrecv(ANY,MSG_REQUEST);
            if(status != 0)
            {
                handleRequestMessage(ticket,&maxTicket,myId, currentResource,wantsToEnter,&blockedProcesses,processes,numberOfProcesses,inCriticalSection);
                char buff[255];
                sprintf(buff,"[LOCAL] Handled request message, blocked count == %d",(int)g_list_length(blockedProcesses));
                logEvent(buff,myId);
            }
        }
        logEvent("[LOCAL STOP] Exited local section",myId);
    }

    freeMemory(resources,processes,numberOfProcesses,numberOfResources,NULL);
    free(processIds);
	pvm_exit();

}
#pragma clang diagnostic pop
