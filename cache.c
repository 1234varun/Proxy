#include "cache.h"
#include "csapp.h"

void insert_cache(web_res *pointer,char* wobject,char *url,char* cookie,int rsize)
{
  /* Iterate through the list till we encounter the last web_res .*/
  while(pointer->next!=NULL)
    {
      pointer = pointer -> next;
    }
  /* Allocate memory for the new web_res  and put data in it.*/
  pointer->next = (web_res *)malloc(sizeof(web_res ));
  pointer = pointer->next;
  pointer->url = url;
  pointer->cookie = cookie;
  pointer->rsize=rsize;
  pointer->wobject=wobject;
  pointer->next=NULL;
}


web_res* find_cache(web_res *pointer,char *url,char *cookie )
{       
  pointer =  pointer -> next; //First web_res  is dummy web_res .
  /* Iterate through the entire linked list and search for the key. */
  while(pointer!=NULL)
    {
      if(!strcmp(pointer->url,url) && !strcmp(pointer->cookie,cookie) ) //key is found.
	{         
	  return pointer;
	}
      pointer = pointer -> next;//Search in the next web_res .
    }
  /*Key is not found */
  return NULL;
}


int delete_cache(web_res *pointer)
{ int size;
  web_res *temp;
  printf("inside delete \n");      
  temp = pointer -> next;
  /*temp points to the web_res  which has to be removed*/
  pointer->next = temp->next;
  /*We removed the web_res  which is next to the pointer (which is also temp) */
  size=temp->rsize;
  //  free(temp->url);
  // free(temp->cookie);
  //free(temp);
  return size; 
 /* Beacuse we deleted the web_res , we no longer require the memory used for it . 
           free() will deallocate the memory.
  */
}

void lru_cache(web_res *pointer,char *url,char *cookie)
{
  
  web_res* start=pointer;
  printf("inside LRU \n");
  /* Go to the web_res  for which the web_res  next to it has to be deleted */
  while(pointer->next!=NULL && !(!strcmp((pointer->next)->url,url) && !strcmp((pointer->next)->cookie,cookie)))
    {
      pointer = pointer -> next;
    }
   if(pointer->next==NULL)
    {
      printf("Object is not present in the list\n");
      return;
    }
   web_res *temp;
   temp = pointer -> next;
   /*temp points to the web_res  which has to be removed*/
   pointer->next = temp->next;
   /*We removed the web_res  which is next to the pointer (which is also temp) */

   pointer=start; 

   while(pointer->next!=NULL)
     {
       pointer = pointer -> next;
     }   
   pointer->next=temp;
   temp->next=NULL;    
 
}

void print_cache(web_res *pointer)
{
  int total_size=0;
  pointer=pointer->next;
  while(pointer->next!=NULL)
    {
        printf("url which is cached is %s\n",pointer->url); 
	//  printf("cookie which is cached is %s\n",pointer->cookie);
	// printf("size which is cached is %d\n",pointer->rsize);
      total_size=total_size+pointer->rsize;
      pointer = pointer -> next;
    } 
  printf("url which is cached is %s\n",pointer->url);  
  total_size=total_size+pointer->rsize;
  printf("Total Size cached is %d\n",total_size);
    fflush(stdout);
}
