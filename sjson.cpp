/*
  Copyright (c) 2009 Dave Gamble
 
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
 
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
 
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

/* sJSON is based on cJSON written by Dave Gamble */

/* sJSON */
/* JSON parser in C. */

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include "sjson.h"

/* sjson: - no {} needed around the whole file
          - "=" is allowed instead of ":"
          - quotes around the key are optional
          - commas after values are optional */

static const char *ep;

const char *sJSONgetErrorPtr() {return ep;}

inline static int compareNames( const sJSON* c, eastl::FixedMurmurHash hash )
{
    if( c->nameHash == hash.mHash )
    {
       if( !c->nameString && !hash.mStr )
       {
          return 0;
       }
       if( !hash.mStr )
       {
          return 1;
       }
        return strcmp( c->nameString, hash.mStr );
    }
    return 1;
}

static void *(*sJSON_malloc)(size_t sz) = malloc;
static void (*sJSON_free)(void *ptr) = free;

static char* sJSON_strdup(const char* str) {
   size_t len;
   char* copy;

   len = strlen(str) + 1;
   if (!(copy = (char*)sJSON_malloc(len)))
      return 0;
   memcpy(copy,str,len);
   return copy;
}

void sJSONinitHooks(sJSON_Hooks* hooks) {
   if (!hooks) { /* Reset hooks */
     sJSON_malloc = malloc;
     sJSON_free = free;
     return;
   }

   sJSON_malloc = (hooks->malloc_fn)?hooks->malloc_fn:malloc;
   sJSON_free	 = (hooks->free_fn)?hooks->free_fn:free;
}

/* Internal constructor. */
static sJSON *sJSON_New_Item() {
	sJSON* node = (sJSON*)sJSON_malloc(sizeof(sJSON));
   if (node)
      memset(node,0,sizeof(sJSON));
	return node;
}

/* Delete a sJSON structure. */
void sJSONdelete(sJSON *c) {
	sJSON *next;
   while (c) {
		next=c->next;
      if (!(c->type&sJSON_IsReference) && c->child)
         sJSONdelete(c->child);
      if (!(c->type&sJSON_IsReference) && c->valueString)
         sJSON_free(c->valueString);
      if (c->nameString)
         sJSON_free(c->nameString);
		sJSON_free(c);
		c=next;
	}
}

/* Parse the input text to generate a number, and populate the result into item. */
static const char *parse_number(sJSON *item, const char *num) {
   double n=0,sign=1,scale=0;
   int subscale=0,signsubscale=1;

	/* Could use sscanf for this? */
	if (*num=='-') sign=-1,num++;	/* Has sign? */
	if (*num=='0') num++;			/* is zero */
	if (*num>='1' && *num<='9')	do	n=(n*10.0)+(*num++ -'0');	while (*num>='0' && *num<='9');	/* Number? */
	if (*num=='.') {num++;		do	n=(n*10.0)+(*num++ -'0'),scale--; while (*num>='0' && *num<='9');}	/* Fractional part? */
	if (*num=='e' || *num=='E')		/* Exponent? */
	{	num++;if (*num=='+') num++;	else if (*num=='-') signsubscale=-1,num++;		/* With sign? */
		while (*num>='0' && *num<='9') subscale=(subscale*10)+(*num++ - '0');	/* Number? */
	}

	n=sign*n*pow(10.0,(scale+subscale*signsubscale));	/* number = +/- number.fraction * 10^+/- exponent */
	
   item->valueDouble=n;
   item->valueInt=(int)n;
	item->type=sJSON_Number;
	return num;
}

/* Render the number nicely from the given item into a string. */
static char *print_number(sJSON *item) {
	char *str;
   double d=item->valueDouble;
   if (fabs(((double)item->valueInt)-d)<=DBL_EPSILON && d<=INT_MAX && d>=INT_MIN) {
		str=(char*)sJSON_malloc(21);	/* 2^64+1 can be represented in 21 chars. */
      if (str)
         sprintf(str,"%d",item->valueInt);
   } else {
		str=(char*)sJSON_malloc(64);	/* This is a nice tradeoff. */
      if (str) {
         if (fabs(floor(d)-d)<=DBL_EPSILON)
            sprintf(str,"%.0f",d);
         else if (fabs(d)<1.0e-6 || fabs(d)>1.0e9)
            sprintf(str,"%e",d);
         else
            sprintf(str,"%f",d);
		}
	}
	return str;
}

