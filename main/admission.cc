#include "admission.hh"
#include <external/sha1.hh>
#include <string.h>
#include <stdlib.h>


void admission_itemname_hash(Str& ih,const Str& name,const Str& location,const Str& time, const Str& price_class)
{
	genericChar outbuf[256];
	SHA1Context sha1ctxt;
	SHA1Reset(&sha1ctxt);
	SHA1Input(&sha1ctxt,(const uint8_t*)location.Value(),location.length);
	SHA1Input(&sha1ctxt,(const uint8_t*)time.Value(),time.length);
	
	uint8_t hashbytes[SHA1HashSize];
	SHA1Result(&sha1ctxt,hashbytes);
	uint32_t result=0;
	for(int i=0;i<4;i++)
	{
		result <<= 8;
		result |= hashbytes[i];
	}
	
	snprintf(outbuf,256,"%s~@%08X:%s",name.Value(),result,price_class.Value());
	ih.Set(outbuf);
}//converts a name to the item~hash form.

void admission_parse_hash_name(Str& name,const Str& ih)
{
	static genericChar buffer[256];
	strcpy(buffer,ih.Value());
	char* zloc=strstr(buffer,"~@");
	if(zloc)
	{
		*zloc=0;
	}
	name.Set(buffer);
}
void admission_parse_hash_ltime_hash(Str& hashout,const Str& ih)
{
	static genericChar buffer[256];
	strcpy(buffer,ih.Value());
	char* zloc=strstr(buffer,"~@");
	uint32_t val=0;
	if(zloc)
	{
		val=strtoul(zloc+2,NULL,16);
	}
	if(val!=0)
	{
		snprintf(buffer,256,"%08X",val);
		hashout.Set(buffer);
	}
	else
	{
		hashout.Set("");
	}
}
const char* admission_filteredname(const Str& item_name)
{
	static genericChar buf[256];
	Str outname;
	admission_parse_hash_name(outname,item_name);
	snprintf(buf,256,"%s",outname.Value());
	return buf;
}
/*
void PrintItemAdmissionFiltered(char* buf,int qual,const char* item)
{
	static genericChar buffer[256];
	strcpy(buffer,item);
	char* zloc=strstr(buffer,"~@");
	if(zloc)
	{
		*zloc='\0'; //insert terminating null character to hide hash on print.
	}
	PrintItem(buf,qual,buffer);
}*/
