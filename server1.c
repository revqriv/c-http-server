#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <pthread.h>

#define CACHE_SIZE 2
#define MAX_PATH 100
#define MAX_RESPONSE 1048576

struct CacheItem{
	char path[MAX_PATH];
	char response[MAX_RESPONSE];
	int is_full;
};

struct CacheItem my_cache[CACHE_SIZE];
pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;

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

        int found_in_cache=0;

	pthread_mutex_lock(&cache_lock);
        for(int i=0;i<CACHE_SIZE;i++){
       		if(my_cache[i].is_full && strcmp(my_cache[i].path,path)==0){
                	send(client,my_cache[i].response,strlen(my_cache[i].response),0);
                        found_in_cache=1;
          	        break;
                }
        }
	pthread_mutex_unlock(&cache_lock);
        if(!found_in_cache){
        	char response[MAX_RESPONSE];
                if(strcmp(path, "/")==0){
                	FILE *f=fopen("index.html","r");
			if(f==NULL){
				char *body="<h1>500 Internal Server Errror</h1>";
				sprintf(response,"HTTP/1.1 500 Internal Server Error\r\nContent Length: %ld\r\n\r\n%s",strlen(body),body);
			}
			else{
	                        fseek(f,0,SEEK_END);
       		                long file_size=ftell(f);
                	        fseek(f,0,SEEK_SET);

				char *body=malloc(file_size+1);
	
        	                fread(body,1,file_size,f);
                	        body[file_size]='\0';
                        	fclose(f);
                     		sprintf(response, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n%s",strlen(body),body);
                        	free(body);
			}
            	}
                else if(strcmp(path,"/about")==0){
                        char *body = "<h1>About Page</h1>";
                        sprintf(response, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n%s",strlen(body),body);
                }
                else{
                        char *body = "<h1>404 Not Found</h1>";
                        sprintf(response, "HTTP/1.1 404 Not Found\r\nContent-Length: %ld\r\n\r\n%s",strlen(body),body);
               	}
		
		pthread_mutex_lock(&cache_lock);
                for(int i=0;i<CACHE_SIZE;i++){
                        if(!my_cache[i].is_full){
                               	strncpy(my_cache[i].path, path, MAX_PATH-1);
                                strncpy(my_cache[i].response, response, MAX_RESPONSE-1);
                               	my_cache[i].is_full=1;
                                break;
                        }
               	}
		pthread_mutex_unlock(&cache_lock);

                send(client, response, strlen(response), 0);
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

        for(int i=0;i<CACHE_SIZE;i++){
                my_cache[i].is_full=0;
        }

        while(1){
                int client = accept(sockfd, NULL, NULL);
		int *new_sock=malloc(sizeof(int));
		*new_sock=client;

		pthread_t thread_id;
		pthread_create(&thread_id,NULL,handle_client,(void*)new_sock);
	}
	return 0;
}
