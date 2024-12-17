#include "headers.h"

const char* getRequestTypeString(int request_type) {
    switch (request_type) {
        case CONNECT: return "CONNECT";
        case READ: return "READ";
        case WRITE: return "WRITE";
        case DELETE: return "DELETE";
        case GETINFO: return "GETINFO";
        case STREAM: return "STREAM";
        case CREATE: return "CREATE";
        case COPY: return "COPY";
        case LIST: return "LIST";
        default: return "UNKNOWN";
    }
}

const char* getErrorCodeString(int error_code) {
    switch (error_code) {
        case SUCCESS: return "SUCCESS";
        case INVALID_PATH: return "INVALID_PATH";
        case FILE_ALREADY_EXISTS: return "FILE_ALREADY_EXISTS";
        case FILE_NOT_FOUND: return "FILE_NOT_FOUND";
        case STORAGE_SERVER_NOT_FOUND: return "STORAGE_SERVER_NOT_FOUND";
        case CLIENT_SERVER_DISCONNECTED: return "CLIENT_SERVER_DISCONNECTED";
        case ACCESS_DENIED: return "ACCESS_DENIED";
        case FILE_NOT_CREATED: return "FILE_NOT_CREATED";
        case FILE_NOT_DELETED: return "FILE_NOT_DELETED";
        case TIMEOUT: return "TIMEOUT";
        default: return "UNKNOWN_ERROR";
    }
}

void logs(int server, int ss_id, int ss_port_or_client_socket, int request_type, char* request_data, int error_code)
{
    FILE* fptr = fopen("log.txt" , "a");
    getloglock();
    if (fptr == NULL)
    {
        fprintf(stderr, RED("fopen : %s\n"), strerror(errno));
        return;
    }

    if (server == 1)
    {
        fprintf(fptr, "Communicating with Storage Server : %d\n", ss_id);
        fprintf(fptr, "NFS Port number                   : %d\n", NS_PORT);
        fprintf(fptr, "Storage Server Port number        : %d\n", ss_port_or_client_socket);
        fprintf(fptr, "Request type                      : %s\n", getRequestTypeString(request_type));
        fprintf(fptr, "Request data                      : %s\n", request_data);
        fprintf(fptr, "Status                            : %s\n", getErrorCodeString(error_code));
        fprintf(fptr, "\n");
    }
    else
    {
        fprintf(fptr, "Communicating with Client\n");
        fprintf(fptr, "NFS Port number                   : %d\n", NS_PORT);
        fprintf(fptr, "Client Port number                : %d\n", ss_port_or_client_socket);
        fprintf(fptr, "Request type                      : %s\n", getRequestTypeString(request_type));
        fprintf(fptr, "Request data                      : %s\n", request_data);
        fprintf(fptr, "Status                            : %s\n", getErrorCodeString(error_code));
        fprintf(fptr, "\n");
    }
    releaseloglock();

    fclose(fptr);

    return;
}