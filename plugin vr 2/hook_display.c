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
unsigned int vertexAddres = 0;
unsigned int vertexAddresTemp = 0;
unsigned int vertexBase = 0;
unsigned int vertexPntr = 0;
unsigned int base = 0;
unsigned int baseOffset = 0;
unsigned int indexPntr = 0;

//vertex structure
int itexture = 0;
int icolor = 0;
int inormal = 0;
int iposition = 0;
int iweight = 0;
int iindex = 0;
int iskinningWeightCount = 0;
int imorphingVertexCount = 0;
int itransform2D         = 0;

int size_mapping[4] = { 0, 1, 2, 4 };
int size_padding[4] = { 0, 0, 1, 3 };
int color_size_mapping[8] = { 0, 1, 1, 1, 2, 2, 2, 4 };
int color_size_padding[8] = { 0, 0, 0, 0, 1, 1, 1, 3 };

int vertexSize = 0;
int alignmentSize = 0;

int textureOffset = 0;
int colorOffset = 0;
int normalOffset = 0;
int positionOffset = 0;
int oneSize = 0;

typedef struct TextNorVertex
{
	float u,v;
	float nx,ny,nz;
	float x,y,z;
};

typedef struct 
{
	float u,v;
	float x,y,z;
}TexVertex;

typedef struct NorVertex
{
	float nx,ny,nz;
	float x,y,z;
};

typedef struct SkinTexVertex
{
	float w[8];
	float u,v;
	float x,y,z;
};

