
/*

Program:	ReqTools file requester
Author:		Lindsay Meek
Version:	10/4/94

*/

#include "req.h"

#include <exec/types.h>
#include <string.h>

void *OpenLibrary();
void *rtAllocRequestA();
long rtFileRequest();

#include <libraries/reqtools.h>

struct ReqToolsBase *ReqToolsBase=NULL;
struct rtFileRequester *filereq=NULL;

bool req_INIT()
{
	if (!(ReqToolsBase = OpenLibrary (REQTOOLSNAME, REQTOOLSVERSION))) 
		return false;

	if (filereq = rtAllocRequestA (RT_FILEREQ, NULL)) 
		return true;
	else
		return false;
}

bool req_CLOSE()
{
	if(filereq != NULL)
	{
		rtFreeRequest (filereq);
		filereq=NULL;
	}
	if(ReqToolsBase != NULL)
	{
		CloseLibrary(ReqToolsBase);
		ReqToolsBase=NULL;
	}
	return true;
}
	
bool req_FILE(text *full_name)
{
	char file[64];

	full_name[0]=0;
	file[0]=0;

	if (rtFileRequest (filereq, file, "Pick a Protracker Module", TAG_END))
	{
		if(strlen(filereq->Dir)!=0)
		{
			strcpy(full_name,filereq->Dir);

			if(filereq->Dir[strlen(filereq->Dir)-1]!=':')
				strcat(full_name,"/");
		}
	
		strcat(full_name,file);
	}

	if(strlen(full_name)==0)
		return false;
	else
		return true;
}