static const char *parse_string(sJSON *item,const char *str);

static const char *parse_string_or_identifier(sJSON *item,const char *str) {
   if(*str == '\"')
      return parse_string(item, str);

   //parse identifier
   char c = *str;
   if(c == '_' || (c >= 'a' && c <= 'z') || (c>='A' && c<='Z') ) {
      const char *ptr = str;
      int len = 0;
      while(*ptr && ( *ptr=='_' ||
                     (*ptr>='a' && *ptr<='z') ||
                     (*ptr>='A' && *ptr<='Z') ||
                     (*ptr>='0' && *ptr<='9')   )) {
         ptr++;
         len++;
      }
      char *out = (char*)sJSON_malloc(len+1);   /* This is how long we need for the string, roughly. */
      if(!out) return 0;

      ptr = str;
      char *ptr2 = out;
      while(*ptr && ( *ptr=='_' ||
                     (*ptr>='a' && *ptr<='z') ||
                     (*ptr>='A' && *ptr<='Z') ||
                     (*ptr>='0' && *ptr<='9')   )) {
         *(ptr2++) = *(ptr++);
      }
      *ptr2 = '\0';

      item->valueString=out;
      item->type=sJSON_String;
      return ptr;

   } else {
      ep = str;      /* not an identifier! */
      return 0;
   }
}

/* Parse the input text into an unescaped cstring, and populate item. */
static const unsigned char firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
static const char *parse_string(sJSON *item, const char *str) {
   const char *ptr=str+1;
   char *ptr2;
   char *out;
   int len=0;
   unsigned uc;
   if (*str!='\"') {
      ep=str;     /* not a string! */
      return 0;
   }
	
   while (*ptr!='\"' && *ptr && ++len)
      if (*ptr++ == '\\')
         ptr++;	/* Skip escaped quotes. */
	
	out=(char*)sJSON_malloc(len+1);	/* This is how long we need for the string, roughly. */
   if (!out)
      return 0;
	
   ptr=str+1;
   ptr2=out;
   while (*ptr!='\"' && *ptr) {
      if (*ptr!='\\')
         *ptr2++=*ptr++;
      else {
			ptr++;
         switch (*ptr) {
				case 'b': *ptr2++='\b';	break;
				case 'f': *ptr2++='\f';	break;
				case 'n': *ptr2++='\n';	break;
				case 'r': *ptr2++='\r';	break;
				case 't': *ptr2++='\t';	break;
				case 'u':	 /* transcode utf16 to utf8. DOES NOT SUPPORT SURROGATE PAIRS CORRECTLY. */
					sscanf(ptr+1,"%4x",&uc);	/* get the unicode char. */
					len=3;if (uc<0x80) len=1;else if (uc<0x800) len=2;ptr2+=len;
					
					switch (len) {
						case 3: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
						case 2: *--ptr2 =((uc | 0x80) & 0xBF); uc >>= 6;
						case 1: *--ptr2 =(uc | firstByteMark[len]);
					}
					ptr2+=len;ptr+=4;
					break;
				default:  *ptr2++=*ptr; break;
			}
			ptr++;
		}
	}
	*ptr2=0;
   if (*ptr=='\"')
      ptr++;
   item->valueString=out;
	item->type=sJSON_String;
	return ptr;
}

/* Render the cstring provided to an escaped version that can be printed. */
static char *print_string_ptr(const char *str) {
   const char *ptr;
   char *ptr2,*out;
   int len=0;
   unsigned char token;
	
   if (!str)
      return sJSON_strdup("");
   ptr=str;
   while ((token=*ptr) && ++len) {
      if (strchr("\"\\\b\f\n\r\t",token))
         len++;
      else if (token<32)
         len+=5;
      ptr++;
   }
	
	out=(char*)sJSON_malloc(len+3);
   if (!out)
      return 0;

	ptr2=out;ptr=str;
	*ptr2++='\"';
   while (*ptr) {
      if ((unsigned char)*ptr>31 && *ptr!='\"' && *ptr!='\\')
         *ptr2++=*ptr++;
      else {
			*ptr2++='\\';
         switch (token=*ptr++) {
				case '\\':	*ptr2++='\\';	break;
				case '\"':	*ptr2++='\"';	break;
				case '\b':	*ptr2++='b';	break;
				case '\f':	*ptr2++='f';	break;
				case '\n':	*ptr2++='n';	break;
				case '\r':	*ptr2++='r';	break;
				case '\t':	*ptr2++='t';	break;
				default: sprintf(ptr2,"u%04x",token);ptr2+=5;	break;	/* escape and print */
			}
		}
	}
   *ptr2++='\"';
   *ptr2++=0;
	return out;
}
/* Invote print_string_ptr (which is useful) on an item. */
static char *print_string(sJSON *item)	{
   return print_string_ptr(item->valueString);
}

