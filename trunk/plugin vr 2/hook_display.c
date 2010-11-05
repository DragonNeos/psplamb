/*------------------------------------------------------------------------------*/
/* hook_display																	*/
/*------------------------------------------------------------------------------*/
#include <pspkernel.h>
#include "debug.h"
#include "hook.h"

typedef struct PspGeContext
{
	unsigned int context[512];
} PspGeContext;

typedef struct PspGeListArgs
{
	unsigned int	size;
	PspGeContext* context;
} PspGeListArgs;

static int START_VRAM 	= 0x04000000;
static int END_VRAM 	= 0x041FFFFF;
static int START_RAM 	= 0x08000000;
static int END_RAM 		= 0x09FFFFFF;

//add list on the end
static int (*sceGeListEnQueue)(const void *, void *, int, PspGeListArgs *) = NULL;
//add list on the head
static int (*sceGeListEnQueueHead)(const void *, void *, int, PspGeListArgs *) = NULL;
//remove list 
static int (*sceGeListDeQueue)(int) = NULL;
//list synchro
static int (*sceGeListSync)(int, int) = NULL;
/* * Update the stall address for the specified queue. */
static int (*sceGeListUpdateStallAddr)(int, void *) = NULL;
/* Retrive the current value of a GE command. */
static unsigned int (*sceGeGetCmd)(int) = NULL;
/* Wait for drawing to complete.*/
static int (*sceGeDrawSync)(int) = NULL;
static int (*sceDisplaySetFrameBuf_Func)( void *, int, int, int ) = NULL;
static int (*sceDisplayWaitVblank_Func)( void ) = NULL;
static int (*sceDisplayWaitVblankCB_Func)( void ) = NULL;
static int (*sceDisplayWaitVblankStart_Func)( void ) = NULL;
static int (*sceDisplayWaitVblankStartCB_Func)( void ) = NULL;

/*---------------------------------------------------------------------------*/
/*Drakon test hook									 */
/*---------------------------------------------------------------------------*/
static int test2 = 0;
static int numerek = 0;

//dispay list stuff
unsigned int *currentDList;
unsigned int currentDList_addr = 0;
unsigned int currentDList_stall_addr = 0;
unsigned int currentDList_cbid = 0;
unsigned int currentDList_arg_addr = 0;

unsigned int *currentDList_pc = 0;
int currentDList_status = 0;

unsigned int currentDList_stack[64];
unsigned int currentDList_basestack[64];
unsigned int currentDList_stackIndex = 0;

int currentDList_finished = 0;
int currentDList_ended = 0;
int currentDList_reset = 0;
int currentDList_restarted = 0;
//end dispay list stuff

//vertex stuff
unsigned int vertexBase = 0;
unsigned int vertexPntr = 0;
unsigned int base = 0;
unsigned int baseOffset = 0;
unsigned int indexPntr = 0;



//end vertex stuff
unsigned int addressMask = 0x3FFFFFFF;


int can_parse = 0;
int primitiveCounter = 0;

float getFloat(unsigned int data){ data<<=8; return *(float*)(&data);} 

void debugCommand(unsigned int command,unsigned int argument)
{
	float fargument = getFloat(argument);

	char text[70];
	sprintf(text,"addres: %x command: %u arg: %u farg: %f\n",currentDList,command,argument,fargument);
	debuglog(text);
}

