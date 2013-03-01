#include <stdio.h>
#include "csapp.h"
#include "sbuf.h"
#include "cache.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define Host 0
#define Proxy_Connection 1
#define User_Agent 2        
#define Accept_Encoding 3 
#define Accept_ 4
#define Connection 5
#define Cookie 6
#define NTHREADS 8
#define SBUFSIZE 32
                          

static const char *user_agent = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_type = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding = "Accept-Encoding: gzip, deflate\r\n";
static const char* proxy_connection="Proxy-Connection:close\r\n";
static const char* connection_close="Connection:close\r\n";
void doit(int fd);
void read_requesthdrs(rio_t *rp);
void clienterror(int fd, char *cause, char *errnum,char *shortmsg, char *longmsg);
int matchString(char *header_name,char* is_header_set,char *req,char *next_line,char* cookie);
void findHostName(char* url,char* hostname,int *port,char *uri);
void addAbsentHeader(char *req,char *is_header_set,char* host_header,char* cookie);
void forwardRequest(char *hostname,int *port,int c_fd,char *req,char* cookie,char* url);
int resolveHost(char *hostname,int *port);
void *thread(void *vargp);
void initialize_cache();
void read_lock();
void read_free();
void write_lock();
void write_free();



//Shared Global Variable
sbuf_t sbuf;
int readcnt=0;
sem_t mutex,w;
int csize;
web_res *root;

int main(int argc, char **argv)
{ 
    int i=0;
    pthread_t tid;
  // printf("%s%s%s", user_agent, accept, accept_encoding);
  int listenfd, connfd, port, clientlen;
  struct sockaddr_in clientaddr;
  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  port = atoi(argv[1]);
  initialize_cache();
  sem_init(&mutex,0,1);
  sem_init(&w,0,1);     
  Signal(SIGPIPE, SIG_IGN);
  sbuf_init(&sbuf, SBUFSIZE);
  listenfd = Open_listenfd(port);

  for(i=0;i<NTHREADS;i++)
    {
      Pthread_create(&tid, NULL,thread, NULL);

    }


  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:netp:tiny:accept
    sbuf_insert(&sbuf,connfd); 

   // doit(connfd);                                             //line:netp:tiny:doit
    // Close(connfd);                                            //line:netp:tiny:close
  }
    return 0;
}


/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd) 
{
  int is_static,port;
  struct stat sbuf;
  char method[MAXLINE],uri[MAXLINE],version[MAXLINE];
  char filename[MAXLINE];
  rio_t rio;
  char next_line[MAXLINE],req[MAX_OBJECT_SIZE];
  char header_name[MAXLINE],hostname[MAXLINE];
  char url[MAXLINE],host_header[MAXLINE];
  char is_header_set[7],cookie[MAXLINE];
  int con_true;  
   
 
  memset(is_header_set,0,7);
  /*request line and headers */
  Rio_readinitb(&rio,fd);
  con_true= Rio_readlineb(&rio,next_line,MAXLINE);                   //line:netp:doit:readrequest
  if (con_true<=0)
    return;
  
  sscanf(next_line,"%s %s %s",method,url,version);       //line:netp:doit:parserequest
  //  printf("%s %s %s \n", method, url,version);
  
  //ignore client version
  version[0]='\0';
  strcat(version," HTTP/1.0");
 
 if (strcasecmp(method,"GET")) {                     //line:netp:doit:beginrequesterr
    clienterror(fd, method, "501", "Not Implemented",
                "proxy does not implement this method");
    printf("failure in comparision \n");
    return;
  }         

  //initialize buffer
  next_line[0]='\0';
  host_header[0]='\0';
  req[0]='\0';
  cookie[0]='\0';

  findHostName(url,hostname,&port,uri);                                         //line:netp:doit:endrequesterr
 
  //add the requested resource
  strcat(req,method);
  strcat(req," ");
  strcat(req,uri);
  strcat(req,version);
  strcat(req,"\r\n");


  //Create host header
  strcat(host_header,"Host:");
  strcat(host_header,hostname);
  strcat(host_header,"\r\n"); 

  //  printf("parsing done %s %d %s \n",hostname,port,uri);     

  Rio_readlineb(&rio,next_line,MAXLINE);
  while(strcmp(next_line,"\r\n")) {          //line:netp:readhdrs:checkterm
     
   sscanf(next_line,"%[^:]",header_name);
  
   if(matchString(header_name,is_header_set,req,next_line,cookie))
     {    
       strcat(req,next_line);
     }

    // printf("%s \n",next_line);
   con_true=Rio_readlineb(&rio, next_line, MAXLINE);
   if (con_true <= 0)
     return;
  }
  //   printf(" request is \n\n\n\n\n %s ",req);

  addAbsentHeader(req,is_header_set,host_header,cookie);
   strcat(req,"\r\n");
   //   printf(" request is \n%s\n ",req);
   forwardRequest(hostname,&port,fd,req,cookie,url);
  return;
}
/* $end read_requesthdrs */