/* Predeclare these prototypes. */
static const char *parse_value(sJSON *item,const char *value);
static const char *parse_array(sJSON *item,const char *value);
static const char *parse_object(sJSON *item,const char *value);
static char *print_value(sJSON *item,int depth,int fmt);
static char *print_array(sJSON *item,int depth,int fmt);
static char *print_object(sJSON *item,int depth,int fmt);


/* Utility to jump whitespace and cr/lf */
static const char *skip(const char *in) {
   bool checkAgain;
   do {
      checkAgain = false;
      while (in && *in && (unsigned char)*in<=32)
         ++in;
      if(*in && (*in == '/')) {
         if(*(in+1) && (*(in+1) == '/')) {
            //skip comment till end of line..
            while(*in && ((*in != 10 && *in != 13)))
               ++in;
            //the while-loop at the beginning does the skipping for us :)

            checkAgain = true;      //check next line for comments
         } else if(*(in+1) && (*(in+1) == '*')) {
            //find comment end
         find_next:
            while(*in && (*in != '*'))
               ++in;
            if(*(in+1) && *(in+1) != '/') {
               ++in;             //skip *
               goto find_next;
            }
            in += 2;
            checkAgain = true;
         }
      }
   } while(checkAgain == true);
   return in;
}

/* Parse an object - create a new root, and populate. */
sJSON *sJSONparse(const char *value) {
	ep=0;
	sJSON *c=sJSON_New_Item();
   if (!c)
      return 0;       /* memory fail */

   value = skip(value);
   if(*value == '{' || *value=='[') {  //old style json-file?
      if (!parse_value(c,skip(value))) {
         sJSONdelete(c);
         return 0;
      }
   } else {
      if (!parse_object(c,skip(value))) {
         sJSONdelete(c);
         return 0;
      }
   }
	return c;
}

/* Render a sJSON item/entity/structure to text. */
char *sJSONprint(sJSON *item)				{
   return print_value(item,0,1);
}
char *sJSONprintUnformatted(sJSON *item)	{
   return print_value(item,0,0);
}


/* Parser core - when encountering text, process appropriately. */
static const char *parse_value(sJSON *item,const char *value) {
   if (!value)
      return 0;	/* Fail on null. */
   if (!strncmp(value,"null",4))	{
      item->type=sJSON_NULL;
      return value+4;
   }
   if (!strncmp(value,"false",5)) {
      item->type=sJSON_False;
      return value+5;
   }
   if (!strncmp(value,"true",4))	{
      item->type=sJSON_True;
      item->valueInt=1;
      return value+4;
   }
   if (*value=='-' || (*value>='0' && *value<='9'))
      return parse_number(item,value);
   if (*value=='[')
      return parse_array(item,value);
   if (*value=='{') {
      return parse_object(item,skip(value+1));
   }
   if (*value=='\"')
      return parse_string(item,value);

   ep=value;
   return 0;	/* failure. */
}

/* Render a value to text. */
static char *print_value(sJSON *item,int depth,int fmt) {
   char *out=0;
   if (!item)
      return 0;
   switch ((item->type)&255) {
      case sJSON_NULL:   out=sJSON_strdup("null");	break;
      case sJSON_False:  out=sJSON_strdup("false");break;
      case sJSON_True:	 out=sJSON_strdup("true"); break;
      case sJSON_Number: out=print_number(item);break;
      case sJSON_String: out=print_string(item);break;
      case sJSON_Array:  out=print_array(item,depth,fmt);break;
      case sJSON_Object: out=print_object(item,depth,fmt);break;
   }
   return out;
}