void ExecuteCommand(unsigned int command,unsigned int argument)
{
	//commands
	switch(command)
	{
		case 0x00://NOP
		{
			int test = 0;
			
			if( ((*currentDList) & addressMask) >=  START_RAM || ((*currentDList) & addressMask) <= END_RAM )
				test = 1;
				
			if( ((*currentDList) & addressMask) >=  START_VRAM || ((*currentDList) & addressMask) <= END_VRAM )
				test = 1;
				
			if(test == 0)
			{
				char text[70];
				sprintf(text,"addres: %x Wrong read addres!\n",currentDList);
				//end
				currentDList_ended = 1;
				currentDList_finished = 1;
			}
			
			debugCommand(command,argument);
		}
		break;
		
		case 0x01://VADR
		{
			vertexBase = ( base | argument) + baseOffset;
			
			/*if(vertex_adr_base >= START_VRAM && vertex_adr_base < END_VRAM)
			{
				vertex_adr_base = (0x44000000 | (vertex_adr_base & 0x3FFFFFFF));
			}
			if(vertex_adr_base >= START_RAM && vertex_adr_base < END_RAM)
			{
				vertex_adr_base = (START_RAM | (vertex_adr_base & 0x3FFFFFFF));
			}*/
			
			debugCommand(command,argument);
		}
		break;
		
		case 0x02://IADR index array adress
		{
			indexPntr = ( base | argument) + baseOffset;
			
			/*if(vertex_adr_base >= START_VRAM && vertex_adr_base < END_VRAM)
			{
				vertex_adr_base = (0x44000000 | (vertex_adr_base & 0x3FFFFFFF));
			}
			if(vertex_adr_base >= START_RAM && vertex_adr_base < END_RAM)
			{
				vertex_adr_base = (START_RAM | (vertex_adr_base & 0x3FFFFFFF));
			}*/
			
			debugCommand(command,argument);
		}
		break;
		
		case 0x08: //JUMP
		{
			unsigned int jump = (( base | argument) + baseOffset) & 0xFFFFFFFC;
			currentDList = (unsigned int*)jump;
			
			debugCommand(command,argument);
		}
		break;

		case 0x09: //BJUMP
		{
			//nothing right now...
			debugCommand(command,argument);
		}
		break;
		
		case 0x10: //BASE
		{
			base = (argument << 8) & 0xff000000;
			
			debugCommand(command,argument);
		}
		break;
		
		case 0x13: //OFF_ADR
		{
			baseOffset = argument << 8;
			
			debugCommand(command,argument);
		}
		break;
		
		case 0x14: //ORIGIN_ADR
		{
			baseOffset = currentDList - 1;
			
			debugCommand(command,argument);
		}
		break;
		
		case 0x0A: //CALL
		{
			currentDList_stack[currentDList_stackIndex] = currentDList;
			currentDList_basestack[currentDList_stackIndex] = base;
			currentDList_stackIndex++;
			
			unsigned int call = (( base | argument) + baseOffset) & 0xFFFFFFFC;
			currentDList = (unsigned int*)call;
			
			debugCommand(command,argument);
		}
		
		case 0x0B: //RET
		{
			currentDList_stackIndex--;
			
			currentDList = (unsigned int*)currentDList_stack[currentDList_stackIndex];
			base = currentDList_basestack[currentDList_stackIndex];
			
			debugCommand(command,argument);
		}
		break;
		
		case 0x0C: //END
		{
			currentDList_ended = 1;
			
			debugCommand(command,argument);
		}
		break;
		
		case 0x0F: //FINISH
		{
			currentDList_finished = 1;
			
			debugCommand(command,argument);
		}
		break;
		
		
		//graphics commands
		case 0x04: //PRIM
		{
			int numberOfVertex = argument & 0xFFFF;
			int type = ((argument >> 16) & 0x7);

			char text[70];
			sprintf(text,"%d %d\n",numberOfVertex,type);
			debuglog(text);
			
			debugCommand(command,argument);
		}
		break;
		
		case 0x12: //VERTEX TYPE
		{
			
			debugCommand(command,argument);
		}
		break;
		
	}
}

void RipData()
{
	int k1 = pspSdkSetK1(0);
	
	if(can_parse == 1)
	{
		int listHasEnded = 0;
        currentDList_status = 2;//PSP_GE_LIST_DRAWING;
		
		char text[70];
		sprintf(text,"addres1: %x addres2: %x\n",currentDList,currentDList_stall_addr);
		debuglog(text);
		
		while (!listHasEnded)
		{
			if (currentDList_ended)
			{
				if (currentDList_finished) 
				{
					listHasEnded = 1;
					can_parse = 0;
					break;
				}
			}else
			if(currentDList >= currentDList_stall_addr)
			{
				listHasEnded = 1;
				can_parse = 0;
				break;
			}else
			{
				unsigned int command  = (*currentDList) >> 24;
				unsigned int argument = (*currentDList) & 0x00FFFFFF;
				
				currentDList++;
				
				ExecuteCommand(command,argument);
			}		
		}
	}

	
	
	pspSdkSetK1(k1); 
}

