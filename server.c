#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

struct http_request {
  char method[16];
  char path[256];
  char version[16];
  char headers[16][256];
  int header_count;
  char *body;
};

atomic_int request_count = 0;

void parse_request(char *buffer, struct http_request *req) {
  char *line;
  char *buf = buffer;

  line = strsep(&buf, "\r\n");
  sscanf(line, "%15s %255s %15s", req->method, req->path, req->version);

  // get rest of headers
  int header_count = 0;
  while ((line = strsep(&buf, "\r\n")) != NULL) {
    if (strlen(line) > 0) {
      strncpy(req->headers[header_count], line,
              sizeof(req->headers[header_count]) - 1);
      req->headers[header_count][sizeof(req->headers[header_count]) - 1] = '\0';
      header_count++;
    }
  }
  req->header_count = header_count;
}

void *handle_client(void *client) {
  atomic_fetch_add(&request_count, 1);
  int clientfd = *(int *)(client);
  free(client);
  printf("[thread %lu] START client=%d\n", (unsigned long)pthread_self(),
         clientfd);

  char buf[1024];
  int bytes = recv(clientfd, buf, sizeof(buf) - 1, 0);
  if (bytes == -1) {
    perror("recv");
  } else if (bytes == 0) {
    printf("Client disconnected\n");
  } else {
    buf[bytes] = '\0';
  }

  struct http_request req;
  parse_request(buf, &req);

  printf("[thread %lu] %s %s\n", (unsigned long)pthread_self(), req.method,
         req.path);

  printf("Requests received: %d\n", atomic_load(&request_count));

  char response[2048];
  if (strcmp(req.path, "/slow") == 0) {
    printf("[thread %lu] ENTER /slow\n", (unsigned long)pthread_self());

    sleep(5);

    printf("[thread %lu] EXIT /slow\n", (unsigned long)pthread_self());

    strncpy(response, "HTTP/1.1 200 OK\r\n\r\nSlow response\n",
            sizeof(response) - 1);
    response[sizeof(response) - 1] = '\0';
  } else if (strcmp(req.path, "/fast") == 0) {
    strncpy(response, "HTTP/1.1 200 OK\r\n\r\nFast response\n",
            sizeof(response) - 1);
    response[sizeof(response) - 1] = '\0';
  }

  send(clientfd, response, strlen(response), 0);
  printf("[thread %lu] END client=%d\n", (unsigned long)pthread_self(),
         clientfd);

  close(clientfd);
  return NULL;
}

int main() {
  struct addrinfo hints;
  struct addrinfo *res;
  int backlog = 10;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int status = getaddrinfo(NULL, "8080", &hints, &res);
  if (status != 0) {
    perror("getaddrinfo");
    return 1;
  }

  int desc = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (desc == -1) {
    perror("socket");
    return 1;
  }

  if (bind(desc, res->ai_addr, res->ai_addrlen) == -1) {
    perror("bind");
    return 1;
  }

  if (listen(desc, backlog) != 0) {
    perror("listen");
    return 1;
  }
  printf("Server successfully started at :8080\n");

  while (1) {
    int clientfd = accept(desc, NULL, NULL);
    if (clientfd == -1) {
      perror("accept");
      continue;
    }
    pthread_t thread1;
    int *clientfd_ptr = malloc(sizeof(int));
    *clientfd_ptr = clientfd;
    pthread_create(&thread1, NULL, handle_client, clientfd_ptr);
    pthread_detach(thread1);
  }

  freeaddrinfo(res);
  return 0;
}
