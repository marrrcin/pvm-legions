#include "def.h"
#include "data.h"
#include "slaveName.h"
#include <string.h>

void loadDataFromFile(char *fileName, int *numberOfResources, Resource ***r, int *numberOfProcesses, Process ***p)
{
	FILE *file;
	int procSize,resSize,i;
	Process **processes;
	Resource **resources;
	
	printf("Loading data from %s\n",fileName);
	file = fopen(fileName,"r");
	fscanf(file,"%d\n",numberOfProcesses);
	processes = (Process**)malloc(*numberOfProcesses * sizeof(Process*));
	for(i=0;i<*numberOfProcesses;i++)
	{
		fscanf(file,"%d\n",&procSize);
		processes[i] = (Process*)malloc(sizeof(Process));
		processes[i]->id = i;
		processes[i]->size = procSize;
	}
	
	fscanf(file,"%d\n",numberOfResources);
	resources = (Resource**)malloc(*numberOfResources * sizeof(Resource*));
	for(i=0;i<*numberOfResources;i++)
	{
		fscanf(file,"%d\n",&resSize);
		resources[i] = (Resource*)malloc(sizeof(Resource));
		resources[i]->id = i;
		resources[i]->size = resSize;
	}
	
	fclose(file);
	*r = resources;
	*p = processes;
}

int syncData(Resource **resources,Process **processes, int *slaveIds,int slavesCount, int resCount,int masterId)
{
	int i,*resourceSizes,*processSizes;
	resourceSizes = (int*)malloc(resCount * sizeof(int));
	processSizes = (int*)malloc(slavesCount * sizeof(int));
	for(i=0;i<resCount;i++)
	{
		resourceSizes[i] = resources[i]->size;
	}
	
	for(i=0;i<slavesCount;i++)
	{
		processSizes[i] = processes[i]->size;
	}
	
	for(i=0;i<slavesCount;i++)
	{
		//SEE MSG_SYNC in data.h
		pvm_initsend(PvmDataDefault);
		pvm_pkint(&masterId,1,1);
		pvm_pkint(&slavesCount,1,1);
		pvm_pkint(processSizes,slavesCount,1);
		pvm_pkint(slaveIds,slavesCount,1);
		pvm_pkint(&resCount,1,1);
		pvm_pkint(resourceSizes,resCount,1);
		pvm_send(slaveIds[i],MSG_SYNC);
	}
}

int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		printf("No input file!\n");
		return -1;
	}
	
	int i;
	
	//LOAD DATA
	Resource **resources;
	Process **processes;
	int numberOfProcesses,numberOfResources;
	loadDataFromFile(argv[1],&numberOfResources,&resources,&numberOfProcesses,&processes);
	printf("Loaded %d legions and %d resoures...\n",numberOfProcesses,numberOfResources);

	//SPAWN SLAVES AND SYNC DATA
	int masterId,spawnedCount;
	int *slaveIds;
	slaveIds = (int*)malloc(numberOfProcesses * sizeof(int));
	
	masterId = pvm_mytid();
	spawnedCount = pvm_spawn(SLAVENAME,NULL,PvmTaskDefault,"",numberOfProcesses,slaveIds);
	
	if(numberOfProcesses != spawnedCount)
	{
		printf("[ERROR] Could not spawn enough processes");
	}
	else
	{
		//SYNC
		printf("Synchronizing data with slave processes...\n");
		syncData(resources,processes,slaveIds,spawnedCount,numberOfResources,masterId);
		for(i=0;i<spawnedCount;i++)
		{
			pvm_recv(-1,MSG_SINGLE_STRING);
			char str[255];
			pvm_upkstr(str);
			printf("%s\n",str);
		}
	}
	
	
	
	printf("Deallocating memory...\n");
	for(i=0;i<numberOfResources;i++)
	{
		free(resources[i]);
	}
	free(resources);
	for(i=0;i<numberOfProcesses;i++)
	{
		free(processes[i]);
	}
	free(processes);
	free(slaveIds);
	printf("Deallocated\n");


	printf("Invoking pvm_exit...\n");
	pvm_exit();
	printf("Finished\n");
}

