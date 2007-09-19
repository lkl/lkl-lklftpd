#include <ddk/ntddk.h>

#define UNICODE_STRING_INIT(name, x) UNICODE_STRING name = { \
	.Buffer = L##x, \
	.Length = sizeof(L##x), \
	.MaximumLength = sizeof(L##x) \
}

UNICODE_STRING* INIT_UNICODE(UNICODE_STRING *ustr, const char *str) 
{
        ANSI_STRING astr;

        RtlInitAnsiString(&astr, str);
        if (RtlAnsiStringToUnicodeString(ustr, &astr, TRUE) != STATUS_SUCCESS) 
                return NULL;

        return ustr;
}

#define KeBugOn(x) if (x) { DbgPrint("bug %s:%d\n", __FUNCTION__, __LINE__); while (1); }

void* _file_open(void)
{
	HANDLE f;
	IO_STATUS_BLOCK isb;
	OBJECT_ATTRIBUTES obj_attr;
	UNICODE_STRING ustr;

        INIT_UNICODE (&ustr, "\\DosDevices\\C:\\disk");

        InitializeObjectAttributes(&obj_attr, &ustr, OBJ_CASE_INSENSITIVE,0,0);
        

        ZwOpenFile(&f, GENERIC_READ| GENERIC_WRITE, &obj_attr,
		   &isb, FILE_SHARE_READ| FILE_SHARE_WRITE, 0);

	KeBugOn(isb.Status != STATUS_SUCCESS);

	return f;
}

unsigned long _file_sectors(void)
{
	HANDLE f=_file_open();
	FILE_STANDARD_INFORMATION fsi;
	IO_STATUS_BLOCK isb;

	ZwQueryInformationFile(f, &isb, &fsi, sizeof(fsi), FileStandardInformation);

	KeBugOn(isb.Status != STATUS_SUCCESS);

	ZwClose(f);

        return fsi.EndOfFile.QuadPart/512;
}

void _file_rw(void *f, unsigned long sector, unsigned long nsect, char *buffer, int dir)
{
	IO_STATUS_BLOCK isb;
	LARGE_INTEGER offset = {
		.QuadPart = sector*512,
	};
	
	if (dir)
		ZwWriteFile(f, 0, 0, 0, &isb, buffer, nsect*512, &offset, 0);
	else
		ZwReadFile(f, 0, 0, 0, &isb, buffer, nsect*512, &offset, 0);		

	KeBugOn(isb.Status != STATUS_SUCCESS);
}

