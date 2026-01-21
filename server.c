#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

struct http_request {
  char method[16];
  char path[256];
  char version[16];
  char headers[3][256];
  int header_count;
  char *body;
};

void parse_request(char *buffer, struct http_request *req) {
  char *line;
  char *buf = buffer;
  char *method;
  char *path;
  char *version;
  line = strsep(&buf, "\r\n");

  // get the method
  method = strsep(&line, "/");
  strncpy(req->method, method, sizeof(req->method) - 1);
  req->method[sizeof(req->method) - 1] = '\0';
  printf("Method: %s\n", req->method);

  // get path
  path = strsep(&line, " ");
  strncpy(req->path, path, sizeof(req->path) - 1);
  req->path[sizeof(req->path) - 1] = '\0';
  printf("Path: %s\n", req->path);

  // get version
  version = line;
  strncpy(req->version, version, sizeof(req->version) - 1);
  printf("Version: %s\n", version);

  // get rest of headers
  int header_count = 0;
  while ((line = strsep(&buf, "\r\n")) != NULL) {
    if (strlen(line) > 0) {
      printf("%s\n", line);
      strncpy(req->headers[header_count], line,
              sizeof(req->headers[header_count]) - 1);
      req->headers[header_count][sizeof(req->headers[header_count]) - 1] = '\0';
      header_count++;
    }
  }
  req->header_count = header_count;
}

int main() {
  struct addrinfo hints;
  struct addrinfo *res;
  int backlog = 10;

  char buffer[] = "GET /about HTTP/1.1\r\n"
                  "Host: localhost:8080\r\n"
                  "User-Agent: curl/7.68.0\r\n"
                  "Accept: */*\r\n"
                  "\r\n";

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
  int clientfd = accept(desc, NULL, NULL);
  if (clientfd == -1) {
    perror("accept");
    return 1;
  }

  char buf[1024];
  int bytes = recv(clientfd, buf, sizeof(buf) - 1, 0);
  if (bytes == -1) {
    perror("recv");
    return 1;
  } else if (bytes == 0) {
    printf("Client disconnected\n");
  } else {
    buf[bytes] = '\0';
    printf("Received: %s\n", buf);
  }

  struct http_request *req;
  parse_request(buf, req);
  close(clientfd);

  freeaddrinfo(res);
  return 0;
}
