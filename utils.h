//
// Created by marcin on 20.05.15.
//

#ifndef SRC_UTILS_H
#define SRC_UTILS_H

#include <stdlib.h>

void freeMemory(int i, Resource **resources, Process **processes, int numberOfProcesses,
                int numberOfResources, int *slaveIds) {
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
}


#endif //SRC_UTILS_H
