#include <stdio.h>
#include <stdlib.h>


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <strings.h>
#include <memory.h>

#define BACKLOG_SIZE 5
#define BUFFER_SIZE 1024
#define CMD_BUF_SIZE 256
#define MAX_POSSIBLE_OPERANDS 4

#define ERROR_NOSOCK 0
#define ERROR_HOSTRESOLV 1
#define ERROR_CONNECT 2

void error(int err) {
   const char *msg;
   switch(err) {
      case ERROR_NOSOCK:
         msg = "Could not create socket";
         break;
      case ERROR_HOSTRESOLV:
         msg = "Could not resolve hostname";
         break;
      case ERROR_CONNECT:
         msg = "Could not connect to server";
         break;
      default:
         msg = "Unknown error";
         break;
   }

   fprintf(stderr, msg);
   fprintf(stderr, "\n");
   
   exit(err);
}

int sockfd = -1;

void cleanup() {
   if (sockfd < 0) {
      return;
   }

   close(sockfd);
}


int main(int argc, const char * const * argv) {

   if (argc != 3) {
      fprintf(stderr, "Usage: client SERVER PORT\n");
      exit(100);
   }

   sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (sockfd < 0) {
      error(ERROR_NOSOCK);
   }
   atexit(cleanup);
   
   struct hostent *host;
   host = gethostbyname(argv[1]);
   
   if (host == NULL) {
      error(ERROR_HOSTRESOLV);
   }

   struct sockaddr_in serv_addr;
   bzero((char *) &serv_addr, sizeof(serv_addr));

   serv_addr.sin_family = AF_INET;
   serv_addr.sin_port = htons(atoi(argv[2]));

   bcopy(
      (void*) host->h_addr,
      (void*) &serv_addr.sin_addr.s_addr,
      host->h_length
   );
   serv_addr.sin_addr.s_addr = INADDR_ANY;

   if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
      error(ERROR_CONNECT);
   }

   pid_t res = fork();

   if (res == 0) {
      // child
      char *lineptr;
      size_t n;
      while(1) {
         lineptr = NULL;
         getline(&lineptr, &n, stdin); 
         write(sockfd, lineptr, strlen(lineptr));
         fsync(sockfd);
         free(lineptr);
      }
   }

   char* cumulative_buffer = malloc(256);
   cumulative_buffer[255] = 0;
   int cumulative_buffer_ctr = 0;

   char* buffer = malloc(256);
   buffer[255] = 0;

   while(1) {
      bzero(buffer, 256);
      int code = read(sockfd, buffer, 255);
      if (code == 0) {
         exit(0);
      }
      if (code < 0) {
         perror("Error: \n");
      }
      if (code > 0) {
         int i;
         for (i = 0; i < code; i++) {
            if (buffer[i] == '\n') {
               cumulative_buffer[cumulative_buffer_ctr] = 0;
               cumulative_buffer_ctr = 0;

               if(strcmp(cumulative_buffer, "-1") == 0) {
                  printf("Receive: incorrect operation command.\n");
               }
               else if(strcmp(cumulative_buffer, "-2") == 0) {
                  printf("Receive: number of inputs is less than two.\n");
               }
               else if(strcmp(cumulative_buffer, "-3") == 0) {
                  printf("Receive: number of inputs is more than four.\n");
               }
               else if(strcmp(cumulative_buffer, "-4") == 0) {
                  printf("Receive: one or more of the inputs contain(s) non-number(s).\n");
               }
               else if(strcmp(cumulative_buffer, "-5") == 0) {
                  printf("Receive: exit.\n");
               }
               else if(strcmp(cumulative_buffer, "-6") == 0) {
                  printf("Receive: command was too long.\n");
               }
               else {
                  printf("Receive: %s\n", cumulative_buffer);
               }

               fflush(stdout);

            }
            else {
               cumulative_buffer[cumulative_buffer_ctr] = buffer[i];
               cumulative_buffer_ctr++;
            }
         }
      }
   }


   return 0;
}