/* Build an array from input text. */
static const char *parse_array(sJSON *item,const char *value) {
	sJSON *child;
   if (*value!='[')	{  /* not an array! */
      ep=value;
      return 0;
   }

   item->type = sJSON_Array;
   value = skip(value+1);
   if (*value == ']')     /* empty array. */
      return value+1;

   item->child = child = sJSON_New_Item();
   if (!item->child) /* memory fail */
      return 0;
   value = skip(parse_value(child,skip(value)));	/* skip any spacing, get the value. */
   if (!value)
      return 0;

   while(*value != ']') {
		sJSON *new_item;
      if (!(new_item = sJSON_New_Item())) /* memory fail */
         return 0;
      child->next = new_item;
      new_item->prev = child;
      child = new_item;
      if(*value == ',')
         value=skip(parse_value(child,skip(value+1)));
      else
         value=skip(parse_value(child,skip(value)));
      if (!value)       /* memory fail */
         return 0;
	}

   if (*value == ']')     /* end of array */
      return value+1;
   ep=value;      /* malformed. */
   return 0;
}

/* Render an array to text */
static char *print_array(sJSON *item,int depth,int fmt) {
   char **entries;
   char *out=0,*ptr,*ret;
   int len=5;
   sJSON *child=item->child;
   int numentries=0,i=0,fail=0;

   /* How many entries in the array? */
   while (child) {
      numentries++;
      child=child->next;
   }
   /* Allocate an array to hold the values for each */
   entries=(char**)sJSON_malloc(numentries*sizeof(char*));
   if (!entries)
      return 0;
   memset(entries,0,numentries*sizeof(char*));
   /* Retrieve all the results: */
   child=item->child;
   while (child && !fail) {
      ret=print_value(child,depth+1,fmt);
      entries[i++]=ret;
      if (ret)
         len+=strlen(ret)+2+(fmt?1:0);
      else
         fail=1;
      child=child->next;
   }

   /* If we didn't fail, try to malloc the output string */
   if (!fail)
      out=(char*)sJSON_malloc(len);
   /* If that fails, we fail. */
   if (!out)
      fail=1;

   /* Handle failure. */
   if (fail) {
      for (i=0;i<numentries;i++)
         if (entries[i])
            sJSON_free(entries[i]);
      sJSON_free(entries);
      return 0;
   }

   /* Compose the output array. */
   *out='[';
   ptr=out+1;
   *ptr=0;
   for (i=0;i<numentries;i++) {
      strcpy(ptr,entries[i]);
      ptr+=strlen(entries[i]);
      if (i!=numentries-1) {
         *ptr++=',';
         if(fmt)
            *ptr++=' ';
         *ptr=0;
      }
      sJSON_free(entries[i]);
   }
   sJSON_free(entries);
   *ptr++=']';
   *ptr++=0;
   return out;
}

/* Build an object from the text. */
static const char *parse_object(sJSON *item,const char *value) {
	sJSON *child;
//   if (*value!='{')	{     /* not an object! */
//      ep=value;
//      return 0;
//   }
//	value=skip(value+1);

	item->type=sJSON_Object;
   if (*value=='}')     /* empty array. */
      return value+1;
	
	item->child=child=sJSON_New_Item();
   if (!item->child)
      return 0;
   value=skip(parse_string_or_identifier(child,skip(value)));
   if (!value)
      return 0;
   child->nameHash = eastl::murmurString(child->valueString);
   child->nameString = child->valueString;
   child->valueString = 0;
   if ((*value != ':') && (*value != '=')) {
      ep=value;      /* fail! */
      return 0;
   }
	value=skip(parse_value(child,skip(value+1)));	/* skip any spacing, get the value. */
   if (!value)
      return 0;
	
   while((*value!='}')&&(*value != 0)) {
		sJSON *new_item;
      if (!(new_item=sJSON_New_Item()))
         return 0; /* memory fail */
      child->next=new_item;
      new_item->prev=child;
      child=new_item;
      if(*value == ',')
         value=skip(parse_string_or_identifier(child,skip(value+1)));
      else
         value=skip(parse_string_or_identifier(child,skip(value)));
      if (!value)
         return 0;
      child->nameHash = eastl::murmurString(child->valueString);
      child->nameString=child->valueString;
      child->valueString=0;
      if ((*value!=':') && (*value!='=')) {
         ep=value;   	/* fail! */
         return 0;
      }
		value=skip(parse_value(child,skip(value+1)));	/* skip any spacing, get the value. */
      if (!value)
         return 0;
	}

   if(*value == 0)   //file end
      return value;
   if(*value == '}')
      return value+1;	/* end of array */
   ep=value;
   return 0;	/* malformed. */
}

