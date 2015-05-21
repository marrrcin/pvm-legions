//tract
typedef struct _res
{
	int id;
	int size;
	int state;
} Resource;

//legion
typedef struct _proc
{
	int id;
	long ticketNumber;
	int size;
} Process;

typedef enum _b
{
	true,
	false
} bool;

#define ANY -1

#define ACCEPT 7

//message types and theirs structures (order of pvm_pk*)
#define MSG_SYNC 1
/*
	sync from master to slaves on pvm_spawn
	contents:
	0. master Id
	1. number of legions (int)
	2. array of legion sizes (int[])
	3. array of legion ids (int[])
	4. number of tracts (int)
	5. array of tract sizes (int[])
*/

#define MSG_REQUEST 3
/*
	send between slaves to request entrance into critical section (use tract)
	contents:
	1. legion id (int)
	2. ticket number (unsigned long)
	3. tract id (int)
*/

#define MSG_ALLOW 5
/*
	send between slaves to allow access into cricital section (use tract)
	contents:
	1. resource state (int)
*/

#define MSG_SINGLE_STRING 1234

//input file structure:
/*
	legionsCount
	legionSize
	legionSize
	(...)
	tractsCount
	tractSize
	tractSize
	(...)
	
*/

