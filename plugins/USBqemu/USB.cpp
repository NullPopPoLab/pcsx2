/*  USBqemu
 *  Copyright (C) 2002-2011  PCSX2 Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "qemu-usb/USBinternal.h"

#ifdef _MSC_VER
#	include "svnrev.h"
#endif

OHCIState *qemu_ohci;

Config conf;

HWND gsWindowHandle=NULL;

u8 *ram;
USBcallback _USBirq;
int64_t usb_frame_time=0;
int64_t usb_bit_time=0;

s64 clocks=0;
s64 remaining=0;

int64_t get_ticks_per_sec()
{
	return PSXCLK;
}

void USBirq(int cycles)
{
	USB_LOG("USBirq.\n");

	_USBirq(cycles);
}

s32 CALLBACK USBinit() {
	qemu_ohci = ohci_create(0x1f801600,2);
	qemu_ohci->rhport[0].port.dev = usb_keyboard_init();
	qemu_ohci->rhport[0].port.ops->attach(&(qemu_ohci->rhport[0].port));

	clocks = 0;
	remaining = 0;

	return 0;
}

void CALLBACK USBshutdown() {

	USBDevice* device = qemu_ohci->rhport[0].port.dev;
	
	qemu_ohci->rhport[0].port.ops->detach(&(qemu_ohci->rhport[0].port));
	device->info->handle_destroy(qemu_ohci->rhport[0].port.dev);

	free(qemu_ohci);

}

s32 CALLBACK USBopen() {
	USB_LOG("USBopen\n");

	return 0;
}

void CALLBACK USBclose() {
}

u8   CALLBACK USBread8(u32 addr) {
	USB_LOG("* Invalid 8bit read at address %lx\n", addr);
	return 0;
}

u16  CALLBACK USBread16(u32 addr) {
	USB_LOG("* Invalid 16bit read at address %lx\n", addr);
	return 0;
}

u32  CALLBACK USBread32(u32 addr) {
	u32 hard;

	hard=ohci_mem_read(qemu_ohci,addr - qemu_ohci->mem);

	USB_LOG("* Known 32bit read at address %lx: %lx\n", addr, hard);

	return hard;
}

void CALLBACK USBwrite8(u32 addr,  u8 value) {
	USB_LOG("* Invalid 8bit write at address %lx value %x\n", addr, value);
}

void CALLBACK USBwrite16(u32 addr, u16 value) {
	USB_LOG("* Invalid 16bit write at address %lx value %x\n", addr, value);
}

void CALLBACK USBwrite32(u32 addr, u32 value) {
	USB_LOG("* Known 32bit write at address %lx value %lx\n", addr, value);
	ohci_mem_write(qemu_ohci,addr - qemu_ohci->mem, value);
}

void CALLBACK USBirqCallback(USBcallback callback) {
	_USBirq = callback;
}

extern u32 bits;

int CALLBACK _USBirqHandler(void) 
{
	//fprintf(stderr," * USB: IRQ Acknowledged.\n");
	//qemu_ohci->intr_status&=~bits;
	return 1;
}

USBhandler CALLBACK USBirqHandler(void) {
	return (USBhandler)_USBirqHandler;
}

void CALLBACK USBsetRAM(void *mem) {
	ram = (u8*)mem;
}

// extended funcs

char USBfreezeID[] = "USBqemu01";

typedef struct {
	char freezeID[10];
	int cycles;
	int remaining;
	OHCIState t;
	int extraData; // for future expansion with the device state
} USBfreezeData;

s32 CALLBACK USBfreeze(int mode, freezeData *data) {
	USBfreezeData usbd;

	if (mode == FREEZE_LOAD) 
	{
		if(data->size < sizeof(USBfreezeData))
			return -1;

		usbd = *(USBfreezeData*)data->data;
		usbd.freezeID[9] = 0;
		usbd.cycles = static_cast<int>(clocks);
		usbd.remaining = static_cast<int>(remaining);
		
		if( strcmp(usbd.freezeID, USBfreezeID) != 0)
			return -1;

		if (data->size != sizeof(USBfreezeData))
			return -1;
		
		for(int i=0; i< qemu_ohci->num_ports; i++)
		{
			usbd.t.rhport[i].port.opaque = qemu_ohci;
			usbd.t.rhport[i].port.ops = qemu_ohci->rhport[i].port.ops;
			usbd.t.rhport[i].port.dev = qemu_ohci->rhport[i].port.dev; // pointers
		}
		*qemu_ohci = usbd.t;

		// WARNING: TODO: Load the state of the attached devices!

	}
	else if (mode == FREEZE_SAVE) 
	{
		data->size = sizeof(USBfreezeData);
		data->data = (s8*)malloc(data->size);
		if (data->data == NULL)
			return -1;
		

		strcpy(usbd.freezeID, USBfreezeID);
		usbd.t = *qemu_ohci;
		for(int i=0; i< qemu_ohci->num_ports; i++)
		{
			usbd.t.rhport[i].port.ops = NULL; // pointers
			usbd.t.rhport[i].port.opaque = NULL; // pointers
			usbd.t.rhport[i].port.dev = NULL; // pointers
		}

		clocks = usbd.cycles;
		remaining = usbd.remaining;

		// WARNING: TODO: Save the state of the attached devices!
	}

	return 0;
}

void CALLBACK USBasync(u32 _cycles)
{
	remaining += _cycles;
	clocks += remaining;
	if(qemu_ohci->eof_timer>0)
	{
		while(remaining>=qemu_ohci->eof_timer)
		{
			remaining-=qemu_ohci->eof_timer;
			qemu_ohci->eof_timer=0;
			ohci_frame_boundary(qemu_ohci);
		}
		if((remaining>0)&&(qemu_ohci->eof_timer>0))
		{
			s64 m = qemu_ohci->eof_timer;
			if(remaining < m)
				m = remaining;
			qemu_ohci->eof_timer -= m;
			remaining -= m;
		}
	}
	//if(qemu_ohci->eof_timer <= 0)
	//{
	//	ohci_frame_boundary(qemu_ohci);
	//}
}

void cpu_physical_memory_rw(u32 addr, u8 *buf,
                            int len, int is_write)
{
	if(is_write)
		memcpy(&(ram[addr]),buf,len);
	else
		memcpy(buf,&(ram[addr]),len);
}

s64 get_clock()
{
	return clocks;
}

void *qemu_mallocz(uint32_t size)
{
	void *m=malloc(size);
	if(!m) return NULL;
	memset(m,0,size);
	return m;
}
