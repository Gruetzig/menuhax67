#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <3ds.h>

#include "rop_usa_bin.h"
#include "rop_usa30_bin.h"
#include "rop_eur_bin.h"
#include "rop_jpn_bin.h"
#include "rop_kor_bin.h"

u8 *data; 
int iscfw=0;
char path[0x200]={0};

const char *yellow="\x1b[33;1m";
const char *blue="\x1b[34;1m";
const char *dblue="\x1b[34;0m";
const char *white="\x1b[37;1m";

Result menuhax67(){
	Result res=0;
	u32 base_addr=0;  //0x3093d0
	u8 region=0xff;
	bool isnew=*(u32*)0x1FF80030 == 6;
	FS_ProductInfo info;
	u64 menus[7]={  //index will correspond to region value
		0x0004003000008202LL, //JPN
		0x0004003000008F02LL, //USA
		0x0004003000009802LL, //EUR
		0x0004003000009802LL, //AUS same as EUR
		0,                    //CHN
		0x000400300000A902LL, //KOR
		0                     //TWN
		};
	u32 rop[0xC0/4]={0};
	u32 fail=1;
	u32 procid=0;
	u16 menuversion=0;
	
	res += CFGU_SecureInfoGetRegion(&region);
	res += NS_LaunchTitle(menus[region], 0, &procid);
	res += FSUSER_GetProductInfo(&info, procid);
	menuversion=info.remasterVersion;
	
	if(res){
		printf("Error: issue with version check\n");
		return 4;
	}
	
	if(region==0){
		base_addr=0x00347a10;
		memcpy(rop, rop_jpn_bin, rop_jpn_bin_size); 
		printf("JPN\n");
		if(menuversion != 31){
			printf("Error: unsupported menu r%d, expected r31\n", menuversion);
			return 3;
		}
	}
	else if(region==1){
		printf("USA r%d\n", menuversion);
		if(menuversion == 29){
			base_addr=0x00346a10;
			memcpy(rop, rop_usa_bin, rop_usa_bin_size); 
		}
		else if(menuversion == 30){
			base_addr=0x00347a10;
			memcpy(rop, rop_usa30_bin, rop_usa30_bin_size); 
		}
		else{
			printf("Error: unsupported menu r%d, expected r29/30\n", menuversion);
			return 3;
		}
	}
	else if(region==2 || region==3){
		base_addr=0x00347a10;
		memcpy(rop, rop_eur_bin, rop_eur_bin_size); 
		printf("EUR\n");
		if(menuversion != 29){
			printf("Error: unsupported menu r%d, expected r29\n", menuversion);
			return 3;
		}
	}
	else if(region==5){
		base_addr=0x00346a10;
		memcpy(rop, rop_kor_bin, rop_kor_bin_size); 
		printf("KOR\n");
		if(menuversion != 15){
			printf("Error: unsupported menu r%d, expected r15\n", menuversion);
			return 3;
		}
	}
	else{
		printf("Error: region not supported\n");    
		return 1;
	}
	
	base_addr+=0xA8;
	
	if(isnew){
		printf("NEW3DS\n");
		for(int i=0;i<0xC0/4;i++){
			if(rop[i]==0x35040000){ //find the old3ds linearmem address and patch it to new3ds address
				rop[i]=0x38C40000;
				fail=0;
			}
		}
		if(fail==1){
			printf("Error: new3ds address location not found\n");
			return 2;
		}
	}
	else{
		printf("OLD3DS\n");
		fail=0;
	}
	
	res = CFG_GetConfigInfoBlk4(2, 0x50001, data);
	data[1]=0xE;
	res = CFG_SetConfigInfoBlk4(2, 0x50001, data);
	
	res = CFG_GetConfigInfoBlk4(8, 0x50009, data);
	*(u32*)(data+4)=base_addr;
	res = CFG_SetConfigInfoBlk4(8, 0x50009, data);
	
	res = CFG_SetConfigInfoBlk4(0xc0, 0xc0000, rop);//36857a10 08557a10
	
	res = CFG_UpdateConfigSavegame();  //note that this is the cfg:i version of this function, so it won't work with anything but mset
	printf("done %08X\n", (int)res);   //easy workaround is to patch header 00 00 03 08 --> 00 00 03 04 in the binary (first occurrence 2f50)
	                                   //not really in the mood to make a local libctru or bother the libctru maintainers
	return 0;
}

