# include "headers.h"

void read_fn(clientrequest * a)
{
    printf(PINK("Read request detected\n"));
    char filepath[BUFFER_SIZE];
    snprintf(filepath, sizeof(filepath), "%s/%s", cwd, a->path);
    // printf("%s\n", filepath);
    FILE *file = fopen(filepath, "r");
    if (file == NULL) {
        perror("fopen");
        char buffer[] = "Error opening file";
        send(a->socket, buffer, strlen(buffer), 0);
    } else {
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), file) != NULL) {
            send(a->socket, buffer, strlen(buffer), 0);
        }
        fflush(file);  // Ensure data is written
        fclose(file);
    }
}
void write_fn(clientrequest * a, int isAppend) // 0 over write , 1 : append 
{
    char buffer[MAXDATASIZE];
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s", cwd, a->path);
    ss_to_client_ack ack;
    memset(&ack, 0, sizeof(ack));
    
    FILE *file;
    if(isAppend == 1)
    {
        file = fopen(filepath, "a");
    }
    else file = fopen(filepath, "w");
    if (file == NULL) {
        snprintf(ack.ack_message, sizeof(ack.ack_message), "Error opening file: %s", a->path);
        ack.errorCode = 404;
        send(a->socket, &ack, sizeof(ack), 0);
        return;
    }

    while (1) {
        memset(buffer, 0, sizeof(buffer));

        // Receive data in chunks
        ssize_t bytes_received = recv(a->socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received < 0) {
            perror("recv error");
            break;
        }
        if (bytes_received == 0) {
            printf("Client closed the connection\n");
            break;
        }

        buffer[bytes_received] = '\0';
            printf("%s", buffer);
        // Write data to file

            if (strcmp(buffer, "END\n") == 0) {
            printf("End of transmission (EOF) received\n");
            break;  // Break the loop if EOF is received
        }


        size_t bytes_written = fwrite(buffer, 1, bytes_received, file);
        if (bytes_written < bytes_received) {
            perror("Error writing to file");
            break;
        }
    }

    fclose(file);

    // Send success acknowledgment and ensure it's sent completely
    snprintf(ack.ack_message, sizeof(ack.ack_message), "Write successful: %s", a->path);
    ack.errorCode = 0;

    size_t ack_size = sizeof(ack);
    size_t bytes_sent = 0;

    while (bytes_sent < ack_size) {
        ssize_t result = send(a->socket, ((char *)&ack) + bytes_sent, ack_size - bytes_sent, 0);
        if (result < 0) {
            perror("send error");
            break;
        }
        bytes_sent += result;
    }
}

int async_write_fn(clientrequest * a, int isAppend) // Asynchronous write function
{
    char buffer[WRITEBUFFER];
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s", cwd, a->path);
    ss_to_client_ack ack;
    memset(&ack, 0, sizeof(ack));

    char data_buffer[WRITEBUFFER];
    // Acknowledge the initiation of the write request
    // Receive data in chunks

    size_t total_bytes_received = 0;
       while (1) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_received = recv(a->socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received < 0) {
            perror("recv error");
            return 1;
        }
        if (bytes_received == 0) {
            printf("Client closed the connection\n");
            return 1;
        }

        // Check for end of transmission
        if (strcmp(buffer, "END\n") == 0) {
            printf("End of transmission (EOF) received\n");
            break;  // Break the loop if EOF is received
        }

        // Store received data in the buffer
        if (total_bytes_received + bytes_received < WRITEBUFFER) {
            memcpy(data_buffer + total_bytes_received, buffer, bytes_received);
            total_bytes_received += bytes_received;
        } else {
            printf("Buffer overflow, data will be truncated\n");
            break; // Handle buffer overflow as needed
        }
    }

    // sending the initial ack back
    snprintf(ack.ack_message, sizeof(ack.ack_message), "Writing request initiated for: %s", a->path);
    ack.errorCode = 0;
    send(a->socket, &ack, sizeof(ack), 0);
     
    // sleep(5); // sleeping for 5 sec to simulate the difference in time taken to write the file
    FILE *file;
    if(isAppend == 1)
    {
        file = fopen(filepath, "a");
    }
    else file = fopen(filepath, "w");

    // Now write the received data to the file
    if (file == NULL) {
        snprintf(ack.ack_message, sizeof(ack.ack_message), "Error opening file: %s", a->path);
        ack.errorCode = 404;
        send(a->socket, &ack, sizeof(ack), 0);
        return 1;
    }

    size_t bytes_written = fwrite(data_buffer, 1, total_bytes_received, file);
    if (bytes_written < total_bytes_received) {
        perror("Error writing to file");
    }

    fclose(file);
    return 0;
}