
/*

Program:	Parallel processing experiment
Author:		Lindsay Meek
Version:	28/2/94

*/

#include "lang.h"
#include <exec/ports.h>
#include <exec/tasks.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <exec/semaphores.h>

/* Prototypes */
void AddHead(struct List *list, struct Node *node);
void AddTail(struct List *list, struct Node *node);
void Remove(struct Node *node);
struct Node * RemHead(struct List *list);
struct Node * RemTail(struct List *list);
void Enqueue(struct List *list, struct Node *node);
struct Node * FindName(struct List *list, const char *name);
void InitSemaphore(struct SignalSemaphore *sigSem);
void ObtainSemaphore(struct SignalSemaphore *sigSem);
void ReleaseSemaphore(struct SignalSemaphore *sigSem);
void *AllocMem(),FreeMem(),CreateProc(),*CreatePort();
void *GetMsg(),*WaitPort(),DeletePort(),PutMsg();
void CopyMem(char *source, char *dest, long size);
long Wait(long signalSet);
void Signal(struct Task *task, long signalSet);
struct Task * FindTask(const char *name);
long AllocSignal(long signalNum);
void FreeSignal(long signalNum);

struct List **Variables;	/* List of variables hashed by the lower 5 bits of first variable character */
struct SignalSemaphore VarScan;	/* List scanning semaphore */

/* Variable request by reader */
typedef struct VARREQ {
struct Node ReqNode;
struct Task *ReqTask;
ULONG ReqMask;
BYTE ReqSig;
} VARREQ;

typedef struct VARIABLE {
struct Node VarNode;	/* Attach node for variable */
char VarName[64];	/* Variable name */
short VarSize;		/* Variable size in bytes */
int VarFilled;		/* Var has been updated without a read */
void *VarValue;		/* Pointer to variable value */
struct List VarList;	/* List of requesters */
} VARIABLE;

/* List based matching */
struct Node *MatchName(char *name)
{
	struct Node *C;
	struct Node *N;
	char *p1,*p2,c1,c2;

	/* look up variable in correct hashing chain */
	C=Variables[name[0]&31]->lh_Head;

	N=C->ln_Succ;
	while(N!=NULL)
	{
		p1=C->ln_Name;
		p2=name;

		c1=*p1++;
		c2=*p2++;
		while(c1!=0 && c2!=0)
		{
			if(c1!=c2)	break;

			c1=*p1++;
			c2=*p2++;
		}		

		if(c1==0 && c2==0)	return C;
			
		C=N;
		N=C->ln_Succ;
	}

	return NULL;
}

/* 
	find the variable pointed to by the name
	if not found, create an entry
*/

VARIABLE *findcreate(char *var,short size)
{
	VARIABLE *v;

	/* Quickfind, or search the variable list */

	v=(VARIABLE *)MatchName(var);

	if(v==NULL)
	{
		/* Create a new variable */

		v=AllocMem(sizeof(VARIABLE),MEMF_PUBLIC);
		if(v!=NULL)
		{
			v->VarNode.ln_Name = v->VarName;
			v->VarNode.ln_Pri  = -strlen(var);
			strcpy(v->VarName,var);
			v->VarSize=size;
			v->VarFilled=0;
			NewList(&v->VarList);

			v->VarValue=AllocMem(size,MEMF_PUBLIC);
			if(v->VarValue!=NULL)
			{
				/* Store variable in correct hash tree */
				Enqueue(Variables[var[0]&31],&v->VarNode);
			}
			else
			{
				FreeMem(v,sizeof(VARIABLE));
				v=NULL;
			}
		}
	}

	return v;
}

/* read a named variable */
void read3(char *var,void *addr,short size)
{
	VARIABLE *v;
	VARREQ *vr;

	ObtainSemaphore(&VarScan);

	v=findcreate(var,size);

	if(v!=NULL)
	{
		/* suspend if no data */
		if(!v->VarFilled)
		{	
			vr=(VARIABLE *)(FindTask(NULL)->tc_UserData);
	
			AddTail(&v->VarList,&vr->ReqNode);

			ReleaseSemaphore(&VarScan);

			Wait(vr->ReqMask);

			ObtainSemaphore(&VarScan);
		}
		else
			v->VarFilled=0;		/* Stop other readers */

		/* Copy the data */
		CopyMem(v->VarValue,addr,size);
	}
	else
	{
		#ifdef DEBUG
			printf("VarAlloc in Read failed\n");
		#endif
	}
	
	ReleaseSemaphore(&VarScan);	
}

/* write a named variable */
void write3(char *var,void *addr,short size)
{
	VARIABLE *v;
	VARREQ *vr;

	ObtainSemaphore(&VarScan);

	v=findcreate(var,size);

	if(v!=NULL)
	{
		/* Update value */
		CopyMem(addr,v->VarValue,size);

		vr=(VARREQ *)v->VarList.lh_Head;

		if(vr->ReqNode.ln_Succ != NULL)
		{
			/* Signal the top waiting task */
			Remove(&vr->ReqNode);
			Signal(vr->ReqTask,vr->ReqMask);
		}
		else
			v->VarFilled=1;	/* Tell the readers that data is here */
	}
	else
	{
		#ifdef DEBUG
			printf("VarAlloc in Write failed\n");
		#endif
	}

	ReleaseSemaphore(&VarScan);	
}