Result uninstall(){
	Result res;
	Result res2;

	res = CFG_GetConfigInfoBlk4(2, 0x50001, data);
	data[1]=0x3;            //reverts brightness level to 3 (range 1-5)
	res = CFG_SetConfigInfoBlk4(2, 0x50001, data);
	
	res = CFG_GetConfigInfoBlk4(8, 0x50009, data);
	*(u32*)(data+4)=0x00000101;  //don't know what this is, but 0x101 is the value that's usually there
	res = CFG_SetConfigInfoBlk4(8, 0x50009, data);

	memset(data, 0, 0xC0);  //reverts parental controls to a blank state
	data[0x9]=0x14;
	
	res = CFG_SetConfigInfoBlk4(0xc0, 0xc0000, data);
	
	res = CFG_UpdateConfigSavegame();
	
	if(iscfw){
		strcat(path, "/Nintendo DSiWare/F00D43D5.bin");
		res2 = remove(path);
		if(!res2) printf("F00D43D5.bin removed\n");
	}
	
	printf("done %08X\n", (int)res);
	
	return 0;
}

int cursor=0;
int menu(u32 n){
	consoleClear();
	
	printf("menuhax67 installer v1.2 - zoogie\nSTATUS: %s\n\n", iscfw ? "cfw":"user");
	printf("CAUTION: This will crank your brightness up to\nfull and erase parental controls\n\n");

	char *choices[]={
		"INSTALL menuhax67",
		"REMOVE  menuhax67",
		"EXIT    to menu",
	};
	
	int maxchoices=sizeof(choices)/4; //each array element is a 32 bit pointer so numElements is sizeof/4 (this is a bad practice but whatever).
	
	if(n & KEY_UP) cursor--;
	else if (n & KEY_DOWN) cursor++;
	if (cursor >= maxchoices) cursor=0;
	else if (cursor < 0) cursor=maxchoices-1;
	
	
	for(int i=0; i<maxchoices; i++){
		printf("%s%s%s\n", cursor==i ? yellow:white, choices[i], white);
	}
	
	printf("--------------------------------------------------");
	printf(" \n");
	
	if(n & KEY_A) {
		
		switch(cursor){
			case 0:
			menuhax67();
			break;
			
			case 1:
			uninstall(); 
			break;
			
			case 2:
			return 1;
			break;
			
			default:;
		};
		svcSleepThread(500*1000*1000);
	}
	
	return 0;
}

int main(int argc, char* argv[])
{
	Result res;
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);
	u8 ctrpath[0x200]={0};
	memset(ctrpath, 0, 0x200); //make damned sure this is safe
	memset(path, 0, 0x200); 
	
	data=(u8*)malloc(0x10000);

	cfguInit();
	nsInit();
	fsInit();
	
	res = CFG_GetConfigInfoBlk4(2, 0, data);
	if(res){
		printf("Error: cfg:s or cfg:i not available, abort\n");
		while(1) svcSleepThread(17*1000*1000);
	}
	res = FSUSER_ExportIntegrityVerificationSeed((FS_IntegrityVerificationSeed*)(data+0x8000)); //data outputed is don't care, we're just testing for cfw. hax* userland would never be allowed to call this.
	if(!res){
		iscfw=1;
		res = FSUSER_GetSdmcCtrRootPath(ctrpath, 0x80*2);
		for(int i=0;i<0x180;i++){
			path[i]=ctrpath[i*2];
			if(!path[i]) break;
		}
	}
	//printf("%08X\n",(int)res);
	//printf("%s\n",(char*)path);
	//printf("\n");
	
	menu(0);

	// Main loop
	while (aptMainLoop())
	{
		gspWaitForVBlank();
		gfxSwapBuffers();
		hidScanInput();

		u32 kDown = hidKeysDown();
		
		if(kDown & 0xfff){
			//if(fail) break;
			res = menu(kDown);
			if(res) break;
		}
	}

	gfxExit();
	return 0;
}
