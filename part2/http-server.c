/*
 * tcp-recver.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
static void die(const char *s) { perror(s); exit(1); }

int main(int argc, char **argv)
{
    if (argc != 5) {
        fprintf(stderr, "usage: %s <server-port> <web-root> <mdb-lookup host> <mdb-lookup port>\n", argv[0]);
        exit(1);
    }

    //unsigned short port = atoi(argv[1]);
    //const char *filebase = argv[2];

    unsigned short port = atoi(argv[1]);
    const char *webroot = argv[2];

    const char *hostname = argv[3];

    //convert hostname to host ip
    struct hostent *he;

    if((he = gethostbyname(hostname))== NULL){
        die("gethostbyname failed");
    }
        char *mdbip = inet_ntoa(*(struct in_addr *)he->h_addr);


    unsigned short mdbport = atoi(argv[4]);


   int mdbsock; // socket descriptor
    if ((mdbsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        die("mdb-lookup socket failed");

    // Construct a server address structure

    struct sockaddr_in mdbservaddr;
    memset(&mdbservaddr, 0, sizeof(mdbservaddr)); // must zero out the structure
    mdbservaddr.sin_family      = AF_INET;
    mdbservaddr.sin_addr.s_addr = inet_addr(mdbip);
    mdbservaddr.sin_port        = htons(mdbport); // must be in network byte order

    // Establish a TCP connection to the server

    if (connect(mdbsock, (struct sockaddr *) &mdbservaddr, sizeof(mdbservaddr)) < 0)
        die("mdb-lookup connect failed");

    FILE *mdbresponse = fdopen(mdbsock, "r");

    if (mdbresponse == NULL){
        die("fdopen");
    }

    // Create a listening socket (also called server socket) 

    int servsock;
    if ((servsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        die("server socket failed");

    // Construct local address structure

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // any network interface
    servaddr.sin_port = htons(port);

    // Bind to the local address

    if (bind(servsock, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
        die("bind failed");

    // Start listening for incoming connections

    if (listen(servsock, 5 /* queue size for connection requests */ ) < 0)
        die("listen failed");

    int clntsock;
    socklen_t clntlen;
    struct sockaddr_in clntaddr;

    FILE *fp;

    //struct stat st;

    while (1) {

        // Accept an incoming connection

        clntlen = sizeof(clntaddr); // initialize the in-out parameter

        if ((clntsock = accept(servsock,
                        (struct sockaddr *) &clntaddr, &clntlen)) < 0)
            die("accept failed");

	
	FILE *input;
	input = fdopen(clntsock, "r");
	if (input == NULL){
	    die("fdopen");
	}
	int err = 0;
	char line[4096] = "/0";
	while(1){
	    if(fgets(line, sizeof(line), input) == NULL){
		printf("%s \"(null) (null) (null)\" 501 Not Implemented \r\n", inet_ntoa(clntaddr.sin_addr));
	    	fclose(input);
		break;
	    } else {
	    char *token_separators = "\t \r\n"; // tab, space, new line
            char *method = strtok(line, token_separators);
            char *requestURI = strtok(NULL, token_separators);
            char *httpVersion = strtok(NULL, token_separators);
	
	    char *badrequest = "HTTP/1.0 400 Bad Request\r\n\r\n<html><body><h1>400 Bad Request</h1></body></html>\r\n";
	    char *notimplemented = "HTTP/1.0 501 Not Implemented\r\n\r\n<html><body><h1>501 Not Implemented</h1></body></html>\r\n";
	    char *notfound = "HTTP/1.0 404 Not Found\r\n\r\n<html><body><h1>404 Not Found</h1></body></html>\r\n";
	    char *ok = "HTTP/1.0 200 OK\r\n\r\n";

	    const char *form =
            	"<h1>mdb-lookup</h1>\n"
            	"<p>\n"
            	"<form method=GET action=/mdb-lookup>\n"
            	"lookup: <input type=text name=key>\n"
            	"<input type=submit>\n"
            	"</form>\n"
            	"<p>\n";

	    char path[1024]; 
	    int pathlength = sprintf(path, "%s%s", webroot, requestURI);
            if (pathlength < 0)
            	die("sprintf failed");
	    struct stat buf; 

	    //printf("%s\n", path);i
	        if (method == NULL || requestURI == NULL || httpVersion == NULL){
		    err = 501;
                    if (send(clntsock, notimplemented, strlen(notimplemented), 0) != strlen(notimplemented)){
                        die("send");
                    }
		} else if (strcmp("GET", method) != 0){
		    err = 501;
		    if (send(clntsock, notimplemented, strlen(notimplemented), 0) != strlen(notimplemented)){
		    	die("send");
		    }
	    	} else if (strcmp(httpVersion, "HTTP/1.0") != 0 && strcmp(httpVersion, "HTTP/1.1") != 0){ 
		    err = 501;
		    if (send(clntsock, notimplemented, strlen(notimplemented), 0) != strlen(notimplemented)){
                        die("send");
                    }
	        } else if (requestURI[0] != '/'){
		    //printf("error 1");
		    err = 400;
		    if (send(clntsock,badrequest, strlen(badrequest), 0) != strlen(badrequest)){
                        die("send");
                    }
		} else if(strstr(requestURI, "/../") != NULL || strstr(requestURI, "/..")){
		    //printf("error 2");
		    err = 400;
		    if (send(clntsock,badrequest, strlen(badrequest), 0) != strlen(badrequest)){
                        die("send");
                    }
		} else if (strcmp(requestURI, "/mdb-lookup")==0){
		    err = 1;
		    if (send(clntsock, ok, strlen(ok), 0) != strlen(ok)){
                            die("send");
                    }
		    if (send(clntsock, form, strlen(form), 0) != strlen(form)){
				die("send");
                    }
	        } else if (strncmp(requestURI,"/mdb-lookup?key=", 16) == 0){
		    err = 1;
		    char *tablestart = "<p>\r\n<table border = "">\r\n<tbody>\r\n";
		    char *tableend = "</tbody>\r\n</table>\r\n</p>\r\n";
		    if (send(clntsock, ok, strlen(ok), 0) != strlen(ok)){
                            die("send");
                    }
		    if (send(clntsock, form, strlen(form), 0) != strlen(form)){
                                die("send");
                    }
		    if (send(clntsock, tablestart, strlen(tablestart), 0) != strlen(tablestart)){
                                die("send");
                    }
		    char tmpsearch[128];
		    strcpy(tmpsearch, requestURI);
		    strtok(tmpsearch, "=");
		    char* tmpstring = strtok(NULL, "=");
		    char searchstring[128];
		    int length;
		
		    if(tmpstring == NULL){
			length = sprintf(searchstring, "\n");
                        if (length < 0)
                            die("sprintf failed");
		    } else {
		    	length = sprintf(searchstring, "%s\n", tmpstring);
    		    	if (length < 0)
        		    die("sprintf failed");
		    }
		    //printf("this is the search string: \"%s\"\n", searchstring);

		    if (send(mdbsock, searchstring, length, 0) != length){
		    	die("send");
		    }
		    char temp[4096];
		    while(fgets(temp, sizeof(temp), mdbresponse) != NULL){
		    
			if (strcmp(temp,"\n") == 0){
			    break;
			}
			//printf("%s", line);


			char row[1024];
    			length = sprintf(row, "<tr>\r\n<td> %s </td>\r\n</tr>\r\n ", temp);
    			if (length < 0)
        		    die("sprintf failed");
    			if (send(clntsock, row, length, 0) != length)
        		    die("send failed");

		    }



		    if (send(clntsock, tableend, strlen(tableend), 0) != strlen(tableend)){
                                die("send");
                    }
		
                } else if (stat(path, &buf) == -1){
                    //printf("error 3");
                    err = 404;
                    if (send(clntsock, notfound, strlen(notfound), 0) != strlen(notfound)){
                        die("send");
                    } 
		} else if (S_ISDIR(buf.st_mode) == 1){
		    if (requestURI[strlen(requestURI)-1] != '/'){
			err = 501;
			if (send(clntsock, notimplemented, strlen(notimplemented), 0) != strlen(notimplemented)){
                            die("send");
                   	}	
		    } else{
			strcat(path, "index.html");
		    }
		}
	        
		if (err == 0){
		    if ((fp = fopen(path, "rb")) == NULL){
		    	err = 404;
			if (send(clntsock, notfound, strlen(notfound), 0) != strlen(notfound)){
                            die("send");
                    	}
		    } else {
			unsigned int n;
    			char sendbuf[4096];
			if (send(clntsock, ok, strlen(ok), 0) != strlen(ok)){
                            die("send");
                    	}
			while((n = fread(sendbuf, 1, sizeof(sendbuf), fp)) > 0){
			   if (send(clntsock, sendbuf, n, 0) != n)
				die("send"); 
			}
			if(ferror(fp)){
			    die("fread");
			}
			fclose(fp);
		    }
		}
	    
		if (err == 501){
		    printf("%s \"%s %s %s\" 501 Not Implemented \r\n", inet_ntoa(clntaddr.sin_addr), method, requestURI, httpVersion);
		} else if (err == 400){
		    printf("%s \"%s %s %s\" 400 Bad Request \r\n", inet_ntoa(clntaddr.sin_addr), method, requestURI, httpVersion);
		} else if (err == 404){
		    printf("%s \"%s %s %s\" 404 Not Found \r\n", inet_ntoa(clntaddr.sin_addr), method, requestURI, httpVersion);
		} else {
		    printf("%s \"%s %s %s\" 200 OK \r\n", inet_ntoa(clntaddr.sin_addr), method, requestURI, httpVersion);
		}
	    	fclose(input);
		break;
	    }
    	
	}

    }
    fclose(mdbresponse);
}
