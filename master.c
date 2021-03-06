#include "def.h"
#include "data.h"
#include "slaveName.h"
#include <string.h>
#include "utils.h"
#include <glib.h>
int loadDataFromFile(char *fileName, int *numberOfResources, Resource ***r, int *numberOfProcesses, Process ***p)
{
	FILE *file;
	int procSize,resSize,i;
	Process **processes;
	Resource **resources;
	
	printf("Loading data from %s\n",fileName);
	file = fopen(fileName,"r");
	fscanf(file,"%d\n",numberOfProcesses);
	processes = (Process**)malloc(*numberOfProcesses * sizeof(Process*));

	int sumProc = 0;
	int maxProc = 0;
	for(i=0;i<*numberOfProcesses;i++)
	{
		fscanf(file,"%d\n",&procSize);
		processes[i] = (Process*)malloc(sizeof(Process));
		processes[i]->id = i;
		processes[i]->size = procSize;
		sumProc += procSize;
		maxProc = procSize > maxProc ? procSize : maxProc;
	}
	
	fscanf(file,"%d\n",numberOfResources);
	resources = (Resource**)malloc(*numberOfResources * sizeof(Resource*));
	printf("Processes : %d | Resources : %d\n",*numberOfProcesses,*numberOfResources);
	int sumRes = 0;
	int minRes = INFINITY;
	for(i=0;i<*numberOfResources;i++)
	{
		fscanf(file,"%d\n",&resSize);
		resources[i] = (Resource*)malloc(sizeof(Resource));
		resources[i]->id = i;
		resources[i]->size = resSize;
		sumRes += resSize;
		minRes = resSize < minRes ? resSize : minRes;
	}
	
	fclose(file);
	*r = resources;
	*p = processes;

	printf("SumProc %d , SumRes %d, MinRes %d, MaxProc %d\n",sumProc,sumRes,minRes,maxProc);
	if ((*numberOfProcesses <= *numberOfResources) || (sumProc <= sumRes) || (minRes <= maxProc))
		return 1;
	else
		return 0;
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
	int numberOfProcesses, numberOfResources, err;
	err = loadDataFromFile(argv[1],&numberOfResources,&resources,&numberOfProcesses,&processes);

	//SPAWN SLAVES AND SYNC DATA
	int masterId,spawnedCount;
	int *slaveIds;
	slaveIds = (int*)malloc(numberOfProcesses * sizeof(int));

	if (err == 1) {
		printf("Invalid input data. Please try again.\n");
		freeMemory(resources, processes, numberOfProcesses, numberOfResources, slaveIds);
		return -1;
	}

	printf("Loaded %d legions and %d resoures...\n",numberOfProcesses,numberOfResources);
	masterId = pvm_mytid();
	spawnedCount = pvm_spawn(SLAVENAME,NULL,PvmTaskDefault,"",numberOfProcesses,slaveIds);
	printf("spawnedCount: %d", spawnedCount);
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
        printf("Synced\n");
	}



	printf("Deallocating memory...\n");
    freeMemory(resources, processes, numberOfProcesses, numberOfResources, slaveIds);
    printf("Deallocated\n");


	printf("Invoking pvm_exit...\n");
    for(i=0;i<spawnedCount;i++)
    {
        pvm_recv(-1,MSG_SINGLE_STRING);
        char str[255];
        pvm_upkstr(str);
        printf("%s\n",str);
    }
	pvm_exit();
	printf("Finished\n");
}

