#include <windows.h>
#include "stdio.h"
#include "Fat32.h"
#include "vga.h"
#include "pe.h"

char    kernel_loader[256] = "\\boot\\osldr.exe";

#define KERNEL_LOADER_BASE	0x20000

//memory_map
#pragma pack(push,1)
struct  memory_map
{
	uint64 BaseAddr;
	uint64 Length;
	uint32 Type;
};
#pragma pack(pop)

#define MEMTYPE_RAM       1
#define MEMTYPE_RESERVED  2
#define MEMTYPE_ACPI      3
#define MEMTYPE_NVS       4

extern "C" void bios_print_string(char *str);
extern "C" int bios_get_drive_params(int drive, int *cyls, int *heads, int *sects);
extern "C" int bios_read_disk(int drive, int cyl, int head, int sect, int nsect, void *buffer);
extern "C" int vesa_get_info(struct vesa_info *info);
extern "C" int vesa_get_mode_info(int mode, struct vesa_mode_info *info);
extern "C" int vesa_set_mode(int mode);


uint32 load_os_loader()
{
	//������������
	void*   loader_buf = (void*)KERNEL_LOADER_BASE;
	uint32  loader_buf_size = 0x20000;

	MBR mbr;
	read_sectors(&mbr, 0, 1);
	printf("read MBR OK\n");

	uint32 volume0_start_sector = mbr.partition_table[0].first_sector;
	printf("Volume[0] start sector=%d\n", volume0_start_sector);


	FAT32 fat32;

	fat32.Init(mbr.bootdrv, volume0_start_sector);

	FILE_OBJECT file;
	if (!fat32.open_file(&file, kernel_loader))
	{
		printf("Open \\boot\\osldr.exe failed\n");
		__asm jmp $
	}
	printf("Open \\boot\\osldr.exe OK\n");
	printf("start_cluster=%X, size=%d\n", file.start_cluster, file.size);

	if (fat32.load_file(&file, loader_buf, loader_buf_size) != file.size)
	{
		printf("Load Osloader failed\n");
		__asm jmp $
	}
	printf("Load Osloader OK\n");

	return file.size;
}

struct  regs16_t
{
	unsigned short di, si, bp, sp, bx, dx, cx, ax;
	unsigned short gs, fs, es, ds, eflags;
};

extern "C" int callbios(unsigned char intnum, regs16_t *regs);

void main(memory_map* mem_map, int32 count)
{
	printf("\nBootLoader is starting...\n");
	printf("memory map=%08X count=%08X:\n", mem_map, count);
	regs16_t regs;

	//int drive, cyls, heads, sects;
	//bios_get_drive_params(0x80, &cyls, &heads, &sects);
	//printf("cyls=%d, heads=%d, sects=%d\n", cyls, heads, sects);
	//// wait for key
	//regs.ax = 0x0000;
	//callbios(0x16, &regs);

	uint64 memsize = 0;
	memory_map* pmap = mem_map;
	for (int i = 0; i < count; i++)
	{
		printf("%d %08X %08X %08X %d ", 
			i, 
			(uint32)pmap->BaseAddr, 
			(uint32)(pmap->BaseAddr + pmap->Length), 
			(uint32)pmap->Length, 
			pmap->Type);
		switch (pmap->Type)
		{
		case MEMTYPE_RAM: 
			printf("RAM\n"); 
			if (memsize < (uint64)(pmap->BaseAddr + pmap->Length))
			{
				memsize = (uint64)(pmap->BaseAddr + pmap->Length);
			}
			break;
		case MEMTYPE_RESERVED: printf("RESERVED\n"); break;
		case MEMTYPE_ACPI: printf("ACPI\n"); break;
		case MEMTYPE_NVS: printf("NVS\n"); break;
		default:	printf("\n"); break;
		}
		pmap++;
	}

	load_os_loader();
	
	typedef void (*osloader_main)(uint64 memsize);

	PE pe((void*)KERNEL_LOADER_BASE);
	osloader_main osldr_main = (osloader_main)pe.EntryPoint();
	osldr_main(memsize);

	__asm jmp $
}

