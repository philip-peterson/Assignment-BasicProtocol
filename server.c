#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <strings.h>
#include <memory.h>

#define BACKLOG_SIZE 5
#define BUFFER_SIZE 1024
#define CMD_BUF_SIZE 256
#define MAX_POSSIBLE_OPERANDS 4

#define ERROR_NOSOCK 0
#define ERROR_NOBIND 1
#define ERROR_LISTEN 2
#define ERROR_ACCEPT 3

#define SERVER_ERROR_INVALCMD "-1"
#define SERVER_ERROR_TOOFEWARG "-2"
#define SERVER_ERROR_TOOMANYARG "-3"
#define SERVER_ERROR_INVALINPUT "-4"
#define SERVER_ERROR_EXIT "-5"

#define SERVER_ERROR_OVERFLOW "-6"

#define WRITE2SOCK(txt) \
      if(write(res, txt, strlen(txt)) < 0) { \
         fprintf(stderr, "Connection closed non-gracefully (write error!).\n"); \
         goto next_connection; \
      };

void error(int err) {
   const char *msg;
   switch(err) {
      case ERROR_NOSOCK:
         msg = "Could not create socket";
         break;
      case ERROR_NOBIND:
         msg = "Could not bind socket";
         break;
      case ERROR_LISTEN:
         msg = "Could not listen to socket";
         break;
      case ERROR_ACCEPT:
         msg = "Error in accepting connection";
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

int identifyCommand(char* string) {
   int command = -1;

   if (strstr(string, "add ") == string || strcmp(string, "add") == 0) {
      command = 0;
   }
   else if (strstr(string, "subtract ") == string || strcmp(string, "subtract") == 0) {
      command = 1;
   }
   else if (strstr(string, "multiply ") == string || strcmp(string, "multiply") == 0) {
      command = 2;
   }
   else if (strstr(string, "bye ") == string || strcmp(string, "bye") == 0) {
      command = 3;
   }


   return command;
}

int argsAreValid(char* string, char** argptrs, int numargs) {
   if (numargs == 0) {
      return 1;
   }

   int l = strlen(string);
   int i;
   for(i = argptrs[0]-string; i < l; i++) {
      if (!(string[i] == ' ' || isdigit(string[i]) )) {
         return 0;
      }
   }

   for ( i = 1; i < numargs; i++) {
      if (argptrs[i] == argptrs[i-1]) {
         return 0; // encountered zero-length argument
      }
   }
   return 1;
}

int cmd(int operation, char* string, char** argptrs, int numargs) {
   int i;
   int total = 0;

   int n = strlen(string);
   for(i = 0; i < numargs; i++) {
      size_t sz = ((i == numargs-1) ? (string+n) : (argptrs[i+1])) - argptrs[i] + 1;
      char* x = malloc( sz );
      x[sz-1] = 0;
      memcpy(x, argptrs[i], sz);

      if (i == 0) {
         total = atoi(x);
      }
      else {
         if (operation == 0) {
            total += atoi(x);
         }
         else if (operation == 1) {
            total -= atoi(x);
         }
         else if (operation == 2) {
            total *= atoi(x);
         }
      }
   }
   return total;
}

const char* handleCommand(char* output, char* buffer, int buffer_size) {
   // Search the buffer for \0. If it is found, we have an invalid command, due
   // to the specs. If not, we can proceed, turning buffer into a string.

   int i;
   for (i=0; i<buffer_size; i++) {
      if (buffer[i] == '\0') {
         strcpy(output, SERVER_ERROR_INVALCMD);
         return;
      }
   }

   char* string = malloc(sizeof(char)*(buffer_size+1));
   memcpy(string, buffer, buffer_size);
   string[buffer_size] = 0x00;

   char** argptrs = malloc(sizeof(char*) * MAX_POSSIBLE_OPERANDS);
   for (i = 0; i < MAX_POSSIBLE_OPERANDS; i++) {
      argptrs[i] = NULL;
   }
   
   int numargs = 0;

   char* cur = string; 
   for (; cur < string+buffer_size; cur++) {
      if (cur[0] == ' ') {
         if (numargs == MAX_POSSIBLE_OPERANDS) {
            strcpy(output, SERVER_ERROR_TOOMANYARG);
            return;
         }
         else {
            argptrs[numargs] = cur;
            numargs++;
         }
      }
   }

   int command = identifyCommand(string);
   if (command < 0) {
      strcpy(output, SERVER_ERROR_INVALCMD);
      return;
   }

   if (command == 3) {
      strcpy(output, SERVER_ERROR_EXIT);
      return;
   }

   if (numargs < 2) {
      strcpy(output, SERVER_ERROR_TOOFEWARG);
      return;
   }

   if (!argsAreValid(string, argptrs, numargs)) {
      strcpy(output, SERVER_ERROR_INVALINPUT);
      return;
   }

   const char* pool = malloc(32);

   sprintf(output, "%d", cmd(command, string, argptrs, numargs));
   return;
}


int main(int argc, const char * const * argv) {

   if (argc != 2) {
      fprintf(stderr, "Usage: server PORT\n");
      exit(100);
   }

   sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (sockfd < 0) {
      error(ERROR_NOSOCK);
   }
   atexit(cleanup);
   signal(SIGPIPE, SIG_IGN);

   struct sockaddr_in serv_addr;
   bzero((void *) &serv_addr, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(atoi(argv[1]));

   if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
      error(ERROR_NOBIND);
   }

   if (listen(sockfd, BACKLOG_SIZE) != 0) {
      error(ERROR_LISTEN);
   }

   struct sockaddr client_addr;
   socklen_t addrlen = sizeof(client_addr);

   char *buf = malloc(sizeof(char)*BUFFER_SIZE);

   char *cmd_buf = malloc(sizeof(char)*CMD_BUF_SIZE);
   int cmd_buf_ctr = 0;


   next_connection:
   while(1) {

      int res = accept(sockfd, &client_addr, &addrlen);

      if (res < 0) {
         error(ERROR_ACCEPT);
      }

      char address[INET_ADDRSTRLEN];

      if (inet_ntop(AF_INET, &(((struct sockaddr_in*)&client_addr)->sin_addr), address, INET_ADDRSTRLEN) == NULL) {
         fprintf(stdout, "get connection from client...\n");
      }
      else {
         fprintf(stdout, "get connection from %s...\n", address);
      }

      WRITE2SOCK("Hello!\n");

      /* This flag is set to 1 if the client sends a command that is larger than CMD_BUF_SIZE.
       * After CMD_BUF_SIZE is reached, we ignore bytes until we encounter a '\n', at which
       * point the flag is set back to 0.
       */
      int ignoring_rest_of_command = 0;

      while (1) {
         int bytes = read(res, buf, BUFFER_SIZE);
         if (bytes == 0) {
            fprintf(stdout, "Connection closed gracefully.\n");
            goto next_connection;
         }
         if (bytes < 0) {
            fprintf(stderr, "Connection closed non-gracefully.\n");
            goto next_connection;
         }

         int i;

         for (i = 0; i < bytes; i++) {

            if (ignoring_rest_of_command) {
               if (buf[i] == '\n') {
                  ignoring_rest_of_command = 0;
               }
               continue;
            }

            if (buf[i] == '\n') {
               char *result = malloc(256);
               bzero((void *) result, 256);
               handleCommand(result, cmd_buf, cmd_buf_ctr);

               printf("get: ");
               int j;
               for (j = 0; j < cmd_buf_ctr; j++) {
                  fputc(cmd_buf[j], stdout);
               }
               printf(", return: %s\n", result);
               fflush(stdout);

               cmd_buf_ctr = 0;
               WRITE2SOCK(result);
               WRITE2SOCK("\n");
               fsync(res);
               if (strcmp(result, SERVER_ERROR_EXIT) == 0) {
                  free(result);
                  close(res);
                  goto next_connection;
               }
               free(result);
            }
            else {
               cmd_buf[cmd_buf_ctr] = buf[i];
               cmd_buf_ctr++;
               if (cmd_buf_ctr >= CMD_BUF_SIZE) {
                  cmd_buf_ctr = 0;
                  ignoring_rest_of_command = 1;
                  WRITE2SOCK(SERVER_ERROR_OVERFLOW "\n");
               }
            }

         }

      }
   }

   return 0;
}
