#include "def.h"
#include "data.h"
#include <unistd.h>

int main()
{
	//SYNC WITH MASTER
	int myId,masterId,numberOfResources,numberOfProcesses,i;
	int *sizes,*ids;
	Resource **resources;
	Process **processes;
	
	myId=pvm_mytid();
	pvm_recv( -1, MSG_SYNC);
	pvm_upkint(&masterId, 1, 1);
	pvm_upkint(&numberOfProcesses,1,1);
	
	sizes = (int*)malloc(numberOfProcesses * sizeof(int));
	pvm_upkint(sizes,numberOfProcesses,1);
	ids = (int*)malloc(numberOfProcesses * sizeof(int));
	pvm_upkint(ids,numberOfProcesses,1);
	
	processes = (Process**)malloc(numberOfProcesses * sizeof(Process*));
	for(i=0;i<numberOfProcesses;i++)
	{
		processes[i] = (Process*)malloc(sizeof(Process));
		processes[i]->id = ids[i];
		processes[i]->size = sizes[i];
	}
	free(ids);
	free(sizes);
	
	pvm_upkint(&numberOfResources,1,1);
	sizes = (int*)malloc(numberOfResources * sizeof(int));
	resources = (Resource**)malloc(numberOfResources * sizeof(Resource*));
	for(i=0;i<numberOfResources;i++)
	{
		resources[i] = (Resource*)malloc(sizeof(Resource));
		resources[i]->id = i;
		resources[i]->size = sizes[i];
	}
	
	char buffer[255];
	sprintf(buffer,"%d has received %d legions and %d resources",myId,numberOfProcesses,numberOfResources);
	pvm_initsend(PvmDataDefault);
	pvm_pkstr(buffer);
	pvm_send(masterId,MSG_SINGLE_STRING);

	pvm_exit();
	
	
}