int Forks;

int real_main(int argc,char **argv);	/* the user main */

void fork_wait(void)
{
	/* wait for all forked tasks to finish */
	while(Forks != 0) Delay(16);
}

main(int argc,char **argv)
{
	int rc,i;
	VARIABLE *v,*n;
	struct Task *t;
	VARREQ *vr;

	Forks=0;

	Variables = AllocMem(sizeof(struct List *)*32,MEMF_PUBLIC);

	if(Variables == NULL)
	{
		printf("Could not initialise variable lists\n");
		return 10;
	}

	for(i=0;i<32;i++)
	{
		Variables[i]=AllocMem(sizeof(struct List),MEMF_PUBLIC);

		if(Variables[i]==NULL)
		{
			printf("Could not initialise variable lists\n");
			return 10;
		}

		NewList(Variables[i]);
	}

	InitSemaphore(&VarScan);

	vr=AllocMem(sizeof(VARREQ),MEMF_PUBLIC);
	if(vr==NULL)
	{
		printf("Could not initialise variable requester for parent\n");
		return 11;
	}

	t=FindTask(NULL);
	t->tc_UserData=(APTR)vr;
	vr->ReqTask=t;
	vr->ReqSig=AllocSignal(-1);

	if(vr->ReqSig<0)
	{
		printf("Could not allocate signal for parent\n");
		return 12;
	}

	vr->ReqMask=1L<<vr->ReqSig;
	
	rc=real_main(argc,argv);

	fork_wait();

	/* eat variables */
	ObtainSemaphore(&VarScan);

	for(i=0;i<32;i++)
	{
		v=(VARIABLE *)Variables[i]->lh_Head;
		while(v->VarNode.ln_Succ != NULL)
		{
			n=(VARIABLE *)v->VarNode.ln_Succ;

			Remove(&v->VarNode);
			FreeMem(v->VarValue,v->VarSize);
			FreeMem(v,sizeof(VARIABLE));
	
			v=n;
		}

		FreeMem(Variables[i],sizeof(struct List));
	}

	FreeMem(Variables,sizeof(struct List *)*32);

	ReleaseSemaphore(&VarScan);

	/* kill varreq for this task */
	FreeSignal(vr->ReqSig);
	FreeMem(vr,sizeof(VARREQ));

	return rc;
}

/* fork a new process */

typedef struct INFO {
	void (*pc)();
	struct MsgPort *dback;
	struct Message dmsg;
	} INFO;

#define CTOB(x) (void *)(((long)(x))>>2)    /*	BCPL conversion */
#define BTOC(x) (void *)(((long)(x))<<2)

extern void forkstub();

/* fork a new process at the given priority */
fork(void (*pc)(void),int pri)
{
	INFO *info;
	char name[64];

	info=AllocMem(sizeof(INFO),MEMF_PUBLIC);
	if(info==NULL) return 0;

	info->dback = CreatePort(NULL,0);

	if(info->dback == NULL)
	{
		FreeMem(info,sizeof(INFO));
		return 0;
	}

	info->pc    = pc;

	/* Tell the new process where the info block is */
	sprintf(name,"%lu",info);

	CreateProc(name, pri, CTOB(forkstub), 4096);

	WaitPort(info->dback);			    /* handshake startup    */
   	GetMsg(info->dback);			    /* remove dummy msg     */
   	DeletePort(info->dback);
	
	FreeMem(info,sizeof(INFO));

	return 1;
}

changepri(int pri)
{
	struct Task *t=FindTask(NULL);
	int oldpri;

	oldpri = t->tc_Node.ln_Pri;

	t->tc_Node.ln_Pri = (UBYTE)pri;

	return oldpri;
}

/* called by the new process to initialise itself */
forkhandler()
{
	struct Task *task;
	INFO *i;
	VARREQ *vr;
	void (*pc)();

	task=FindTask(NULL);	/* find task name */

	sscanf(task->tc_Node.ln_Name,"%lu",&i);	/* convert it to a long unsigned */

	pc=i->pc;		/* get the start pc of the process */

	/* Allocate the var requester */
	vr=AllocMem(sizeof(VARREQ),MEMF_PUBLIC);
	if(vr!=NULL)
	{
	task->tc_UserData=(APTR)vr;
	
	vr->ReqTask=task;
	vr->ReqSig=AllocSignal(-1);
	vr->ReqMask=1L<vr->ReqSig;

	Forbid();
		Forks++;
	Permit();

	PutMsg(i->dback,&i->dmsg);	/* tell fork we're going */

	pc();				/* start the process */

	Forbid();
		Forks--;
	Permit();

	FreeSignal(vr->ReqSig);
	FreeMem(vr,sizeof(VARREQ));
	}
	else
	{
		PutMsg(i->dback,&i->dmsg);	/* tell fork we're going even if we're aborting */
	}
}

/* assembly stub to take the new dos created process and make it C friendly */

#asm
	public	_forkstub
	public	_forkhandler
	public	_geta4

	cseg
	nop
	nop
	nop

_forkstub:

	nop
	nop
	movem.l D2-D7/A2-A6,-(sp)

	jsr	_geta4		;get the global data pointer
	jsr	_forkhandler	;call the process code

	movem.l (sp)+,D2-D7/A2-A6
	rts

#endasm
