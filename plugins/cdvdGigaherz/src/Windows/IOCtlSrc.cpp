/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014 David Quintana [gigaherz]
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../CDVD.h"

#include <winioctl.h>
#include <ntddcdvd.h>
#include <ntddcdrm.h>
#include <cstddef>
#include <cstdlib>

template<class T>
bool ApiErrorCheck(T t,T okValue,bool cmpEq)
{
	bool cond=(t==okValue);

	if(!cmpEq) cond=!cond;

	if(!cond)
	{
		static char buff[1024];
		DWORD ec = GetLastError();
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,NULL,ec,0,buff,1023,NULL);
		MessageBoxEx(0,buff,"ERROR?!",MB_OK,0);
	}
	return cond;
}

#if FALSE
typedef struct {
  SCSI_PASS_THROUGH_DIRECT spt;
  ULONG Filler;
  UCHAR ucSenseBuf[32];
} SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, *PSCSI_PASS_THROUGH_DIRECT_WITH_BUFFER;

SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER sptIOCTL;         // global read bufs

char szBuf[1024];

PSCSI_ADDRESS pSA;

DWORD SendIoCtlScsiCommand(HANDLE hDevice, u8 direction,
						   u8  cdbLength,  u8*   cdb,
						   u32 dataLength, void* dataBuffer
						   )
{
	DWORD dwRet;

	memset(szBuf,0,1024);

	pSA=(PSCSI_ADDRESS)szBuf;
	pSA->Length=sizeof(SCSI_ADDRESS);

	if(!DeviceIoControl(hDevice,IOCTL_SCSI_GET_ADDRESS,NULL,
						0,pSA,sizeof(SCSI_ADDRESS),
						&dwRet,NULL))
	{
		return -1;
	}

	memset(&sptIOCTL,0,sizeof(sptIOCTL));

	sptIOCTL.spt.Length             = sizeof(SCSI_PASS_THROUGH_DIRECT);
	sptIOCTL.spt.TimeOutValue       = 60;
	sptIOCTL.spt.SenseInfoLength    = 14;
	sptIOCTL.spt.SenseInfoOffset    = offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);

	sptIOCTL.spt.PathId				= pSA->PortNumber;
	sptIOCTL.spt.TargetId           = pSA->TargetId;
	sptIOCTL.spt.Lun                = pSA->Lun;

	sptIOCTL.spt.DataIn             = (direction>0)?SCSI_IOCTL_DATA_IN:((direction<0)?SCSI_IOCTL_DATA_OUT:SCSI_IOCTL_DATA_UNSPECIFIED);
	sptIOCTL.spt.DataTransferLength = dataLength;
	sptIOCTL.spt.DataBuffer         = dataBuffer;

	sptIOCTL.spt.CdbLength          = cdbLength;
	memcpy(sptIOCTL.spt.Cdb,cdb,cdbLength);

	DWORD code = DeviceIoControl(hDevice,
					IOCTL_SCSI_PASS_THROUGH_DIRECT,
					&sptIOCTL,sizeof(sptIOCTL),
					&sptIOCTL,sizeof(sptIOCTL),
					&dwRet,NULL);
	ApiErrorCheck<DWORD>(code,0,false);
	return code;
}

//0x00 = PHYSICAL STRUCTURE
DWORD ScsiReadStructure(HANDLE hDevice, u32 layer, u32 format, u32 buffer_length, void* buffer)
{
	u8 cdb[12]={0};


	cdb[0]     = 0xAD;
	/*
	cdb[2]     = (unsigned char)((addr >> 24) & 0xFF);
	cdb[3]     = (unsigned char)((addr >> 16) & 0xFF);
	cdb[4]     = (unsigned char)((addr >> 8) & 0xFF);
	cdb[5]     = (unsigned char)(addr & 0xFF);
	*/
	cdb[6]     = layer;
	cdb[7]     = format;
	cdb[8]     = (unsigned char)((buffer_length>>8) & 0xFF);
	cdb[9]     = (unsigned char)((buffer_length) & 0xFF);

	return SendIoCtlScsiCommand(hDevice, 1, 12, cdb, buffer_length, buffer);
}


