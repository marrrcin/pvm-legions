//
// Created by marcin on 21.05.15.
//

#ifndef SRC_LOGGER_H
#define SRC_LOGGER_H
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int first = 1;
void logEvent(char *message,int id)
{
    char fileName[255];
    FILE *file;
    sprintf(fileName,"/home/marcin/programowanie-rozproszone/logs/%d.log",id);
    //sprintf(fileName,"/home/tortila/Documents/uni/pr/logs/%d.log",id);

    if(access(fileName,F_OK) != -1 && first != 1) //file.exists() == true
    {
        file = fopen(fileName,"a");
    }
    else
    {
        file = fopen(fileName,"w+");
        first = 0;
    }

    time_t logTime = time(NULL);
    char *timeStr =  asctime(localtime(&logTime));
    timeStr[strlen(timeStr)-1] = '\0';
    fprintf(file,"%s : %s\n",timeStr,message);
    fclose(file);
}


#endif //SRC_LOGGER_H