/* Render an object to text. */
static char *print_object(sJSON *item,int depth,int fmt) {
   char **entries=0, **names=0;
   char *out=0, *ptr, *ret, *str;
   int len=7, i=0, j;
   sJSON *child = item->child;
   int numentries=0, fail=0;
   /* Count the number of entries. */
   while (child) {
      numentries++;
      child=child->next;
   }
   /* Allocate space for the names and the objects */
   entries=(char**)sJSON_malloc(numentries*sizeof(char*));
   if (!entries)
      return 0;
   names=(char**)sJSON_malloc(numentries*sizeof(char*));
   if (!names) {
      sJSON_free(entries);
      return 0;
   }
   memset(entries,0,sizeof(char*)*numentries);
   memset(names,0,sizeof(char*)*numentries);

   /* Collect all the results into our arrays: */
   child=item->child;depth++;
   if (fmt)
      len+=depth;
   while (child) {
      names[i] = str = print_string_ptr(child->nameString);
      entries[i++] = ret = print_value(child,depth,fmt);
      if (str && ret)
         len+=strlen(ret)+strlen(str)+2+(fmt?2+depth:0);
      else
         fail=1;
      child=child->next;
   }

   /* Try to allocate the output string */
   if (!fail)
      out=(char*)sJSON_malloc(len);
   if (!out)
      fail=1;

   /* Handle failure */
   if (fail) {
      for (i=0;i<numentries;i++) {
         if (names[i])
            sJSON_free(names[i]);
         if (entries[i])
            sJSON_free(entries[i]);
      }
      sJSON_free(names);
      sJSON_free(entries);
      return 0;
   }

   /* Compose the output: */
   *out='{';ptr=out+1;if (fmt)*ptr++='\n';*ptr=0;
   for (i=0;i<numentries;i++) {
      if (fmt)
         for (j=0;j<depth;j++)
            *ptr++='\t';
      strcpy(ptr,names[i]);
      ptr+=strlen(names[i]);
      *ptr++=':';
      if (fmt)
         *ptr++='\t';
      strcpy(ptr,entries[i]);
      ptr+=strlen(entries[i]);
      if (i!=numentries-1)
         *ptr++=',';
      if (fmt)
         *ptr++='\n';
      *ptr=0;
      sJSON_free(names[i]);
      sJSON_free(entries[i]);
   }

   sJSON_free(names);
   sJSON_free(entries);
   if (fmt)
      for (i=0;i<depth-1;i++)
         *ptr++='\t';
   *ptr++='}';
   *ptr++=0;
   return out;
}

/* Get Array size/item / object item. */
uint32_t sJSONgetArraySize(sJSON *array) {
   sJSON *c=array->child;
   int i=0;
   while(c) {
      i++;
      c=c->next;
   }
   return i;
}
sJSON *sJSONgetArrayItem(sJSON *array,int item) {
   sJSON *c=array->child;
   while (c && item>0) {
      item--;
      c=c->next;
   }
   return c;
}
sJSON *sJSONgetObjectItem(sJSON *object, eastl::FixedMurmurHash stringHash) {
   sJSON *c=object->child;
   while(c && compareNames( c, stringHash) )
      c=c->next;
   return c;
}


/* Utility for array list handling. */
static void suffix_object(sJSON *prev, sJSON *item) {
   prev->next=item;
   item->prev=prev;
}
/* Utility for handling references. */
static sJSON *create_reference(sJSON *item) {
   sJSON *ref=sJSON_New_Item();
   if (!ref)
      return 0;
   memcpy(ref,item,sizeof(sJSON));
   ref->nameString = 0;
   ref->nameHash = 0;
   ref->type |= sJSON_IsReference;
   ref->next = ref->prev = 0;
   return ref;
}

/* Add item to array/object. */
void   sJSONaddItemToArray(sJSON *array, sJSON *item) {
   sJSON *c=array->child;
   if (!item)
      return;
   if (!c) {
      array->child=item;
   } else {
      while (c && c->next)
         c=c->next;
      suffix_object(c,item);
   }
}
void   sJSONaddItemToObject(sJSON *object, const char *string, sJSON *item)	{
   if (!item)
      return;
   if (item->nameString)
      sJSON_free(item->nameString);
   item->nameString=sJSON_strdup(string);
   item->nameHash = eastl::murmurString(string);
   sJSONaddItemToArray(object,item);
}
void	sJSONaddItemReferenceToArray(sJSON *array, sJSON *item) {
   sJSONaddItemToArray(array,create_reference(item));
}
void	sJSONaddItemReferenceToObject(sJSON *object,const char *string,sJSON *item) {
   sJSONaddItemToObject(object,string,create_reference(item));
}