/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */

int matchString(char *header_name,char* is_header_set,char *req,char *next_line,char* cookie)
{

  if (!strcmp("Host", header_name)) {
      is_header_set[Host] = 1;
      strcat(req,next_line);
      return 0;
  }
  else if (!strcmp("Proxy-Connection",header_name)) {
        is_header_set[Proxy_Connection] = 1;   
	strcat(req,proxy_connection);
        return 0;
  }
  else if (!strcmp("User-Agent",header_name)) {
      is_header_set[User_Agent] = 1;
      strcat(req,user_agent);
      return 0;
  }
  else if (!strcmp("Accept-Encoding",header_name)) {
    is_header_set[Accept_Encoding] = 1;
    strcat(req,accept_encoding);
    return 0; 
 }

  else if (!strcmp("Accept",header_name)) {
    is_header_set[Accept_] = 1;
    strcat(req,accept_type);
    return 0;
  }
  else if(!strcmp("Connection",header_name)) {
    is_header_set[Connection] = 1;
    strcat(req,connection_close);
    return 0;
  }
  else if(!strcmp("Cookie",header_name))
    {
      strcat(cookie,next_line);
      is_header_set[Cookie] = 1;
       
    }
  return 1;
}

void findHostName(char* url,char* hostname,int *port,char *uri)
{
  int count;
  *port=0;
  //  printf("url is %s \n\n",url);
   count=sscanf(url,"http://%[^:]:%d%s",hostname,port,uri);
 
  if(count != 3)
    {
        count=sscanf(url,"http://%[^/]%s",hostname,uri);
      if(count !=2)
        {
          printf("parse_error");

        }

    }
  if(*port==0)
    {
      *port=80;
    }
  //  printf("exited\n");
   
}



void addAbsentHeader(char *req,char *is_header_set,char* host_header,char* cookie)
{  
  
      if(is_header_set[Host]!=1)
	{
          strcat(req,host_header);
        }
      if(is_header_set[Proxy_Connection]!=1)  
        {
	  strcat(req,proxy_connection);
        }
      if(is_header_set[User_Agent]!=1)
        {
	   strcat(req,user_agent);
        }
      if(is_header_set[Accept_]!=1)
        {
	  strcat(req,accept_type);
        }
      if(is_header_set[Connection]!=1)
        {
	  strcat(req,connection_close);         
               
        }
      if(is_header_set[Accept_Encoding]!=1)
        {
          strcat(req,accept_encoding);

        }
      if(is_header_set[Cookie]!=1)
        {
          strcpy(cookie,"absent");

        }

 
}

int resolveHost(char *hostname,int *port)
{

  int s_fd;  
  struct addrinfo hints, *servinfo, *p;
  int rval;
  char buffer[MAXLINE]; 

  sprintf(buffer,"%d",*port); 

  memset(&hints, 0,sizeof hints);
  hints.ai_family = AF_UNSPEC; 
  hints.ai_socktype = SOCK_STREAM;

  if ((rval = getaddrinfo(hostname,buffer,&hints,&servinfo)) != 0) {
    //    printf(stderr, "getaddrinfo: %s\n", gai_strerror(rval));
    fprintf(stderr, "failed to resolve \n");
    s_fd=0; 
 }

  // loop through all the results and connect to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((s_fd = socket(p->ai_family, p->ai_socktype,
			 p->ai_protocol)) == -1) {
      perror("socket");
      continue;
    }

    if (connect(s_fd, p->ai_addr, p->ai_addrlen) == -1) {
      close(s_fd);
      perror("connect");
      continue;
    }

    break; // if connection was successful break
  }

  if (p == NULL) {
    // no connection
    fprintf(stderr, "failed to connect\n");
    s_fd=0;
  }

  return s_fd;
  //  freeaddrinfo(servinfo); // all done with this structure

}

