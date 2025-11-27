#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define INI_FILE "/etc/ndcsd.conf" 
#define STOPMESSAGE ">>>STOP<<<"
#define VERSION "v.0.0.2"
#define STARTUPMESSAGE "NDCS server. v.0.0.2. (c) Roman Dmytrenko 2025. roman.webtex@gmail.com"

struct server_cfg {
    short unsigned int  server_port;
    char external_engine[256];
    char base_path[256];
};

char buffer[256] = { 0 }, *pb=buffer;

/*-----------------------------------------------------------------------------------------*/
int read_ini_file(struct server_cfg* config) {
    FILE *fp;
    char *lex;

    if ((fp=fopen(INI_FILE, "r")) == NULL ) {
        return -1;
    }
    
    while((*pb = fgetc(fp)) != '\n' || *pb != EOF) {
        if (*pb == '\n') {
            *pb = '\0';
            lex = strtok(buffer, "=");
            while(lex != NULL) {
                if(strcmp(lex, "server_port") == 0) {
                    lex = strtok(NULL, "=");
                    config->server_port = atoi(lex);
                }
                if(strcmp(lex, "external_engine") == 0) {
                    lex = strtok(NULL, "=");
                    strcpy(config->external_engine, lex);
                }
                if(strcmp(lex, "base_path") == 0) {
                    lex = strtok(NULL, "=");
                    strcpy(config->base_path, lex);
                }
                lex = strtok(NULL, "=");
            }
            buffer[0]='\0';
            pb=buffer;
        } else if(*pb == EOF) {
            break;
        } else {
            *pb++;
        }
    }
    fclose(fp);
    return 0;
}

/*-----------------------------------------------------------------------------------------*/
int searchenginehandler(int socket, struct server_cfg * conf) {
    char resbuff[256] = { 0 } ;
    char fullcommand[256] = { 0 } ;
    char* dir;
    char* rest; 
    FILE* in;
    extern FILE* popen();
    int j = 0;

    strcat(fullcommand, "cd ");
    strcat(fullcommand, conf->base_path);

    dir=strtok(buffer, "<");
    if(dir == NULL) {
        return -1;        
    }

    while(dir[j] != '\0') {
        if(dir[j] == '\\') {
            dir[j] = '/';
        }
        j++;
    }

    rest=strtok(NULL, "<");

    strcat(fullcommand, dir);
    strcat(fullcommand, "; ");
    strcat(fullcommand, conf->external_engine);
    strcat(fullcommand, " nozip ");

    j=0;
    while(rest[j] != '\0') {
        if(rest[j] == '{' || rest[j] == '}') {
            rest[j] = '"';
        }
        if(rest[j] == '\\') {
            rest[j] = '/';
        }
        j++;
    }

    strcat(fullcommand, rest);

    if( (in = popen(fullcommand, "r")) == NULL) {
        return -1;
    }

    while(fgets(resbuff, sizeof(resbuff), in) != NULL) {
        send(socket, resbuff, strlen(resbuff), 0);
    }
    buffer[0]='\0';
    pclose(in);
}

/*-----------------------------------------------------------------------------------------*/
int main(int argc, char const* argv[])
{
    int server_fd, new_socket;
    ssize_t valread;
    struct sockaddr_in serv_addr;
    int opt = 1;
    socklen_t addrlen = sizeof(serv_addr);
    struct server_cfg c_config, *p_config = &c_config;

    /* read config file */
    if (read_ini_file(p_config) != 0) {
        perror("unable to read ini file");
        exit(EXIT_FAILURE);
    }
    
    printf("%s\nRunning on %d port. Search engine - %s\n", STARTUPMESSAGE, c_config.server_port, c_config.external_engine);

    /* creating socked file descripter */
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("unable to create socked");
        exit(EXIT_FAILURE);
    }
    
    /* attach socket to port */
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("unable to set socket options");
        exit(EXIT_FAILURE);
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(c_config.server_port);

    /* attach socket to port */
    if (bind(server_fd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("unable to bind socket");
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 3) < 0) {
        perror("unable to listen");
        exit(EXIT_FAILURE);
    }
    
    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr*) &serv_addr, &addrlen )) < 0) {
            perror("unable to accept");
            exit(EXIT_FAILURE);
        }

        valread = read(new_socket, buffer, 256 - 1);
        buffer[valread]='\0';

        if (strncmp(buffer, STOPMESSAGE, strlen(STOPMESSAGE)) == 0 ) {
            printf("%s\n", "NDCS Server. By!");
            break;
        }
        
        if (searchenginehandler(new_socket, p_config) < 0) {
            perror("unable to run command");
            break;
        }
        close(new_socket);
    }
    close(server_fd);
    return 0;
}