DWORD ScsiReadBE_2(HANDLE hDevice, u32 addr, u32 count, u8* buffer)
{
	u8 cdb[12]={0};

	cdb[0]     = 0xBE;
	cdb[2]     = (unsigned char)((addr >> 24) & 0xFF);
	cdb[3]     = (unsigned char)((addr >> 16) & 0xFF);
	cdb[4]     = (unsigned char)((addr >> 8) & 0xFF);
	cdb[5]     = (unsigned char)(addr & 0xFF);
	cdb[8]     = count;
	cdb[9]     = 0xF8;
	cdb[10]    = 0x2;

	return SendIoCtlScsiCommand(hDevice, 1, 12, cdb, 2352, buffer);
}

DWORD ScsiRead10(HANDLE hDevice, u32 addr, u32 count, u8* buffer)
{
	u8 cdb[10]={0};

	cdb[0]     = 0x28;
	//cdb[1]     = lun<<5;
	cdb[2]     = (unsigned char)((addr >> 24) & 0xFF);
	cdb[3]     = (unsigned char)((addr >> 16) & 0xFF);
	cdb[4]     = (unsigned char)((addr >> 8) & 0xFF);
	cdb[5]     = (unsigned char)(addr & 0xFF);
	cdb[8]     = count;

	return SendIoCtlScsiCommand(hDevice, 1, 10, cdb, 2352, buffer);
}

DWORD ScsiReadTOC(HANDLE hDevice, u32 addr, u8* buffer, bool use_msf)
{
	u8 cdb[12]={0};

	cdb[0]     = 0x43;
	cdb[7]     = 0x03;
	cdb[8]     = 0x24;

	if(use_msf) cdb[1]=2;

	return SendIoCtlScsiCommand(hDevice, 1, 10, cdb, 2352, buffer);
}

s32 IOCtlSrc::GetSectorCount()
{
	DWORD size;

	LARGE_INTEGER li;
	int plain_sectors = 0;

	if(GetFileSizeEx(device,&li))
	{
		return li.QuadPart / 2048;
	}

	GET_LENGTH_INFORMATION info;
	if(DeviceIoControl(device, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &info, sizeof(info), &size, NULL))
	{
		return info.Length.QuadPart / 2048;
	}

	CDROM_READ_TOC_EX tocrq={0};

	tocrq.Format = CDROM_READ_TOC_EX_FORMAT_FULL_TOC;
	tocrq.Msf=1;
	tocrq.SessionTrack=1;

	CDROM_TOC_FULL_TOC_DATA *ftd=(CDROM_TOC_FULL_TOC_DATA*)sectorbuffer;

	if(DeviceIoControl(device,IOCTL_CDROM_READ_TOC_EX,&tocrq,sizeof(tocrq),ftd, 2048, &size, NULL))
	{
		for(int i=0;i<101;i++)
		{
			if(ftd->Descriptors[i].Point==0xa2)
			{
				if(ftd->Descriptors[i].SessionNumber==ftd->LastCompleteSession)
				{
					int min=ftd->Descriptors[i].Msf[0];
					int sec=ftd->Descriptors[i].Msf[1];
					int frm=ftd->Descriptors[i].Msf[2];

					return MSF_TO_LBA(min,sec,frm);
				}
			}
		}
	}

	int sectors1=-1;

	if(ScsiReadStructure(device,0,DvdPhysicalDescriptor, sizeof(dld), &dld)!=0)
	{
		if(dld.ld.EndLayerZeroSector>0) // OTP?
		{
			sectors1 = dld.ld.EndLayerZeroSector - dld.ld.StartingDataSector;
		}
		else //PTP or single layer
		{
			sectors1 = dld.ld.EndDataSector - dld.ld.StartingDataSector;
		}

		if(ScsiReadStructure(device,1,DvdPhysicalDescriptor, sizeof(dld), &dld)!=0)
		{
			// PTP second layer
			//sectors1 += dld.ld.EndDataSector - dld.ld.StartingDataSector;
			if(dld.ld.EndLayerZeroSector>0) // OTP?
			{
				sectors1 += dld.ld.EndLayerZeroSector - dld.ld.StartingDataSector;
			}
			else //PTP
			{
				sectors1 += dld.ld.EndDataSector - dld.ld.StartingDataSector;
			}
		}

		return sectors1;
	}

	return -1;
}

