typedef struct web_res 
{
  struct web_res *next;
  char *url;   
  char *cookie;
  char *wobject;
  int rsize;
}web_res;

void insert_cache(web_res *pointer,char* wobject,char *url,char*cookie,int rsize);
web_res* find_cache(web_res *pointer,char *url,char *cookie );
int delete_cache(web_res *pointer);
void lru_cache(web_res *pointer,char *url,char *cookie);
void print_cache(web_res *pointer);
