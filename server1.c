#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <pthread.h>

#define N_BUCKETS 128
#define CACHE_SIZE 64
#define MAX_PATH 100
#define MAX_RESPONSE 1048576

static pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct Entry{
	char key[MAX_PATH];
	char *resp;
	size_t len;
	struct Entry* prev;
	struct Entry* next;
	struct Entry* hnext;
}Entry;

static Entry* buckets[N_BUCKETS];
static Entry* head;
static Entry* tail;
static int count=0;

unsigned hash(const char* s){ //hash function to get hash value for key
	unsigned h=5381;
	while(*s){
		h=h*33 + (unsigned char)*s;
		s++;
	}
	return h%N_BUCKETS;
}

static void remove_entry(Entry* e){
	if(e->prev) e->prev->next=e->next;
	else head=e->next;
	if(e->next) e->next->prev=e->prev;
	else tail=e->prev;
	e->prev=e->next=NULL;
}

static void push_top(Entry* e){
	e->prev=NULL;
	e->next=head;
	if(head){
		head->prev=e;
	}
	head=e;
	if(!tail) tail=e;
}

static Entry* find(const char* key){
	for(Entry* e=buckets[hash(key)];e;e=e->hnext){
		if(strcmp(e->key,key)==0) return e;
	}
	return NULL;
}

static void hash_remove(Entry* e){ 
	Entry **p=&buckets[hash(e->key)];
	while(*p && *p!=e) p=&(*p)->hnext;
	if(*p) *p=e->hnext;
}

size_t cache_get(const char *key,char *copy,size_t cap){
	size_t n=0;
	Entry *e=find(key);
	pthread_mutex_lock(&cache_lock);
	if(e && e->len<=cap){
		memcpy(copy,e->resp,e->len);
		n=e->len;
		remove_entry(e);
		push_top(e);
	}
	pthread_mutex_unlock(&cache_lock);
	return n;
}

void cache_put(const char *key,const char* resp, size_t len){
	Entry* e=malloc(sizeof(Entry));
	char* copy=malloc(len);
	if(!e || !copy){
		free(e);
		free(copy);
		return;
	}
	memcpy(copy,resp,len);
	strncpy(e->key,key,MAX_PATH-1);
	e->key[MAX_PATH-1]='\0';
	e->resp=copy;
	e->len=len;
	e->prev=e->next=e->hnext=NULL;

	pthread_mutex_lock(&cache_lock);
	if(find(key)){
		pthread_mutex_unlock(&cache_lock);
		free(e);
		free(copy);
		return;
	}

	unsigned h=hash(key);
	e->hnext=buckets[h];
	buckets[h]=e;
	push_top(e);
	count++;
	Entry* victim=NULL;
	if(count>CACHE_SIZE){
		victim=tail;
		remove_entry(victim);
		hash_remove(victim);
		count--;
	}
	pthread_mutex_unlock(&cache_lock);
	if(victim){
		free(victim->resp);
		free(victim);
	}
}

void *handle_client(void* client_ptr){
	int client=*(int *)client_ptr;
	free(client_ptr);
	
	pthread_detach(pthread_self());

	char buffer[1024] = {0};
    recv(client, buffer, sizeof(buffer), 0);
    char method[10], path[MAX_PATH];
    sscanf(buffer, "%9s %99s", method, path);
    //printf("method: %s, path: %s\n", method, path);

    if(strlen(path)==0){ //to ignore backgrnd stuff,empty path
		close(client);
    	return NULL;
    }
    char response[MAX_RESPONSE];
	int n=0;

	size_t hit=cache_get(path,response,sizeof(response));
	if(hit){
		send(client,response,hit,0);
		close(client);
		return NULL;
	}
    if(strcmp(path, "/")==0){
        FILE *f=fopen("index.html","r");
	
		if(f==NULL){
			char *body="<h1>500 Internal Server Errror</h1>";
			n=snprintf(response,MAX_RESPONSE,"HTTP/1.1 500 Internal Server Error\r\nContent Length: %zu\r\n\r\n%s",strlen(body),body);
		}
		else{
			fseek(f,0,SEEK_END);	        
			long file_size=ftell(f);
    	    fseek(f,0,SEEK_SET);

			char *body=malloc(file_size+1);
	
        	fread(body,1,file_size,f);
 	        body[file_size]='\0';
    	    fclose(f);
	        n=snprintf(response,MAX_RESPONSE, "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n%s",strlen(body),body);
			free(body);
		}
    }
    else if(strcmp(path,"/about")==0){
        char *body = "<h1>About Page</h1>";
        n=snprintf(response,MAX_RESPONSE, "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n%s",strlen(body),body);
    }
    else{
        char *body = "<h1>404 Not Found</h1>";
    	n=snprintf(response,MAX_RESPONSE, "HTTP/1.1 404 Not Found\r\nContent-Length: %zu\r\n\r\n%s",strlen(body),body);
    }

	send(client, response, n, 0);
	if(strncmp(response,"HTTP/1.1 200",12)==0){
		cache_put(path,response,n);
	}
    close(client);
	return NULL;
}



int main() {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);

        int opt=1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(8080);
        addr.sin_addr.s_addr = INADDR_ANY;

        bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
        listen(sockfd, 10);

        printf("listening on port 8080\n");

        while(1){
                int client = accept(sockfd, NULL, NULL);
		int *new_sock=malloc(sizeof(int));
		*new_sock=client;

		pthread_t thread_id;
		pthread_create(&thread_id,NULL,handle_client,(void*)new_sock);
	}
	return 0;
}