s32 IOCtlSrc::GetLayerBreakAddress()
{
	DWORD size;
	if(ScsiReadStructure(device,0,DvdPhysicalDescriptor, sizeof(dld), &dld)!=0)
	{
		if(dld.ld.EndLayerZeroSector>0) // OTP?
		{
			return dld.ld.EndLayerZeroSector - dld.ld.StartingDataSector;
		}
		else //PTP or single layer
		{
			u32 s1 = dld.ld.EndDataSector - dld.ld.StartingDataSector;

			if(ScsiReadStructure(device,1,DvdPhysicalDescriptor, sizeof(dld), &dld)!=0)
			{
				//PTP
				return s1;
			}

			// single layer
			return 0;
		}
	}

	return -1;
}
#endif

#define RETURN(v) {OpenOK=v; return;}

s32 IOCtlSrc::Reopen()
{
	if(device!=INVALID_HANDLE_VALUE)
	{
		DWORD size;
		DeviceIoControl(device,IOCTL_DVD_END_SESSION,&sessID,sizeof(DVD_SESSION_ID),NULL,0,&size, NULL);
		CloseHandle(device);
	}

	DWORD share = FILE_SHARE_READ;
	DWORD flags = FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN;
	DWORD size;

	OpenOK = false;

	device = CreateFile(fName, GENERIC_READ|GENERIC_WRITE|FILE_READ_ATTRIBUTES, share, NULL, OPEN_EXISTING, flags, 0);
	if(device==INVALID_HANDLE_VALUE)
	{
		device = CreateFile(fName, GENERIC_READ|FILE_READ_ATTRIBUTES, share, NULL, OPEN_EXISTING, flags, 0);
		if(device==INVALID_HANDLE_VALUE)
			return -1;
	}
	// Dual layer DVDs cannot read from layer 1 without this ioctl
	DeviceIoControl(device, FSCTL_ALLOW_EXTENDED_DASD_IO, nullptr, 0, nullptr, 0, &size, nullptr);

	sessID=0;
	DeviceIoControl(device,IOCTL_DVD_START_SESSION,NULL,0,&sessID,sizeof(DVD_SESSION_ID), &size, NULL);

	tocCached = false;
	mediaTypeCached = false;
	discSizeCached = false;
	layerBreakCached = false;

	OpenOK=true;
	return 0;
}

IOCtlSrc::IOCtlSrc(const char* fileName)
{
	device=INVALID_HANDLE_VALUE;

	strcpy_s(fName,256,fileName);

	Reopen();
	SetSpindleSpeed(false);
}

IOCtlSrc::~IOCtlSrc()
{
	if(OpenOK)
	{
		SetSpindleSpeed( true );
		DWORD size;
		DeviceIoControl(device,IOCTL_DVD_END_SESSION,&sessID,sizeof(DVD_SESSION_ID),NULL,0,&size, NULL);

		CloseHandle(device);
	}
}

struct mycrap
{
	DWORD shit;
	DVD_LAYER_DESCRIPTOR ld;
	// The IOCTL_DVD_READ_STRUCTURE expects a size of at least 22 bytes when
	// reading the dvd physical layer descriptor
	// 4 bytes header
	// 17 bytes for the layer descriptor
	// 1 byte of the media specific data for no reason whatsoever...
	UCHAR fixup;
};

DVD_READ_STRUCTURE dvdrs;
mycrap dld;
DISK_GEOMETRY dg;
CDROM_READ_TOC_EX tocrq={0};