void forwardRequest(char *hostname,int *port,int c_fd,char *req,char * cookie,char* url)
{

  int s_fd,rsize=0;
  rio_t s_rio;
  char next_line[MAXLINE];
  int length=0;
  char header_name[MAXLINE]; 
  int con_true=0;
  int read_count=0,no_cache=0;
  char res[MAX_OBJECT_SIZE];
  web_res *fetched=NULL;  
  char *wobject,*wurl,*wcookie;
  
 printf("Requested url is :%s\n",url);
//initialize buffer 
  next_line[0]='\0';
  header_name[0]='\0';
  res[0]='\0'; 
  length=0; 

   s_fd=resolveHost(hostname,port);   
   // printf("server desciptor is %d \n",s_fd);
   Rio_readinitb(&s_rio,s_fd);
 
   /* check if request in cache */
   read_lock();
   fetched=find_cache(root,url,cookie);
  
   if(fetched!=NULL)
     { 
   rsize=fetched->rsize;
   wobject=fetched->wobject;
     }

   read_free();
     
   //   printf("getting response for url%s \n",url); 
   read_count=0;
   if(rsize>0)
     { 
       // printf("content from cache\n");
       printf("Cache served this request: %s\n",url);
       do{
	 //printf("loop3\n");
	  if(rsize-MAXLINE>=0)
	   {
	    memcpy(next_line,wobject+read_count,MAXLINE);
	    con_true=rio_writen(c_fd,next_line,MAXLINE);
	    read_count=read_count+MAXLINE;
            rsize=rsize-MAXLINE;  
           }
           else
	     {
               memcpy(next_line,wobject+read_count,rsize);
               con_true=rio_writen(c_fd,next_line,rsize);
               break;
             }

       }while(con_true>0);
   
       write_lock();
       lru_cache(root,url,cookie);
       write_free();
 
      }
    else
      {   printf("Web server served  this request: %s\n",url);
	//Send request to server

	if ((rio_writen(s_fd,req,strlen(req)) <= 0)) {
	  printf("writen error.\n");
	  return;
	}
	//	printf("request sent to server for url %s \n",url);    

    do{
      // printf("loop2\n");
     con_true = rio_readnb(&s_rio,next_line,MAXBUF);
        
     if(read_count+con_true >=MAX_OBJECT_SIZE)
       {
         printf("response too big \n");
         no_cache=1;
         break;
       }
     rio_writen(c_fd,next_line,con_true);
     memcpy(res+read_count,next_line,con_true); 
     read_count=read_count+con_true; 
    }while(con_true>0);
      
    printf("response sent to client %d bytes \n",read_count);
    if(!no_cache)
      {   
    wobject=(char *)malloc(read_count);
    wurl=(char *)malloc(MAXLINE);
    wcookie=(char *)malloc(MAXLINE);
   
    if(wobject!=NULL && wurl!=NULL && wcookie!=NULL)
      {
        memcpy(wobject,res,read_count);
        memcpy(wurl,url,strlen(url));
        memcpy(wcookie,cookie,strlen(cookie));
        
      }
    else
      {
	return;
      }
     
      write_lock();   
      
        while(csize+read_count >MAX_CACHE_SIZE)
	  {
          csize=csize-delete_cache(root); 
	  // printf("loop1\n");
          }
	insert_cache(root,wobject,wurl,wcookie,read_count);     
	 csize=csize+read_count;
       write_free();  
      }
  } 
   Close(s_fd);

      write_lock(); 
   print_cache(root);
   write_free();  
   fflush(stdout);
   
  //Read the response 
   /*
    Rio_readlineb(&s_rio,next_line,MAXLINE);
    while(strcmp(next_line,"\r\n")) {          //line:netp:readhdrs:checkterm

      sscanf(next_line,"%[^:]",header_name);
      strcat(res,next_line);
      if(!strcmp(header_name,"Content-Length"))
	{
	  sscanf(next_line+16,"%d",&length);
	  printf("matched\n");
        printf("This is the value of length %d\n",length);
	} 
      // printf("%s \n",next_line);
      con_true=Rio_readlineb(&s_rio, next_line, MAXLINE);
      if (con_true <= 0)
	return;
    }
 
    strcat(res,"\r\n");
    header_len=strlen(res);
    printf("size of header %d \n",header_len);  
   read_count=0;
  while ((con_true = rio_readnb(&s_rio,next_line,MAXLINE)) != 0 && read_count < length) { 
    memcpy(res+(read_count+header_len),next_line,con_true );
       read_count=read_count+con_true;      
   
    }
      

  printf("size of the response %d \n",read_count);  
  
  //printf("response was %s\n ",res);

  //write back to client 
  printf("client id is %d\n",c_fd); 
  if ((rio_writen(c_fd,res,(header_len+read_count))) <= 0) {
    printf("writen error.\n");
    return;
  }
   */

    
}

void *thread(void *vargp) {  
    
  printf("inside thread\n");
    
  Pthread_detach(pthread_self()); 
  while (1) { 
    int connfd = sbuf_remove(&sbuf);
     doit(connfd); 
     Close(connfd);
  }    
}

void read_lock()
{
  P(&mutex);
  readcnt++;
  if(readcnt==1)/*first in */
    P(&w);
  V(&mutex);

}

void read_free()
{
  P(&mutex);
  readcnt--;
  if(readcnt==0) /* last out */
    V(&w);
  V(&mutex);
}

void write_lock()
{
  P(&w);

}
void write_free()

{
   
  V(&w);

}

void initialize_cache()
{
  //Initialize a cache block
  root = (web_res *)malloc(sizeof(web_res ));
  if(root==NULL){
    return;
  }
  root->next=NULL;

}
