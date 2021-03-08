#include "threadpool.h"
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h> // time now
#include <dirent.h>   //dir open and close
#include <fcntl.h>    // file open and close

#define ARG_SIZE 3
#define BUF_SIZE 4000
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define DIRECTORY 0
#define REG_FILE 1

char timebuf[128];
char modifiedBuf[128];
char *tokens[ARG_SIZE];
int newSocket = -1;
void error500();

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void freeAll()
{ // free al the memory allocation and close the socket
    for (int i = 0; i < ARG_SIZE; i++)
    {
        if (tokens[i] != NULL)
            free(tokens[i]);
    }
    if (newSocket != -1)
        close(newSocket);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void systemError(char *msg)
{ //function to print errors of system functions
    freeAll();
    perror(msg);
    printf("\n");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void *commandError()
{ //function to print errors for wrong command
    printf("Usage: server <port> <pool-size> <max-number-of-request>\n");
    freeAll();
    exit(1);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

char *currentTime()
{ //returns a string of the current date and time
    time_t now;
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    return timebuf;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

char *lastModifiedTime(char *fileName)
{ //returns a string of the last modified date and time of a file
    struct stat fs;
    if (stat(fileName, &fs) == -1)
    {
        error500();
        return NULL;
    }
    time_t now = fs.st_mtime;
    strftime(modifiedBuf, sizeof(modifiedBuf), RFC1123FMT, gmtime(&now));
    return modifiedBuf;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

char *get_mime_type(char *name)
{ //returns a string with the type of the file
    char *ext = strrchr(name, '.');
    if (!ext)
        return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
        return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(ext, ".gif") == 0)
        return "image/gif";
    if (strcmp(ext, ".png") == 0)
        return "image/png";
    if (strcmp(ext, ".css") == 0)
        return "text/css";
    if (strcmp(ext, ".au") == 0)
        return "audio/basic";
    if (strcmp(ext, ".wav") == 0)
        return "audio/wav";
    if (strcmp(ext, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0)
        return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0)
        return "audio/mpeg";
    return NULL;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

char *buildResponse(char *status, char *phrase, int bodyLength)
{ // build a string that contains the response to the client without the body
    char *response = (char *)calloc(1000, sizeof(char));
    char *version = "HTTP/1.1 ";
    strcat(response, version);
    strcat(response, status);
    strcat(response, " ");
    strcat(response, phrase);
    strcat(response, "\r\n");
    strcat(response, "Server: webserver/1.0\r\n");
    strcat(response, "Date: ");

    char *date = currentTime();
    strcat(response, date);
    strcat(response, "\r\n");
    if (strcmp(status, "302") == 0)
    {
        strcat(response, "Location: ");
        strcat(response, tokens[1]);
        strcat(response, "/");
        strcat(response, "\r\n");
    }
    if (strcmp((status), "200") != 0) // if its an error
        strcat(response, "Content-Type: text/html\r\n");
    else
    { // only when the body is a file or content of a directory
        if (tokens[1] != NULL)
        {
            char *fileName = tokens[1];
            if (strcmp(tokens[1], "./") == 0)
                fileName = "index.html";
            char *type = get_mime_type(fileName);
            if (type != NULL)
            {
                strcat(response, "Content-Type: ");
                strcat(response, type);
                strcat(response, "\r\n");
            }
        }
        else
        {
            strcat(response, "Content-Type: text/html\r\n");
        }
    }
    char body_len[50];
    sprintf(body_len, "%d", bodyLength);
    strcat(response, "Content-Length: ");
    strcat(response, body_len);
    strcat(response, "\r\n");
    if (strcmp(status, "200") == 0)
    {
        char *last_modified = lastModifiedTime(tokens[1]);
        strcat(response, "Last-Modified: ");
        strcat(response, last_modified);
        strcat(response, "\r\n");
    }
    strcat(response, "Connection: close\r\n\r\n");
    return response;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void error302()
{ // 302 Found
    char *body = "<HTML><HEAD><TITLE>302 Found</TITLE></HEAD>\r\n"
                 "<BODY><H4>302 Found</H4>\r\n"
                 "Directories must end with a slash.\r\n"
                 "</BODY></HTML>\r\n";
    char *res = buildResponse("302", "Found", strlen(body));
    strcat(res, body);
    write(newSocket, res, strlen(res) + 1);
    freeAll();
    free(res);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void error400()
{ // 400 Bad Request
    char *body = "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\r\n"
                 "<BODY><H4>400 Bad request</H4>\r\n"
                 "Bad Request.\r\n"
                 "</BODY></HTML>\r\n";
    char *res = buildResponse("400", "Bad Request", strlen(body));
    strcat(res, body);
    write(newSocket, res, strlen(res) + 1);
    freeAll();
    free(res);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void error403()
{ // 403 Forbidden
    char *body = "<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\r\n"
                 "<BODY><H4>403 Forbidden</H4>\r\n"
                 "Access denied.\r\n"
                 "</BODY></HTML>\r\n";
    char *res = buildResponse("403", "Forbidden", strlen(body));
    strcat(res, body);
    write(newSocket, res, strlen(res) + 1);
    freeAll();
    free(res);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void error404()
{ // 404 Not Found
    char *body = "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\r\n"
                 "<BODY><H4>404 Not Found</H4>\r\n"
                 "File not found.\r\n"
                 "</BODY></HTML>\r\n";
    char *res = buildResponse("404", "Not Found", strlen(body));
    strcat(res, body);
    write(newSocket, res, strlen(res) + 1);
    freeAll();
    free(res);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void error500()
{ // 500 Internal Server Error
    char *body = "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\r\n"
                 "<BODY><H4>500 Internal Server Error</H4\r\n>"
                 "Some server side error.\r\n"
                 "</BODY></HTML>\r\n";
    char *res = buildResponse("500", "Internal Server Error", strlen(body));
    strcat(res, body);
    write(newSocket, res, strlen(res) + 1);
    freeAll();
    free(res);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void error501()
{ // 501 Not supported
    char *body = "<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>\r\n"
                 "<BODY><H4>501 Not supported</H4>\r\n"
                 "Method is not supported.\r\n"
                 "</BODY></HTML>\r\n";
    char *res = buildResponse("501", "Not supported", strlen(body));
    strcat(res, body);
    write(newSocket, res, strlen(res) + 1);
    freeAll();
    free(res);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

char *substring(char *s, int start, int end)
{ // this function return a sub string of s from start to end indexes
    if (start == end)
        return NULL;
    char *ret = (char *)calloc(end - start + 5, sizeof(char)); // allocate memory
    if (ret == NULL)
    {
        error500();
        return NULL;
    }
    int i = start, j = 0;
    for (; i < end; i++, j++)
        ret[j] = s[i]; //copy the necessary chars
    ret[j] = '\0';
    return ret;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int splitTheCommand(char *command)
{ // split the command into a char**

    int start = 0, end = 0, counter = 0, idx = 0;
    for (int i = 0; i <= strlen(command); i++)
    { // counts how
        if (command[i] == ' ' || i == strlen(command))
        {
            counter++;
        }
    }
    if (counter != 3)
    {
        error400();
        return -1;
    }

    for (int i = 0; i <= strlen(command); i++)
    { // split the command to different strings
        if (command[i] == ' ' || i == strlen(command))
        { // space between words or end of command are the sign to substring
            start = end;
            end = i;
            tokens[idx] = substring(command, start, end);
            idx++;
            end++; // without the space between the words
        }
    }
    return 0;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int checkValidTokens()
{ //check the validity of the first and last tokens
    if (tokens[1][0] != '/')
    {
        error400();
        return -1;
    }
    if (strcmp("GET", tokens[0]) != 0)
    {
        error501();
        return -1;
    }
    if (strcmp("HTTP/1.1", tokens[2]) != 0 && strcmp("HTTP/1.0", tokens[2]) != 0)
    {
        error400();
        return -1;
    }
    if (strstr(tokens[1], "//") != NULL)
    {
        error400();
        return -1;
    }

    return 0;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int numberOfFilesInDir(char *path)
{
    // counts how many files there are in the directory to know the size of the buf e will create
    DIR *dir = opendir(path);
    struct dirent *dentry;
    struct stat fs;
    if (stat(path, &fs) == -1)
    {
        closedir(dir);
        error404();
    }
    int count = 0;
    if (S_IROTH & fs.st_mode)
    {
        while ((dentry = readdir(dir)) != NULL)
        {
            count++;
        }
    }
    closedir(dir);
    return count;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int checkPermissions(char *path)
{ // this function checks if a certain path has the right permissions
    struct stat fs;
    stat(path, &fs);
    int type = 0;
    if (S_ISDIR(fs.st_mode))
        type = DIRECTORY;
    else if (S_ISREG(fs.st_mode))
        type = REG_FILE;
    char fullPath[BUF_SIZE]; // we comapre to full path to know if we arrived to the full path received
    char copyPath[BUF_SIZE]; // copy the path and cut it every time

    strcpy(fullPath, path);
    strcpy(copyPath, path);

    char *permissions = strtok(copyPath, "/");
    char *check = calloc(strlen(path) + BUF_SIZE, sizeof(char));
    if (check == NULL)
    {
        error500();
        return -1;
    }

    while (permissions != NULL)
    {
        if (type == DIRECTORY) // if its a directory add / all the time
        {
            strcat(check, permissions);
            strcat(check, "/");
        }
        else if (type == REG_FILE) // if the path is a file we dont need a / in the end
        {
            strcat(check, permissions);
            if (strcmp(check, fullPath) != 0)
            {
                strcat(check, "/");
            }
        }
        if (strcmp(check, fullPath) == 0)
        { // this is the file/ dir requested (last part of the path added), check r permmisions
            stat(fullPath, &fs);
            if (!(S_IROTH & fs.st_mode))
            {
                error403();
                free(check);
                return -1;
            }
        }
        else if (stat(check, &fs) == -1)
        {
            free(check);
            error404();
            return -1;
        }
        else if (!(S_IXOTH & fs.st_mode)) // if the dir does not have a x permissions
        {
            free(check);
            error403();
            return -1;
        }
        permissions = strtok(NULL, "/"); // next dir
    }
    free(check);
    return 0;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int buildDirContent(char *path)
{
    // this function bulid the html file that contain the contents of the directory
    DIR *dir = opendir(path);
    struct dirent *dentry;
    struct stat fs;
    if (stat(path, &fs) == -1)
    {
        closedir(dir);
        error404();
    }
    int size = numberOfFilesInDir(path);
    if (S_IROTH & fs.st_mode)
    {
        char *buf = calloc(size * 1500, sizeof(char));
        char *res = buildResponse("200", "OK", BUF_SIZE);
        strcpy(buf, res);
        strcat(buf, "<HTML>\r\n<HEAD><TITLE>Index of ");
        strcat(buf, path);
        strcat(buf, "</TITLE></HEAD>\r\n\r\n<BODY>\r\n<H4>Index of ");
        strcat(buf, path);
        strcat(buf, "</H4>\r\n\r\n");
        strcat(buf, "<table CELLSPACING=8>\r\n<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n\r\n");

        while ((dentry = readdir(dir)) != NULL)
        { // for every file add the details
            strcat(buf, "<tr>\r\n<td><A HREF=\"");
            strcat(buf, dentry->d_name);
            char *newPath = calloc(strlen(path) + strlen(dentry->d_name) + 1, sizeof(char));
            strcpy(newPath, path);
            strcat(newPath, dentry->d_name);

            if (stat(newPath, &fs) != 0)
            { // stat for the specific file checked now
                free(newPath);
                closedir(dir);
                error404();
                return -1;
            }
            if (S_ISDIR(fs.st_mode))
            { // if its dir add /
                strcat(buf, "/\">");
            }
            else
            {
                strcat(buf, "\">");
            }
            strcat(buf, dentry->d_name);
            strcat(buf, "</A></td><td>");

            char *time = lastModifiedTime(newPath);
            if (time == NULL)
                return -1;
            strcat(buf, time);
            strcat(buf, "</td>\r\n");
            free(newPath);
            if (S_ISREG(fs.st_mode)) // if it is a file
            {
                strcat(buf, "<td>");
                char size[15];
                sprintf(size, "%ld", fs.st_size);
                strcat(buf, size);
                strcat(buf, " bytes</td>\r\n");
            }

            strcat(buf, "</tr>\r\n\r\n");
        }
        strcat(buf, "</table>\r\n\r\n<HR>\r\n\r\n<ADDRESS>webserver/1.0</ADDRESS>\r\n\r\n</BODY></HTML>\r\n");

        write(newSocket, buf, strlen(buf) + 1);
        free(buf);
        free(res);
        freeAll();
        close(newSocket);
    }
    else
    {
        error403();
        return -1;
    }
    closedir(dir);
    return 0;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int bringFile(char *path, struct stat fs_path)
{ // this function read the asked file and returns it to the client
    if (checkPermissions(path) != 0)
    { // if the path doesn't have the right permissions
        return -1;
    }
    FILE *output_file;
    output_file = fopen(path, "r");
    if (output_file == NULL)
    {
        fclose(output_file);
        error404();
        return -1;
    }
    unsigned char *buf = (unsigned char *)calloc(fs_path.st_size + 1, sizeof(char));
    fread(buf, fs_path.st_size, 1, output_file);
    char *res = buildResponse("200", "OK", fs_path.st_size);
    write(newSocket, res, strlen(res));
    free(res);
    write(newSocket, buf, fs_path.st_size);
    free(buf);
    freeAll();
    fclose(output_file);
    return 0;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int checkIfDir(struct stat fs_path)
{ // if its a dir, check if the file index.html exist in the dir, if not build one
    if (checkPermissions(tokens[1]) != 0)
    { // if the path doesn't have the right permissions
        return -1;
    }
    int length = strlen(tokens[1]);
    if (tokens[1][length - 1] != '/')
    {
        error302();
        return -1;
    }
    char *path = tokens[1];
    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        closedir(dir);
        error500();
    }
    struct dirent *dentry;
    while ((dentry = readdir(dir)) != NULL)
    {
        if (strcmp(dentry->d_name, "index.html") == 0)
        { // if the file exist in the file
            struct stat fs_file;
            char *newPath = calloc(strlen(path) + 12, sizeof(char));
            strcpy(newPath, path);
            strcat(newPath, "index.html");
            if (stat(newPath, &fs_file) == -1)
            {
                closedir(dir);
                free(newPath);
                error404();
                return -1;
            }
            else if ((S_IROTH & fs_file.st_mode) && (S_IXOTH & fs_path.st_mode)) // check x permissions of the path and r permissions of the file
            {
                bringFile(newPath, fs_path);
                free(newPath);
                closedir(dir);
                return 0;
            }
            else
            { // does not have the right permissions
                error403();
                free(newPath);
                closedir(dir);
                return -1;
            }
        }
    }
    closedir(dir);
    buildDirContent(path); // if its a dir but the file index.html does not exist we need to build it

    return 0;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int pathAnalysis(char *path)
{
    // this function analysis the path given and returns the requested file
    struct stat fs;
    char *newPath = calloc((strlen(path) + 2), sizeof(char));

    strcpy(newPath, "."); // fix the path
    strcat(newPath, path);

    free(tokens[1]);
    tokens[1] = newPath;

    if (stat(newPath, &fs) == -1)
    {
        error404();
        return -1;
    }
    if (S_ISDIR(fs.st_mode))
    { /*directory*/
        if (checkIfDir(fs) != 0)
        {
            return -1;
        }
    }
    else if (S_ISREG(fs.st_mode) && fs.st_size != 0)
    { /*regular file*/
        if (bringFile(newPath, fs) != 0)
            return -1;
    }
    else if (!S_ISREG(fs.st_mode) && !S_ISDIR(fs.st_mode))
    { /* not a regular file*/
        error403();
        return -1;
    }
    return 0;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int handleClient(void *ptr)
{ //the function that the thread sent to
    newSocket = *((int *)ptr);
    char buf[BUF_SIZE];
    memset(buf, '\0', BUF_SIZE);
    read(newSocket, buf, BUF_SIZE);
    strtok(buf, "\r\n"); // keep the first line only

    if (splitTheCommand(buf) != 0) // split the command to array of strings
        return 0;
    if (checkValidTokens() != 0) // check the validity of the command
        return 0;
    if (pathAnalysis(tokens[1]) != 0) // analysis the path and return the file requested
        return 0;
    close(newSocket);

    return 0;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int main(int argc, char *argv[])
{
    if( argc < 4)
        commandError();
    int port = atoi(argv[1]);
    if (port < 0)
        commandError();
    int thread_num = atoi(argv[2]);
    if (thread_num < 0)
        commandError();
    int missions_num = atoi(argv[3]);
    if (missions_num < 0)
        commandError();

    int *arr = (int *)calloc(missions_num, sizeof(int)); // array for the threads of the missions
    for (int i = 0; i < missions_num; i++)
    {
        arr[i] = i;
    }
    threadpool *tp = create_threadpool(thread_num);
    int welcome_socket_fd;
    /* socket descriptor */
    if ((welcome_socket_fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
    {
        systemError("socket");
        destroy_threadpool(tp);
        free(arr);
        exit(1);
    }
    struct sockaddr_in srv; /* used by bind() */

    srv.sin_family = AF_INET;            /* use the Internet addr family */
    srv.sin_port = htons(atoi(argv[1])); /* bind socket ‘fd’ to port */
    srv.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(welcome_socket_fd, (struct sockaddr *)&srv, sizeof(srv)) < 0)
    {
        systemError("bind");
        destroy_threadpool(tp);
        free(arr);
        exit(1);
    }

    if (listen(welcome_socket_fd, 5) < 0)
    {
        systemError("listen");
        destroy_threadpool(tp);
        free(arr);
        exit(1);
    }

    if (tp != NULL)
    {
        for (int i = 0; i < missions_num; i++)
        {
            arr[i] = accept(welcome_socket_fd, NULL, NULL);
            if (arr[i] < 0)
            {
                systemError("accept");
                destroy_threadpool(tp);
                free(arr);
                exit(1);
            }
            dispatch(tp, handleClient, &arr[i]);
        }
        destroy_threadpool(tp);
    }
    free(arr);
    return 0;
}