s32 IOCtlSrc::GetSectorCount()
{
	DWORD size;

	LARGE_INTEGER li;
	int plain_sectors = 0;

	if(discSizeCached)
		return discSize;

	if(GetFileSizeEx(device,&li))
	{
		discSizeCached = true;
		discSize = (s32)(li.QuadPart / 2048);
		return discSize;
	}

	GET_LENGTH_INFORMATION info;
	if(DeviceIoControl(device, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &info, sizeof(info), &size, NULL))
	{
		discSizeCached = true;
		discSize = (s32)(info.Length.QuadPart / 2048);
		return discSize;
	}

	memset(&tocrq,0,sizeof(CDROM_READ_TOC_EX));
	tocrq.Format = CDROM_READ_TOC_EX_FORMAT_FULL_TOC;
	tocrq.Msf=1;
	tocrq.SessionTrack=1;

	CDROM_TOC_FULL_TOC_DATA *ftd=(CDROM_TOC_FULL_TOC_DATA*)sectorbuffer;

	if(DeviceIoControl(device,IOCTL_CDROM_READ_TOC_EX,&tocrq,sizeof(tocrq),ftd, 2048, &size, NULL))
	{
		for(int i=0;i<101;i++)
		{
			if(ftd->Descriptors[i].Point==0xa2)
			{
				if(ftd->Descriptors[i].SessionNumber==ftd->LastCompleteSession)
				{
					int min=ftd->Descriptors[i].Msf[0];
					int sec=ftd->Descriptors[i].Msf[1];
					int frm=ftd->Descriptors[i].Msf[2];

					discSizeCached = true;
					discSize = (s32)MSF_TO_LBA(min,sec,frm);
					return discSize;
				}
			}
		}
	}

	dvdrs.BlockByteOffset.QuadPart=0;
	dvdrs.Format=DvdPhysicalDescriptor;
	dvdrs.SessionId=sessID;
	dvdrs.LayerNumber=0;
	if(DeviceIoControl(device,IOCTL_DVD_READ_STRUCTURE,&dvdrs,sizeof(dvdrs),&dld, sizeof(dld), &size, NULL)!=0)
	{
		s32 sectors1 = _byteswap_ulong(dld.ld.EndDataSector) - _byteswap_ulong(dld.ld.StartingDataSector) + 1;
		if (dld.ld.NumberOfLayers == 1) // PTP, OTP
		{
			if (dld.ld.TrackPath == 0) // PTP
			{
				dvdrs.LayerNumber = 1;
				if (DeviceIoControl(device, IOCTL_DVD_READ_STRUCTURE, &dvdrs, sizeof(dvdrs), &dld, sizeof(dld), &size, nullptr) != 0)
				{
					sectors1 += _byteswap_ulong(dld.ld.EndDataSector) - _byteswap_ulong(dld.ld.StartingDataSector) + 1;
				}
			}
			else // OTP
			{
				// sectors = end_sector - (~end_sector_l0 & 0xFFFFFF) + end_sector_l0 - start_sector
				dld.ld.EndLayerZeroSector = _byteswap_ulong(dld.ld.EndLayerZeroSector);
				sectors1 += dld.ld.EndLayerZeroSector - (~dld.ld.EndLayerZeroSector & 0x00FFFFFF) + 1;
			}
		}

		discSizeCached = true;
		discSize = sectors1;
		return discSize;
	}

	return -1;
}

s32 IOCtlSrc::GetLayerBreakAddress()
{
	DWORD size;
	DWORD code;

	if(GetMediaType()<0)
		return -1;

	if(layerBreakCached)
		return layerBreak;

	dvdrs.BlockByteOffset.QuadPart=0;
	dvdrs.Format=DvdPhysicalDescriptor;
	dvdrs.SessionId=sessID;
	dvdrs.LayerNumber=0;
	if(code=DeviceIoControl(device,IOCTL_DVD_READ_STRUCTURE,&dvdrs,sizeof(dvdrs),&dld, sizeof(dld), &size, NULL)!=0)
	{
		if (dld.ld.NumberOfLayers == 0) // Single layer
		{
			layerBreak = 0;
		}
		else if (dld.ld.TrackPath == 0) // PTP
		{
			layerBreak = _byteswap_ulong(dld.ld.EndDataSector) - _byteswap_ulong(dld.ld.StartingDataSector);
		}
		else // OTP
		{
			layerBreak = _byteswap_ulong(dld.ld.EndLayerZeroSector) - _byteswap_ulong(dld.ld.StartingDataSector);
		}

		layerBreakCached = true;
		return layerBreak;
	}

	//if not a cd, and fails, assume single layer
	return 0;
}