/*------------------------------------------------------------------------------*/
/* MyDisplaySetFrameBuf															*/
/*------------------------------------------------------------------------------*/
static int MyDisplaySetFrameBuf( void *topaddr, int bufferwidth, int pixelformat, int sync )
{
	RipData();
	int ret = sceDisplaySetFrameBuf_Func( topaddr, bufferwidth, pixelformat, sync );
	return( ret );
}

/*------------------------------------------------------------------------------*/
/* MyDisplayWaitVblank															*/
/*------------------------------------------------------------------------------*/
static int MyDisplayWaitVblank( void )
{
	RipData();
	int ret = sceDisplayWaitVblank_Func();
	return( ret );
}

/*------------------------------------------------------------------------------*/
/* MyDisplayWaitVblankCB														*/
/*------------------------------------------------------------------------------*/
static int MyDisplayWaitVblankCB( void )
{
	RipData();
	int ret = sceDisplayWaitVblankCB_Func();
	return( ret );
}

/*------------------------------------------------------------------------------*/
/* MyDisplayWaitVblankStart														*/
/*------------------------------------------------------------------------------*/
static int MyDisplayWaitVblankStart( void )
{
	RipData();
	int ret = sceDisplayWaitVblankStart_Func();
	return( ret );
}

/*------------------------------------------------------------------------------*/
/* MyDisplayWaitVblankStartCB													*/
/*------------------------------------------------------------------------------*/
static int MyDisplayWaitVblankStartCB( void )
{
	RipData();
	int ret = sceDisplayWaitVblankStartCB_Func();
	return( ret );
}


static int MYsceGeListUpdateStallAddr(int qid, void *stall)
{
	int ret = sceGeListUpdateStallAddr(qid, stall);
	return ret;
}

static unsigned int MYsceGeGetCmd(int cmd)
{
	unsigned int ret = sceGeGetCmd(cmd);
	
	return ret;
}

static int MYsceGeListEnQueue(const void *list, void *stall, int cbid, PspGeListArgs *arg)
{

	int k1 = pspSdkSetK1(0);
	
	currentDList = (unsigned int*)list;
	currentDList_addr = (unsigned int)&list;
	currentDList_stall_addr = (unsigned int)&stall;
	currentDList_cbid = cbid;
	
	pspSdkSetK1(k1); 
	
	int ret = sceGeListEnQueue(list, stall, cbid, arg);
	return( ret );
}

static int MYsceGeListEnQueueHead(const void *list, void *stall, int cbid, PspGeListArgs *arg)
{
	int ret = sceGeListEnQueueHead(list, stall, cbid, arg);
	return( ret );
}

static int MYsceGeListDeQueue(int qid)
{
	int ret = sceGeListDeQueue(qid);
	return( ret );
}

static int MYsceGeListSync(int qid, int syncType)
{
	//psp is reseting all video commands right here

	int ret = sceGeListSync(qid, syncType);
	return( ret );
}

static int MYsceGeDrawSync(int syncType)
{
	int ret = sceGeDrawSync(syncType);
	return ret;
}

