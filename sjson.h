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

#ifndef sJSON__h
#define sJSON__h

#include "RuntimeSourceDependency.h"
RUNTIME_COMPILER_SOURCEDEPENDENCY;

#include <stdint.h>

/* sJSON Types: */
#define sJSON_False 0
#define sJSON_True 1
#define sJSON_NULL 2
#define sJSON_Number 3
#define sJSON_String 4
#define sJSON_Array 5
#define sJSON_Object 6
	
#define sJSON_IsReference 256

#include "murmurhash.h"


/* The sJSON structure: */
typedef struct sJSON {
   struct sJSON *next,*prev;	/* next/prev allow you to walk array/object chains. Alternatively,
                                 use GetArraySize/GetArrayItem/GetObjectItem */
   struct sJSON *child;       /* An array or object item will have a child pointer pointing to
                                 a chain of the items in the array/object. */
	int type;					/* The type of the item, as above. */

   char *valueString;		/* The item's string, if type==sJSON_String */
   int valueInt;				/* The item's number, if type==sJSON_Number */
   double valueDouble;		/* The item's number, if type==sJSON_Number */

   char *nameString;			/* The item's name string, if this item is the child of, or is
                              in the list of subitems of an object. */
   uint32_t nameHash;
} sJSON;

typedef struct sJSON_Hooks {
      void *(*malloc_fn)(size_t sz);
      void (*free_fn)(void *ptr);
} sJSON_Hooks;

/* Supply malloc, realloc and free functions to sJSON */
extern void sJSONinitHooks(sJSON_Hooks* hooks);


/* Supply a block of JSON, and this returns a sJSON object you can interrogate. Call sJSON_Delete when finished. */
extern sJSON *sJSONparse(const char *value);

/* Render a sJSON entity to text for transfer/storage. Free the char* when finished. */
extern char  *sJSONprint(sJSON *item);
/* Render a sJSON entity to text for transfer/storage without any formatting. Free the char* when finished. */
extern char  *sJSONprintUnformatted(sJSON *item);
/* Delete a sJSON entity and all subentities. */
extern void   sJSONdelete(sJSON *c);

/* Returns the number of items in an array (or object). */
extern uint32_t sJSONgetArraySize(sJSON *array);
/* Retrieve item number "item" from array "array". Returns NULL if unsuccessful. */
extern sJSON *sJSONgetArrayItem(sJSON *array,int item);
/* Get item "string" from object. Case SENSITIVE! */
extern sJSON *sJSONgetObjectItem(sJSON *object, eastl::FixedMurmurHash stringHash);
extern sJSON *sJSONgetObjectItem(sJSON *object, uint32_t stringHash);


/* For analysing failed parses. This returns a pointer to the parse error. You'll probably need to look a
   few chars back to make sense of it. Defined when sJSON_Parse() returns 0. 0 when sJSON_Parse() succeeds. */
extern const char *sJSONgetErrorPtr();

/* These calls create a sJSON item of the appropriate type. */
extern sJSON *sJSONcreateNull();
extern sJSON *sJSONcreateTrue();
extern sJSON *sJSONcreateFalse();
extern sJSON *sJSONcreateBool(int b);
extern sJSON *sJSONcreateNumber(double num);
extern sJSON *sJSONcreateString(const char *string);
extern sJSON *sJSONcreateArray();
extern sJSON *sJSONcreateObject();

/* These utilities create an Array of count items. */
extern sJSON *sJSONcreateIntArray(int *numbers,int count);
extern sJSON *sJSONcreateFloatArray(float *numbers,int count);
extern sJSON *sJSONcreateDoubleArray(double *numbers,int count);
extern sJSON *sJSONcreateStringArray(const char **strings,int count);


/* Append item to the specified array/object. */
extern void sJSONaddItemToArray(sJSON *array, sJSON *item);
extern void	sJSONaddItemToObject(sJSON *object,const char *string,sJSON *item);
/* Append reference to item to the specified array/object. Use this when you want to add an existing #
   sJSON to a new sJSON, but don't want to corrupt your existing sJSON. */
extern void sJSONaddItemReferenceToArray(sJSON *array, sJSON *item);
extern void	sJSONaddItemReferenceToObject(sJSON *object,const char *string,sJSON *item);

/* Remove/Detatch items from Arrays/Objects. */
extern sJSON *sJSONdetachItemFromArray(sJSON *array,int which);
extern void   sJSONdeleteItemFromArray(sJSON *array,int which);
extern sJSON *sJSONdetachItemFromObject(sJSON *object,const char *string);
extern void   sJSONdeleteItemFromObject(sJSON *object,const char *string);
	
/* Update array items. */
extern void sJSONreplaceItemInArray(sJSON *array,int which,sJSON *newitem);
extern void sJSONreplaceItemInObject(sJSON *object,const char *string,sJSON *newitem);

#define sJSONaddNullToObject(object,name)       sJSONaddItemToObject(object, name, sJSONcreateNull())
#define sJSONaddTrueToObject(object,name)       sJSONaddItemToObject(object, name, sJSONcreateTrue())
#define sJSONaddFalseToObject(object,name)		sJSONaddItemToObject(object, name, sJSONcreateFalse())
#define sJSONaddNumberToObject(object,name,n)	sJSONaddItemToObject(object, name, sJSONcreateNumber(n))
#define sJSONaddStringToObject(object,name,s)	sJSONaddItemToObject(object, name, sJSONcreateString(s))

#endif
