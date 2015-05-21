//
// Created by marcin on 21.05.15.
//

#ifndef SRC_LOGGER_H
#define SRC_LOGGER_H
#include <time.h>
#include <unistd.h>
#include <stdio.h>

void logEvent(char *message,int id)
{
    char fileName[255];
    FILE *file;
    sprintf(fileName,"/home/marcin/programowanie-rozproszone/logs/%d.log",id);

    if(access(fileName,F_OK) != -1) //file.exists() == true
    {
        file = fopen(fileName,"a");
    }
    else
    {
        file = fopen(fileName,"w+");
    }

    time_t logTime = time(NULL);
    fprintf(file,"%s : %s\n",asctime(localtime(&logTime)),message);
    fclose(file);
}


#endif //SRC_LOGGER_H