/*------------------------------------------------------------------------------*/
/* hookDisplay																	*/
/*------------------------------------------------------------------------------*/
void hookDisplay( void )
{
	debuglog("Start hooking display\n");
	
	SceModule *module = sceKernelFindModuleByName( "sceDisplay_Service" );
	if ( module == NULL ){ return; }
	
	if ( sceDisplaySetFrameBuf_Func == NULL )
	{
		sceDisplaySetFrameBuf_Func = HookNidAddress( module, "sceDisplay", 0x289D82FE );
		void *hook_addr = HookSyscallAddress( sceDisplaySetFrameBuf_Func );
		HookFuncSetting( hook_addr, MyDisplaySetFrameBuf );
	}

	if ( sceDisplayWaitVblank_Func == NULL 
	){
		sceDisplayWaitVblank_Func = HookNidAddress( module, "sceDisplay", 0x36CDFADE );
		void *hook_addr = HookSyscallAddress( sceDisplayWaitVblank_Func );
		HookFuncSetting( hook_addr, MyDisplayWaitVblank );
	}

	if ( sceDisplayWaitVblankCB_Func == NULL )
	{
		sceDisplayWaitVblankCB_Func = HookNidAddress( module, "sceDisplay", 0x8EB9EC49 );
		void *hook_addr = HookSyscallAddress( sceDisplayWaitVblankCB_Func );
		HookFuncSetting( hook_addr, MyDisplayWaitVblankCB );
	}

	if ( sceDisplayWaitVblankStart_Func == NULL )
	{
		sceDisplayWaitVblankStart_Func = HookNidAddress( module, "sceDisplay", 0x984C27E7 );
		void *hook_addr = HookSyscallAddress( sceDisplayWaitVblankStart_Func );
		HookFuncSetting( hook_addr, MyDisplayWaitVblankStart );
	}

	if ( sceDisplayWaitVblankStartCB_Func == NULL )
	{
		sceDisplayWaitVblankStartCB_Func = HookNidAddress( module, "sceDisplay", 0x46F186C3 );
		void *hook_addr = HookSyscallAddress( sceDisplayWaitVblankStartCB_Func );
		HookFuncSetting( hook_addr, MyDisplayWaitVblankStartCB );
	}

	//hook drakon modules
	SceModule *module2 = sceKernelFindModuleByName( "sceGE_Manager" );
	if ( module2 == NULL ){ return; }
	
	if ( sceGeListEnQueue == NULL )
	{
		sceGeListEnQueue = HookNidAddress( module2, "sceGe_user", 0xAB49E76A );
		void *hook_addr = HookSyscallAddress( sceGeListEnQueue );
		HookFuncSetting( hook_addr, MYsceGeListEnQueue );
	}
	
	if ( sceGeListEnQueueHead == NULL )
	{
		sceGeListEnQueueHead = HookNidAddress( module2, "sceGe_user", 0x1C0D95A6 );
		void *hook_addr = HookSyscallAddress( sceGeListEnQueueHead );
		HookFuncSetting( hook_addr, MYsceGeListEnQueueHead );
	}
	
	if ( sceGeListDeQueue == NULL )
	{
		sceGeListDeQueue = HookNidAddress( module2, "sceGe_user", 0x5FB86AB0 );
		void *hook_addr = HookSyscallAddress( sceGeListDeQueue );
		HookFuncSetting( hook_addr, MYsceGeListDeQueue );
	}
	
	if ( sceGeListSync == NULL )
	{
		sceGeListSync = HookNidAddress( module2, "sceGe_user", 0x03444EB4 );
		void *hook_addr = HookSyscallAddress( sceGeListSync );
		HookFuncSetting( hook_addr, MYsceGeListSync );
	}
	
	//another test
	if ( sceGeListUpdateStallAddr == NULL )
	{
		sceGeListUpdateStallAddr = HookNidAddress( module2, "sceGe_user", 0xE0D68148 );
		void *hook_addr = HookSyscallAddress( sceGeListUpdateStallAddr );
		HookFuncSetting( hook_addr, MYsceGeListUpdateStallAddr );
	}
	
	if ( sceGeGetCmd == NULL ){
		sceGeGetCmd = HookNidAddress( module2, "sceGe_user", 0xDC93CFEF );
		void *hook_addr = HookSyscallAddress( sceGeGetCmd );
		HookFuncSetting( hook_addr, MYsceGeGetCmd );
	}
	
	if ( sceGeDrawSync == NULL ){
		sceGeDrawSync = HookNidAddress( module2, "sceGe_user", 0xB287BD61 );
		void *hook_addr = HookSyscallAddress( sceGeDrawSync );
		HookFuncSetting( hook_addr, MYsceGeDrawSync );
	}
		
	debuglog("End hooking display\n");
}