void IOCtlSrc::SetSpindleSpeed(bool restore_defaults) { 
	
	DWORD dontcare;
	int speed = 0;
	
	if (GetMediaType() < 0 ) speed = 4800; // CD-ROM to ~32x (PS2 has 24x (3600 KB/s))
	else speed = 11080;	// DVD-ROM to  ~8x (PS2 has 4x (5540 KB/s))
	
	if (!restore_defaults) {
		CDROM_SET_SPEED s;
		s.RequestType = CdromSetSpeed;
		s.RotationControl = CdromDefaultRotation;
		s.ReadSpeed = speed;
		s.WriteSpeed = speed;

		if (DeviceIoControl(device,
			IOCTL_CDROM_SET_SPEED,	//operation to perform
			&s, sizeof(s),	//no input buffer
			NULL, 0,	//output buffer
			&dontcare,	//#bytes returned
			(LPOVERLAPPED)NULL))	//synchronous I/O == 0)
		{
			printf(" * CDVD: setSpindleSpeed success (%dKB/s)\n", speed);
		}
		else
		{
			printf(" * CDVD: setSpindleSpeed failed! \n");
		}
	}
	else {
		CDROM_SET_SPEED s;
		s.RequestType = CdromSetSpeed;
		s.RotationControl = CdromDefaultRotation;
		s.ReadSpeed = 0xffff; // maximum ?
		s.WriteSpeed = 0xffff;

		DeviceIoControl(device,
			IOCTL_CDROM_SET_SPEED,	//operation to perform
			&s, sizeof(s),	//no input buffer
			NULL, 0,	//output buffer
			&dontcare,	//#bytes returned
			(LPOVERLAPPED)NULL);	//synchronous I/O == 0)
	}
}

s32 IOCtlSrc::GetMediaType()
{
	DWORD size;
	DWORD code;

	if(mediaTypeCached)
		return mediaType;

	memset(&tocrq,0,sizeof(CDROM_READ_TOC_EX));
	tocrq.Format = CDROM_READ_TOC_EX_FORMAT_FULL_TOC;
	tocrq.Msf=1;
	tocrq.SessionTrack=1;

	CDROM_TOC_FULL_TOC_DATA *ftd=(CDROM_TOC_FULL_TOC_DATA*)sectorbuffer;

	if(DeviceIoControl(device,IOCTL_CDROM_READ_TOC_EX,&tocrq,sizeof(tocrq),ftd, 2048, &size, NULL))
	{
		mediaTypeCached = true;
		mediaType = -1;
		return mediaType;
	}

	dvdrs.BlockByteOffset.QuadPart=0;
	dvdrs.Format=DvdPhysicalDescriptor;
	dvdrs.SessionId=sessID;
	dvdrs.LayerNumber=0;
	if(code=DeviceIoControl(device,IOCTL_DVD_READ_STRUCTURE,&dvdrs,sizeof(dvdrs),&dld, sizeof(dld), &size, NULL)!=0)
	{
		if (dld.ld.NumberOfLayers == 0) // Single layer
		{
			mediaType = 0;
		}
		else if (dld.ld.TrackPath == 0) // PTP
		{
			mediaType = 1;
		}
		else // OTP
		{
			mediaType = 2;
		}

		mediaTypeCached = true;
		return mediaType;
	}

	//if not a cd, and fails, assume single layer
	mediaTypeCached = true;
	mediaType = 0;
	return mediaType;
}

