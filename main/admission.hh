#ifndef _ADMISSION_HH
#define _ADMISSION_HH

#include "utility.hh"
#include "sales.hh"
#include <external/sha1.hh>

void admission_itemname_hash(Str& ih,const Str& name,const Str& location,const Str& time, const Str& price_class); //converts a name to the item~hash form.
void admission_parse_hash_name(Str& name,const Str& ih);
void admission_parse_hash_ltime_hash(Str& hout,const Str& ih);
const char* admission_filteredname(const Str& item_name);

#endif