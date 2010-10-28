/*------------------------------------------------------------------------------*/
/* hook_display																	*/
/*------------------------------------------------------------------------------*/
#include <pspkernel.h>
#include "../debug.h"
#include "../hook.h"

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

//test
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

unsigned int* MYlocal_list;
unsigned int current_list_addres = 0;
unsigned int stall_addres = 0;
unsigned int adress_arr[200];
unsigned int baseOffset_arr[200];
unsigned int baseOffset = 0;
int adress_number = 0;

int can_parse = 0;

//skinning
float boneWeights[8]; 
//morphing
float morph_weight[8];


int max(int a,int b)
{
	return (b<a)?a:b;
}

int vertexSize = 0;
int alignmentSize = 0;
int size_mapping[4] = { 0, 1, 2, 4 };
int size_padding[4] = { 0, 0, 1, 3 };
int color_size_mapping[8] = { 0, 0, 0, 0, 2, 2, 2, 4 };
int color_size_padding[8] = { 0, 0, 0, 0, 1, 1, 1, 3 };


void RipData()
{
int k1 = pspSdkSetK1(0);
	if(can_parse == 2)
	{
		char texto[50];
		sprintf(texto,"\nList nr: %d - sceGeDrawSync\n",numerek);
		debuglog(texto);
				
		int list_parsing = 1;
		
		unsigned int vertex_adr = 0;
		unsigned int vertex_adr_temp = 0;
		unsigned int vertex_adr_base = 0;
		unsigned int index_adr = 0;
		unsigned int base = 0;

		baseOffset = 0;
		
		int itexture = 0;
		int icolor = 0;
		int inormal = 0;
		int iposition = 0;
		int iweight = 0;
		int iindex = 0;
		int iskinningWeightCount = 0;
		int imorphingVertexCount = 0;
		int itransform2D         = 0;
		
		int 	model_upload_start = 0;
		int     model_upload_x = 0;
		int     model_upload_y = 0;
		float 	model_matrix[3][4];
		
		int primitiveCounter = 0;
		
		//for morphing
		int textureOffset = 0;
		int colorOffset = 0;
		int normalOffset = 0;
		int positionOffset = 0;
		int oneSize = 0;

		//for bone matrix
		int bone_upload_start = 0;
		int bone_upload_x = 0;
		int bone_upload_y = 0;
		int bone_matrix_offset = 0;
		int boneMatrixIndex = 0;
		float bone_matrix[4 * 3];
		float bone_uploaded_matrix[8][4 * 3];

		//info about lists
		char listaText[200];
		sprintf(listaText,"list: %x stall: %x\n",current_list_addres,stall_addres);
		debuglog(listaText);

		while(list_parsing == 1)
		{
			int command = 0;
			int argument = 0;
			int argif = 0;
			float fargument = 0.0f;

			//current addres
			//current_list_addres = (unsigned int)&MYlocal_list;
			//

			command = (*MYlocal_list) >> 24;
			argument = (*MYlocal_list) & 0xffffff;
			
			MYlocal_list++;

			if(command!= 12) //END
			{
				
				//jumping support - test fix
				if(command == 0x08) //Jump To New Address (BASE)
				{
					//unsigned int npc = (base & 0xff000000) | (argument & 0x00ffffff);
					//unsigned int npc = ((base | argument) & 0xFFFFFFFC) + baseOffset;
					unsigned int npc = ((base | argument) + baseOffset) & 0xFFFFFFFC;
					
					//jump to new address
					MYlocal_list = (unsigned int*)npc;

					//char text[100];
					//sprintf(text,"JUMP: %x\n",npc);
					//debuglog(text);
				}else
				
				if(command == 0x09) //BJUMP - conditional jump
				{
					//http://hitmen.c02.at/files/yapspd/psp_doc/chap11.html - some info
					//unsigned int npc = (base & 0xff000000) | (argument & 0x00ffffff);
					//if (npc != 0)
					//{
					//	//jump to new address
					//	MYlocal_list = (unsigned int*)npc;
					//}
					//char text[100];
					//sprintf(text,"BJUMP: %x\n",npc);
					//debuglog(text);
				}else
				
				if(command == 0x10) //base array addres
				{
					//base = argument << 8;
					base = (argument << 8) & 0xff000000;
					//char text[100];
					//sprintf(text,"BASE: %x\n",base);
					//debuglog(text);
					
				}else

				if(command == 0x13) //???   Offset Address (BASE)
				{
					baseOffset = (argument << 8);
				}else

				if(command == 0x14) //???   Origin Address (BASE)
				{
					baseOffset = MYlocal_list - 1;
				}else

				
				if(command == 0x0A) //call
				{
					//hmm bassicly its calling another display list
					//right now i should put my current list on some sort of stack
					//and then change my list to another one
					//unsigned int npc = (base & 0xff000000) | (argument & 0x00ffffff);
					unsigned int npc = ((base | argument) + baseOffset ) & 0xFFFFFFFC;

					//save adres
					adress_arr[adress_number] = MYlocal_list;
					baseOffset_arr[adress_number] = baseOffset;
					adress_number++;
					
					MYlocal_list = (unsigned int*)npc;
					//char text[100];
					//sprintf(text,"CALL: %x\n",npc);
					//debuglog(text);
				}else
				
				if(command == 0x0B) //Return From Call
				{
					adress_number--;
					MYlocal_list = (unsigned int*)adress_arr[adress_number];
					baseOffset = baseOffset_arr[adress_number];

					//char text[100];
					//sprintf(text,"RET: %x\n",adress_arr[adress_number]);
					//debuglog(text);

				}else
								
				if(command == 0x3A)//       MDL     Model Matrix Select
				{
					model_upload_x = argument % 3;
					model_upload_y = argument / 3;
				}else

				if(command == 0x3B)// MODEL       Model Matrix Upload
				{
					argif = argument << 8;
					memcpy(&fargument,&argif,4);

					if (model_upload_y < 4)
					{
						model_matrix[model_upload_x][model_upload_y] = fargument;
						
						char text[100];
						sprintf(text,"mat [%d][%d] = %f\n",model_upload_x,model_upload_y,fargument);
						debuglog(text);
						
						model_upload_x++;
						if(model_upload_x == 3)
						{
							model_upload_x = 0;
							model_upload_y++;
						}

					}

				}else
			
		
				if(command == 0x01) //vertex array adress
				{
					vertex_adr_base = (base | argument) + baseOffset;

					//char text[50];
					//sprintf(text,"Vert arr adress: %x\n",vertex_adr_base);
					//debuglog(text);
					
					if(vertex_adr_base >= START_VRAM && vertex_adr_base < END_VRAM)
					{
						//debuglog("Vert arr in VRAM!!\n");
						vertex_adr_base = (0x44000000 | (vertex_adr_base & 0x3FFFFFFF));
						//char text[50];
						//sprintf(text,"Vert arr adress: %x\n",vertex_adr_base);
						//debuglog(text);
					}
					if(vertex_adr_base >= START_RAM && vertex_adr_base < END_RAM)
					{
						//debuglog("Vert arr in RAM\n");
						vertex_adr_base = (START_RAM | (vertex_adr_base & 0x3FFFFFFF));
						//char text[50];
						//sprintf(text,"Vert arr adress: %x\n",vertex_adr_base);
						//debuglog(text);
					}
					
				}else
				
				if(command == 0x02) //index array adress
				{
					index_adr = (base | argument) + baseOffset;

					//char text[50];
					//sprintf(text,"Index arr adress: %x\n",index_adr);
					//debuglog(text);
					
					if(index_adr >= START_VRAM && index_adr <END_VRAM)
					{
						//debuglog("index arr in VRAM!!\n");
						index_adr = (0x44000000 | (index_adr & 0x3FFFFFFF));
						//char text[50];
						//sprintf(text,"Index arr adress: %x\n",index_adr);
						//debuglog(text);
					}
					if(index_adr >= START_RAM && index_adr <END_RAM)
					{
						//debuglog("index arr in RAM\n");
						index_adr = (START_RAM | (index_adr & 0x3FFFFFFF));
						//char text[50];
						//sprintf(text,"Index arr adress: %x\n",index_adr);
						//debuglog(text);
					}
					
				}else

				//bone matrix support
				if (command == 0x2A)
				{
					//bone_matrix_offset = argument / (4*3);
					//bone_upload_start = 1;

					boneMatrixIndex = argument;
					
					//char text[50];
					//sprintf(text,"bone_matrix_offset: %u\n",bone_matrix_offset);
					//debuglog(text);
				}else

				if (command == 0x2B)
				{
					argif = argument << 8;
					memcpy(&fargument,&argif,4);

					int matrixIndex  = boneMatrixIndex / 12;
					int elementIndex = boneMatrixIndex % 12;

					if (matrixIndex < 8)
					{
						bone_uploaded_matrix[matrixIndex][elementIndex] = fargument;
						boneMatrixIndex++;
					}

				}else

				//morphing support
				if(command == 0x2C || command == 0x2D || command == 0x2E || command == 0x2F || command == 0x30 || command == 0x31 || command == 0x32 || command == 0x33)
				{
					argif = argument << 8;
					memcpy(&fargument,&argif,4);
					
					morph_weight[command - 0x2C] = fargument;

					//char text[100];
					//sprintf(text,"morph weight: %u %f\n",(command - 0x2C),fargument);
					//debuglog(text);
				}else
				
				
				if(command == 0x04) //prim - primitive
				{
					int numberOfVertex = 0,type = 0;
					
					numberOfVertex = argument & 0xFFFF;
					type = ((argument >> 16) & 0x7);
					
					char text[100];
					sprintf(text,"vert num: %u vert type: %d\n",numberOfVertex,type);
					debuglog(text);
			
					
				}else
				
				if(command == 0x12)//vertex type
				{
					itexture             = (argument >>  0) & 0x3;
					icolor               = (argument >>  2) & 0x7;
					inormal              = (argument >>  5) & 0x3;
					iposition            = (argument >>  7) & 0x3;
					iweight              = (argument >>  9) & 0x3;
					iindex               = (argument >> 11) & 0x3;
					iskinningWeightCount = ((argument >> 14) & 0x7) + 1;
					imorphingVertexCount = ((argument >> 18) & 0x7) + 1;
					itransform2D         = ((argument >> 23) & 0x1) != 0;
					
					char text[100];
					sprintf(text,"t: %uc: %u n: %u p: %u w: %u ind: %u s: %u m: %u t: %u\n",itexture,icolor,inormal,iposition,iweight,iindex,iskinningWeightCount,imorphingVertexCount,itransform2D);
					debuglog(text);
					
				}/*else
				
				{
					argif = argument << 8;
					memcpy(&fargument,&argif,4);
					char text[70];
					sprintf(text,"addres: %x command: %u arg1: %u \n",MYlocal_list,command,argument);
					debuglog(text);
				}*/
				
				
				//default:
				//	char text[70];
				//	sprintf(text,"addres: %x command: %u arg1: %u \n",MYlocal_list,command,argument);
				//	debuglog(text);
				//}
			
				
				
			}else
			if(command == 12 && can_parse ==1)
			{
				debuglog("END OF THE COMMANDS\n");
			
				list_parsing = 0;
				can_parse = 2;
				
			}else
				list_parsing = 0;
		}

		can_parse = 0;
	}

	if(can_parse == 1)
	{
		can_parse = 2;
	}
	
	numerek++;
	
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
	
	//set this list to MYLIST
	MYlocal_list = (unsigned int*)list;
	//set addres to some other int variable
	current_list_addres = (unsigned int)&list;
	//
	stall_addres = (unsigned int)&stall;
	
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