//texture info
int texture_height[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
int texture_width[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
int texture_storage_mode = 0;



//end vertex stuff
unsigned int addressMask = 0x3FFFFFFF;

//bones stuff
int boneMatrixIndex = 0;
float bone_uploaded_matrix[8][4 * 3];



int can_parse = 0;

int argif = 0;
float fargument = 0.0f;

int primitiveCounter = 0;
SceUID file;

float getFloat(unsigned int data){ data<<=8; return *(float*)(&data);} 

int max(int a,int b)
{
	return (b<a)?a:b;
}

int min(int a,int b)
{
	return (b<a)?b:a;
}

void debugCommand(int command,int argument)
{
	//float fargument = getFloat(argument);

	char text[70];
	sprintf(text,"addres: %x command: %d arg: %d\n",currentDList,command,argument);
	debuglog(text);
}

void ExecuteCommand(int command,int argument)
{
	switch(command)
	{
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
			
			//debugCommand(command,argument);
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
			
			//debugCommand(command,argument);
		}
		break;
		
		//graphics commands
		case 0x04: //PRIM
		{
			int numberOfVertex = argument & 0xFFFF;
			int type = ((argument >> 16) & 0x7);

			//char text[70];
			//sprintf(text,"%d %d\n",numberOfVertex,type);
			//debuglog(text);
			
			
			//start reading vertext info
			//only skinned and not 2d vertices
			if(iweight != 0 && itexture != 0 && itransform2D == 0)
			{
			
				if(file >= 0)
				{
					//how many vertex
					sceIoWrite(file, &numberOfVertex, sizeof(int));
					//vertex type
					sceIoWrite(file, &type, sizeof(int));
				}
				
				//iterate on every vertex
				int vertCounter = 0;
				for(vertCounter = 0; vertCounter < numberOfVertex; vertCounter++ )
				{
					//we must get memory adres of each vertex
					//but we must check if vertex indexind is used
					int indexcount = vertCounter;
					
					//temp data for every vertex
					float boneWeights[8];
					float tu,tv;
					float nx,ny,nz;
					float px,py,pz;
					
					//if there is indexing
					if(indexPntr != 0 && iindex != 0)
					{
						//yes there is indexing
						unsigned int vertexOffset = indexPntr + (indexcount * iindex);
						
						switch(iindex)
						{
							case 1: 
							{
								unsigned char uc8 = 0;
								memcpy(&uc8,(unsigned char *)vertexOffset,1);
								indexcount = (unsigned int)uc8;
							}
							break;
							case 2:
							{
								vertexOffset = (vertexOffset + 1) & ~1;
								short sh16 = 0;
								memcpy(&sh16,(unsigned char *)vertexOffset,2);
								indexcount = (unsigned int)sh16;
							}
							break;
							case 4:
							{
								vertexOffset = (vertexOffset + 3) & ~3;
								unsigned int in32 = 0;
								memcpy(&in32,(unsigned char *)vertexOffset,4);
								indexcount = in32;
							}
							break;
						}
					}
					
					//set vertex addres in memory
					vertexAddres = vertexBase + indexcount * vertexSize;
					//for morphing vertices
					vertexAddresTemp = vertexAddres;
					
					//weight
					if(iweight != 0)
					{
						int weightNumber = 0;
						for(weightNumber = 0; weightNumber < iskinningWeightCount; weightNumber++)
						{
							switch(iweight)
							{
								case 1:
								{
									unsigned char uc8 = 0;
									memcpy(&uc8,(unsigned char *)vertexAddres,1);
									boneWeights[weightNumber] = (float)uc8 /127.0f;
									vertexAddres++;
								}
								break;

								case 2:
								{
									vertexAddres = (vertexAddres + 1) & ~1;
									short s16 = 0;
									memcpy(&s16,(unsigned char *)vertexAddres,2);
									boneWeights[weightNumber] = (float)s16 / 32767.0f;
									vertexAddres+=2;
								}
								break;

								case 3:
								{
									vertexAddres = (vertexAddres + 3) & ~3;
									float f32 = 0.0f;
									memcpy(&f32,(unsigned char *)vertexAddres,4);
									boneWeights[weightNumber] = f32;
									vertexAddres+=4;
								}
								break;
							}
						}
					}
					
					//texture uv
					//texture
					if(itexture != 0)
					{
						switch(itexture)
						{
							case 1:
							{
								char c8 = 0;
								
								memcpy(&c8,(unsigned char *)vertexAddres,1);
								tu = (float)c8;
								vertexAddres++;
								
								memcpy(&c8,(unsigned char *)vertexAddres,1);
								tv = (float)c8;
								vertexAddres++;
								
								tu/=127.0f;
								tv/=127.0f;
							}
							break;
							case 2:
							{
								short s16 = 0;
								vertexAddres = (vertexAddres + 1) & ~1;
								
								memcpy(&s16,(unsigned char *)vertexAddres,2);
								tu = (float)s16;
								vertexAddres+=2;
								
								memcpy(&s16,(unsigned char *)vertexAddres,2);
								tv = (float)s16;
								vertexAddres+=2;
								
								tu/=32767.0f;
								tv/=32767.0f;
							}
							break;
							case 3:
							{
								vertexAddres = (vertexAddres + 3) & ~3;
								
								memcpy(&tu,(unsigned char *)vertexAddres,4);
								memcpy(&tv,(unsigned char *)vertexAddres,4);
							}
						}
					}
					
					//color
					if(icolor != 0)
					{
						switch(icolor)
						{
							case 1:
							case 2:
							case 3:
							{
								vertexAddres+=1;
							}
							break;
							case 4:
							case 5:
							case 6:
							{
								vertexAddres = (vertexAddres + 1) & ~1;
								vertexAddres+=2;
							}
							break;
							case 7:
							{
								vertexAddres = (vertexAddres + 3) & ~3;
								vertexAddres+=4;
							}
							break;
						}
					}
					
					//normal
					if(inormal != 0)
					{
						switch(inormal)
						{
							case 1:
							{
								vertexAddres+=3;
							}
							break;
							case 2:
							{
								vertexAddres = (vertexAddres + 1) & ~1;
								vertexAddres+=6;
							}
							break;
							case 3:
							{
								vertexAddres = (vertexAddres + 3) & ~3;
								vertexAddres+=12;
							}
							break;
						}
					}
					
					//position
					if(iposition != 0)
					{
						switch(iposition)
						{
							case 1:
							{
								char c8 = 0;
								unsigned char uc8 = 0;
								
								// X and Y are signed 8 bit, Z is unsigned 8 bit
								memcpy(&c8,(unsigned char *)vertexAddres,1);
								px = (float)c8;
								vertexAddres++;
								
								memcpy(&c8,(unsigned char *)vertexAddres,1);
								py = (float)c8;
								vertexAddres++;
								
								memcpy(&uc8,(unsigned char *)vertexAddres,1);
								pz = (float)uc8;
								vertexAddres++;
								
								px /= 127.0f;
								py /= 127.0f;
								pz /= 127.0f;
							}
							break;
							
							case 2:
							{
								short s16 = 0;
								vertexAddres = (vertexAddres + 1) & ~1;

								// X and Y are signed 16 bit, Z is unsigned 16 bit
								memcpy(&s16,(unsigned char *)vertexAddres,2);
								px = (float)s16;
								vertexAddres+=2;
								
								memcpy(&s16,(unsigned char *)vertexAddres,2);
								py = (float)s16;
								vertexAddres+=2;
								
								memcpy(&s16,(unsigned char *)vertexAddres,2);
								pz = (float)s16;
								vertexAddres+=2;
								
								px /= 32767.0f;
								py /= 32767.0f;
								pz /= 32767.0f;

							}
							break;
							
							case 3:
							{
								vertexAddres = (vertexAddres + 3) & ~3;

								memcpy(&px,(unsigned char *)vertexAddres,4);
								vertexAddres+=4;
								memcpy(&py,(unsigned char *)vertexAddres,4);
								vertexAddres+=4;
								memcpy(&pz,(unsigned char *)vertexAddres,4);
								vertexAddres+=4;
							}
							break;
						}
						
						//skinning
						if (iweight != 0)
						{
							//temp values
							float sx = 0, sy = 0, sz = 0;
							
							int weightNumber = 0;
							for(weightNumber = 0; weightNumber < iskinningWeightCount; weightNumber++)
							{
								if (boneWeights[weightNumber] != 0.0f)
								{
									sx += (	px * bone_uploaded_matrix[weightNumber][0]
									+ 		py * bone_uploaded_matrix[weightNumber][3]
									+ 		pz * bone_uploaded_matrix[weightNumber][6]
									+            bone_uploaded_matrix[weightNumber][9]) * boneWeights[weightNumber];

									sy += (	px * bone_uploaded_matrix[weightNumber][1]
									+ 		py * bone_uploaded_matrix[weightNumber][4]
									+ 		pz * bone_uploaded_matrix[weightNumber][7]
									+            bone_uploaded_matrix[weightNumber][10]) * boneWeights[weightNumber];

									sz += (	px * bone_uploaded_matrix[weightNumber][2]
									+ 		py * bone_uploaded_matrix[weightNumber][5]
									+ 		pz * bone_uploaded_matrix[weightNumber][8]
									+            bone_uploaded_matrix[weightNumber][11]) * boneWeights[weightNumber];
								}
							}
							
							//swap temp variables
							px = sx;
							py = sy;
							pz = sz;							
						}
						
					}

					if(file > 0)
					{
						//save vertex to file
						TexVertex vertex;
						vertex.u = tu;
						vertex.v = tv;
						vertex.x = px;
						vertex.y = py;
						vertex.z = pz;
						
						sceIoWrite(file, &vertex, sizeof(TexVertex));
					}
				}
			
			}
			
			// VADDR is updated after vertex rendering.
			// Some games rely on this and don't reload VADDR between 2 PRIM calls.
			//vertex_adr = vinfo.getAddress(mem, numberOfVertex);
			int indexcount = numberOfVertex;
			if(indexPntr != 0 && iindex != 0)
			{
				unsigned int addr = indexPntr + indexcount * iindex;

				switch(iindex)
				{
				case 1: 
					{
						unsigned char uc8 = 0;
						memcpy(&uc8,(unsigned char *)addr,1);
						indexcount = (unsigned int)uc8;
					}
					break;
				case 2:
					{
						addr = (addr + 1) & ~1;
						short sh16 = 0;
						memcpy(&sh16,(unsigned char *)addr,2);
						indexcount = (unsigned int)sh16;
					}
					break;
				case 4:
					{
						addr = (addr + 3) & ~3;
						unsigned int in32 = 0;
						memcpy(&in32,(unsigned char *)addr,4);
						indexcount = in32;
					}
					break;
				}
				
				indexPntr += iindex * numberOfVertex;

			}else
			{
				vertexBase = vertexBase + indexcount * vertexSize;
			}
			
			//debugCommand(command,argument);
		}
		break;
		
		case 0x09: //BJUMP
		{
			//nothing right now...
			//debugCommand(command,argument);
		}
		break;
		
		case 0x08: //JUMP
		{
			unsigned int jump = (( base | argument) + baseOffset) & 0xFFFFFFFC;
			currentDList = (unsigned int*)jump;
			
			//char text[100];
			//sprintf(text,"JUMP: %x\n",jump);
			//debuglog(text);
		}
		break;
		
		case 0x10: //BASE
		{
			base = (argument << 8) & 0xff000000;
			
			//char text[100];
			//sprintf(text,"BASE: %x\n",base);
			//debuglog(text);
		}
		break;
		
		case 0x12: //VERTEX TYPE
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
			
			
			
			vertexSize = 0;
			vertexSize += size_mapping[iweight] * iskinningWeightCount;
			vertexSize = (vertexSize + size_padding[itexture]) & ~size_padding[itexture];

			textureOffset = vertexSize;
			vertexSize += size_mapping[itexture] * 2;
			vertexSize = (vertexSize + color_size_padding[icolor]) & ~color_size_padding[icolor];

			colorOffset = vertexSize;
			vertexSize += color_size_mapping[icolor];
			vertexSize = (vertexSize + size_padding[inormal]) & ~size_padding[inormal];

			normalOffset = vertexSize;
			vertexSize += size_mapping[inormal] * 3;
			vertexSize = (vertexSize + size_padding[iposition]) & ~size_padding[iposition];

			positionOffset = vertexSize;
			vertexSize += size_mapping[iposition] * 3;
			
			//morphing?
			oneSize = vertexSize;
			vertexSize *= imorphingVertexCount;
			//

			alignmentSize = max(size_mapping[iweight],
							max(color_size_mapping[icolor],
							max(size_mapping[inormal],
							max(size_mapping[itexture],
									 size_mapping[iposition]))));
			
			vertexSize = (vertexSize + alignmentSize - 1) & ~(alignmentSize - 1);
			oneSize = (oneSize + alignmentSize - 1) & ~(alignmentSize - 1);

			//char text[100];
			//sprintf(text,"t:%d c:%d n:%d p:%d w:%d i:%d sk:%d mo:%d tr:%d\n",itexture,icolor,inormal,iposition,iweight,iindex,iskinningWeightCount,imorphingVertexCount,itransform2D);
			//debuglog(text);
			
			//debugCommand(command,argument);
		}
		break;
		
		case 0x13: //OFF_ADR
		{
			baseOffset = argument << 8;
			
			//debugCommand(command,argument);
			
			//char text[100];
			//sprintf(text,"BASE off: %x\n",baseOffset);
			//debuglog(text);
		}
		break;
		
		case 0x14: //ORIGIN_ADR
		{
			baseOffset = currentDList - 1;
			
			//char text[100];
			//sprintf(text,"BASE off: %x\n",baseOffset);
			//debuglog(text);
		}
		break;
		
		case 0x0A: //CALL
		{
			unsigned int npc = (( base | argument) + baseOffset) & 0xFFFFFFFC;
			
			currentDList_stack[currentDList_stackIndex] = currentDList;
			currentDList_basestack[currentDList_stackIndex] = baseOffset;
			currentDList_stackIndex++;
			
			currentDList = (unsigned int*)npc;
		
			//char text[100];
			//sprintf(text,"CALL: %x\n",npc);
			//debuglog(text);
		}
		break;
		
		case 0x0B: //RET
		{
			currentDList_stackIndex--;
			
			currentDList = (unsigned int*)currentDList_stack[currentDList_stackIndex];
			baseOffset = currentDList_basestack[currentDList_stackIndex];
			
			//char text[100];
			//sprintf(text,"RET: %x\n",currentDList_stack[currentDList_stackIndex]);
			//debuglog(text);
		}
		break;

		
		case 0x0C: //END
		{
			currentDList_ended = 1;
			
			//char text[100];
			//sprintf(text,"End \n");
			//debuglog(text);

		}
		break;
		
		case 0x0F: //FINISH
		{
			currentDList_finished = 1;
			
			//char text[100];
			//sprintf(text,"Finish \n");
			//debuglog(text);
		}
		break;
		
		//bone matrix support
		case 0x2A:
		{
			boneMatrixIndex = argument;
		}
		break;

		case 0x2B:
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
		}
		break;
		
		
		//texture stuff
		//texture size
		case 0xB8:
		case 0xB7:
		case 0xB9:
		case 0xBA:
		case 0xBB:
		case 0xBC:
		case 0xBD:
		case 0xBF:
		{
			int level = command - 0xB8;
			
			int height_exp2 = min((argument >> 8) & 0x0F, 9);
            int width_exp2 = min((argument) & 0x0F, 9);
			
			texture_height[level] = 1 << height_exp2;
            texture_width[level] = 1 << width_exp2;
			
			//char text[100];
			//sprintf(text,"Tex level: %d size: %d x %d\n",level,texture_width[level],texture_height[level]);
			//debuglog(text);
		}
		break;
		
		//texture pixel format
		case 0xC3:
		{
			texture_storage_mode = argument & 0xF; // Lower four bits.
			
			//char text[100];
			//sprintf(text,"Tex format: %d \n",texture_storage_mode);
			//debuglog(text);
		}
		break;
		
		default:
		{
			//debugCommand(command,argument);
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
		sprintf(text,"addres1: %x addres2: %x\n",currentDList_addr,currentDList_stall_addr);
		debuglog(text);
		
		//open file
		char modelFilename[50];
		sprintf(modelFilename,"ms0:/models/model%d.3d",primitiveCounter);
		file = sceIoOpen(modelFilename, PSP_O_CREAT | PSP_O_WRONLY, 0777);
		
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
			}
			else
			if(currentDList >= currentDList_stall_addr)
			{
				listHasEnded = 1;
				can_parse = 0;
				break;
			}else
			{
				int command  = (*currentDList) >> 24;
				int argument = (*currentDList) & 0x00FFFFFF;
				
				currentDList++;
				
				ExecuteCommand(command,argument);
			}		
		}
		
		if(file >= 0)
		{
			sceIoClose(file);
			primitiveCounter++;
		}
		
		
		currentDList_finished = 0;
		currentDList_ended = 0;
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