sJSON *sJSONdetachItemFromArray(sJSON *array, int which)			{
   sJSON *c=array->child;
   while (c && which>0) {
      c=c->next;
      which--;
   }
   if (!c)
      return 0;
   if (c->prev)
      c->prev->next=c->next;
   if (c->next)
      c->next->prev=c->prev;
   if (c==array->child)
      array->child=c->next;
   c->prev=c->next=0;
   return c;
}
void   sJSONdeleteItemFromArray(sJSON *array,int which) {
   sJSONdelete(sJSONdetachItemFromArray(array,which));
}
sJSON *sJSONdetachItemFromObject(sJSON *object,const char *string) {
   int i=0;
   sJSON *c=object->child;
   eastl::FixedMurmurHash stringHash(string);
   while(c && compareNames( c, stringHash ) ) {
      i++;
      c=c->next;
   }
   if (c)
      return sJSONdetachItemFromArray(object,i);
   return 0;
}

void   sJSONdeleteItemFromObject(sJSON *object,const char *string) {
   sJSONdelete(sJSONdetachItemFromObject(object,string));
}

/* Replace array/object items with new ones. */
void   sJSONreplaceItemInArray(sJSON *array,int which,sJSON *newitem) {
   sJSON *c=array->child;
   while (c && which>0) {
      c=c->next;
      which--;
   }
   if (!c)
      return;
   newitem->next=c->next;
   newitem->prev=c->prev;
   if (newitem->next)
      newitem->next->prev=newitem;
   if (c==array->child)
      array->child=newitem;
   else
      newitem->prev->next=newitem;
   c->next=c->prev=0;
   sJSONdelete(c);
}
void   sJSONreplaceItemInObject(sJSON *object,const char *string,sJSON *newitem) {
   int i=0;
   sJSON *c=object->child;
   eastl::FixedMurmurHash stringHash(string);
   while(c && compareNames( c, stringHash ) ) {
      i++;
      c=c->next;
   }
   if(c) {
      newitem->nameString=sJSON_strdup(string);
      newitem->nameHash = stringHash.mHash;
      sJSONreplaceItemInArray(object,i,newitem);
   }
}

/* Create basic types: */
sJSON *sJSONcreateNull()					{sJSON *item=sJSON_New_Item();if(item)item->type=sJSON_NULL;return item;}
sJSON *sJSONcreateTrue()					{sJSON *item=sJSON_New_Item();if(item)item->type=sJSON_True;return item;}
sJSON *sJSONcreateFalse()					{sJSON *item=sJSON_New_Item();if(item)item->type=sJSON_False;return item;}
sJSON *sJSONcreateBool(int b)				{sJSON *item=sJSON_New_Item();if(item)item->type=b?sJSON_True:sJSON_False;return item;}
sJSON *sJSONcreateNumber(double num)	{sJSON *item=sJSON_New_Item();if(item){item->type=sJSON_Number;item->valueDouble=num;item->valueInt=(int)num;}return item;}
sJSON *sJSONcreateString(const char *string)	{sJSON *item=sJSON_New_Item();if(item){item->type=sJSON_String;item->valueString=sJSON_strdup(string);}return item;}
sJSON *sJSONcreateArray()					{sJSON *item=sJSON_New_Item();if(item)item->type=sJSON_Array;return item;}
sJSON *sJSONcreateObject()					{sJSON *item=sJSON_New_Item();if(item)item->type=sJSON_Object;return item;}

/* Create Arrays: */
sJSON *sJSONcreateIntArray(int *numbers,int count)				 {int i;sJSON *n=0,*p=0,*a=sJSONcreateArray();for(i=0;a && i<count;i++){n=sJSONcreateNumber(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
sJSON *sJSONcreateFloatArray(float *numbers,int count)		 {int i;sJSON *n=0,*p=0,*a=sJSONcreateArray();for(i=0;a && i<count;i++){n=sJSONcreateNumber(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
sJSON *sJSONcreateDoubleArray(double *numbers,int count)		 {int i;sJSON *n=0,*p=0,*a=sJSONcreateArray();for(i=0;a && i<count;i++){n=sJSONcreateNumber(numbers[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
sJSON *sJSONcreateStringArray(const char **strings,int count){int i;sJSON *n=0,*p=0,*a=sJSONcreateArray();for(i=0;a && i<count;i++){n=sJSONcreateString(strings[i]);if(!i)a->child=n;else suffix_object(p,n);p=n;}return a;}