s32 IOCtlSrc::ReadTOC(char *toc,int msize)
{
	DWORD size=0;

	if(GetMediaType()>=0)
		return -1;

	if(!tocCached)
	{
		memset(&tocrq,0,sizeof(CDROM_READ_TOC_EX));
		tocrq.Format = CDROM_READ_TOC_EX_FORMAT_FULL_TOC;
		tocrq.Msf=1;
		tocrq.SessionTrack=1;

		CDROM_TOC_FULL_TOC_DATA *ftd=(CDROM_TOC_FULL_TOC_DATA*)tocCacheData;

		if(!OpenOK) return -1;

		int code = DeviceIoControl(device,IOCTL_CDROM_READ_TOC_EX,&tocrq,sizeof(tocrq),tocCacheData, 2048, &size, NULL);

		if(code==0)
			return -1;

		tocCached = true;
	}

	memcpy(toc,tocCacheData,min(2048,msize));

	return 0;
}

s32 IOCtlSrc::ReadSectors2048(u32 sector, u32 count, char *buffer)
{
	RAW_READ_INFO rri;

	DWORD size=0;

	if(!OpenOK) return -1;

	rri.DiskOffset.QuadPart=sector*(u64)2048;
	rri.SectorCount=count;

	//fall back to standard reading
	if(SetFilePointer(device,rri.DiskOffset.LowPart,&rri.DiskOffset.HighPart,FILE_BEGIN)==-1)
	{
		if(GetLastError()!=0)
			return -1;
	}

	if(ReadFile(device,buffer,2048*count,&size,NULL)==0)
	{
		return -1;
	}

	if(size!=(2048*count))
	{
		return -1;
	}

	return 0;
}


s32 IOCtlSrc::ReadSectors2352(u32 sector, u32 count, char *buffer)
{
	RAW_READ_INFO rri;

	DWORD size=0;

	if(!OpenOK) return -1;

	rri.DiskOffset.QuadPart=sector*(u64)2048;
	rri.SectorCount=count;

	rri.TrackMode=(TRACK_MODE_TYPE)last_read_mode;
	if(DeviceIoControl(device,IOCTL_CDROM_RAW_READ,&rri,sizeof(rri),buffer, 2352*count, &size, NULL)==0)
	{
		rri.TrackMode = XAForm2;
		printf(" * CDVD: CD-ROM read mode change\n");
		printf(" * CDVD: Trying XAForm2\n");
		if(DeviceIoControl(device,IOCTL_CDROM_RAW_READ,&rri,sizeof(rri),buffer, 2352*count, &size, NULL)==0)
		{
			rri.TrackMode = YellowMode2;
			printf(" * CDVD: Trying YellowMode2\n");
			if(DeviceIoControl(device,IOCTL_CDROM_RAW_READ,&rri,sizeof(rri),buffer, 2352*count, &size, NULL)==0)
			{
				rri.TrackMode = CDDA;
				printf(" * CDVD: Trying CDDA\n");
				if(DeviceIoControl(device,IOCTL_CDROM_RAW_READ,&rri,sizeof(rri),buffer, 2352*count, &size, NULL)==0)
				{
					printf(" * CDVD: Failed to read this CD-ROM with error code: %d\n", GetLastError());
					return -1;
				}
			}
		}
	}

	last_read_mode=rri.TrackMode;

	if(size!=(2352*count))
	{
		return -1;
	}

	return 0;
}

s32 IOCtlSrc::DiscChanged()
{
	DWORD size=0;

	if(!OpenOK) return -1;

	int ret = DeviceIoControl(device,IOCTL_STORAGE_CHECK_VERIFY,NULL,0,NULL,0, &size, NULL);

	if(ret==0)
	{
		tocCached = false;
		mediaTypeCached = false;
		discSizeCached = false;
		layerBreakCached = false;

		if(sessID!=0)
		{
			DeviceIoControl(device,IOCTL_DVD_END_SESSION,&sessID,sizeof(DVD_SESSION_ID),NULL,0,&size, NULL);
			sessID=0;
		}
		return 1;
	}

	if(sessID==0)
	{
		DeviceIoControl(device,IOCTL_DVD_START_SESSION,NULL,0,&sessID,sizeof(DVD_SESSION_ID), &size, NULL);
	}

	return 0;
}

s32 IOCtlSrc::IsOK()
{
	return OpenOK;
}
