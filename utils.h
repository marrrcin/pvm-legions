//
// Created by marcin on 20.05.15.
//

#ifndef SRC_UTILS_H
#define SRC_UTILS_H

#include <stdlib.h>
#include <glib.h>
void freeMemory(Resource **resources, Process **processes, int numberOfProcesses,
                int numberOfResources, int *slaveIds) {
    int i;
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
    if(slaveIds != NULL)
    {
        free(slaveIds);
    }

}

void extractProcessIds(Process **processes, int numberOfProc, int **ids)
{
    int *i,c;
    i = (int*)malloc(numberOfProc * sizeof(int));
    for(c=0;c<numberOfProc;c++)
    {
        i[c] = processes[c]->id;
    }
    *ids = i;
}

gint processByIdComparer(gconstpointer a, gconstpointer b)
{
    Process *p1,*p2;
    p1 = (Process*)a;
    p2 = (Process*)b;
    if(p1->id == p2->id)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

GList* findProcessById(int id,GList *list)
{
    Process toFind;
    toFind.id = id;
    return g_list_find_custom(list,&toFind,processByIdComparer);
}

void removeProcess(int id,GList *list)
{
    GList *toRemove = findProcessById(id,list);
    g_list_remove(list,toRemove->data);
}

int getPorocessIdx(int id,Process **processes,int count)
{
    int i=0;
    for(i=0;i<count;i++)
    {
        if(processes[i]->id == id)
        {
            return i;
        }
    }
    return -1;
}

#endif //SRC_UTILS_H

/*
 * GLIST SNIPPET
 *  GList *resources = NULL;
    Process p1,p2,p3;
    p1.id = 1;
    p2.id = 666;
    p2.size = 69;
    p3.id = 3;
    resources = g_list_append(resources,&p1);
    resources = g_list_append(resources,&p2);
    resources = g_list_append(resources,&p3);

    GList *found;
    Process f;
    f.id = 666;
    found = g_list_find_custom(resources,&f,findProcessById);
    Process *x = found->data;
    printf("%d \n",x->size);


 *
 *
 */