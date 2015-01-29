#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <dirent.h>

int port;
int QueueLength = 5;

char * ServerType = "CS 252 lab5";
char * clrf = "\r\n";
char * notFound = "File not found";

char *space = " ";

char * firstPart =  "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">\n<html>\n<head>\n<title></title>\n</head>\n<body>\n<h1>";

char * secondPart_1 = "</h1>\n<table><tr><th><img src=\"../icons/blank.gif\" alt=\"[ICO]\"></th>";
char * secondPart_2_NA = "<th><a href=\"?C=N;O=A\">Name</a></th>";
char * secondPart_2_ND = "<th><a href=\"?C=N;O=D\">Name</a></th>";
char * secondPart_2_MA = "<th><a href=\"?C=M;O=A\">Last modified</a></th>";
char * secondPart_2_MD = "<th><a href=\"?C=M;O=D\">Last modified</a></th>";
char * secondPart_2_SA = "<th><a href=\"?C=S;O=A\">Size</a></th>";
char * secondPart_2_SD = "<th><a href=\"?C=S;O=D\">Size</a></th>";
char * secondPart_2_DA = "<th><a href=\"?C=D;O=A\">Description</a></th>";
char * secondPart_2_DD = "<th><a href=\"?C=D;O=D\">Description</a></th>";
char * secondPart_3 = "</tr><tr><th colspan=\"5\"><hr></th></tr>\n";

char * thirdPart_1 = "<tr><td valign=\"top\"><img src=\"";
char * thirdPart_2 = "\" alt=\"[";
char * thirdPart_3 = "]\"></td><td><a href=\"";
char * thirdPart_4 = "\">";
char * thirdPart_5 = "</a>     ";
char * thirdPart_6 = " </td><td>&nbsp;</td></tr>\n";

char * fourthPart = "<tr><th colspan=\"5\"><hr></th></tr>\n</table><address>Apache/2.2.24 (Unix) mod_ssl/2.2.24 OpenSSL/0.9.8r Server at www.cs.purdue.edu Port 443</address></body></html>";

char * nbsp = "</td><td>&nbsp;";
char * alignRight = " </td><td align=\"right\">";

char * icon_unknown = "../icons/unknown.gif";
char * icon_menu = "../icons/menu.gif";
char * icon_back = "../icons/back.gif";

char * empty = "-"; 

pthread_mutex_t m;

void processRequest(int socket);
void poolSlave (int masterSocket);
void processRequestThread (int socket);
void writeLink (int socket, char * path, char * name, char * fileType, char * parent);
void writeServerAndContentType (int socket, char * contentType);
void writeServerType (int socket);
void writeContentType (int socket, char * contentType);
void writeSuccess (int socket, char * contentType);
void writeFail (int socket, char * contentType);
void writeCGIHeader (int socket);
void writeStat (int socket);
void writeLog (int socket);
int endsWith (char * path, char * suffix);
int sortNameA (const void *name1, const void *name2);
int sortModifiedTimeA (const void *name1, const void *name2);
int sortSizeA (const void *name1, const void *name2);
int sortNameD (const void *name1, const void *name2);
int sortModifiedTimeD (const void *name1, const void *name2);
int sortSizeD (const void *name1, const void *name2);

char * MYNAME = "chengkang xu\n";
char * startTime;
int countRequests = 0;
double minimumTime = 100000000.0;
char slowestRequest [256];
double maximumTime = 0.0;
char fastestRequest [256];

char sourceHost[1024];

extern "C" void disp (int signo) {

	if (signo == SIGCHLD) {
		while (waitpid(-1, NULL, WNOHANG) > 0);
	}

//	signo == SIGPIPE
}

int main( int argc, char ** argv )

{
	/*get server start time*/

	time_t rawtime;
	time (&rawtime);
	startTime = ctime(&rawtime);

	/*handle zombie process*/

	signal(SIGCHLD, disp);

	/*parse arguments*/

	if ( argc < 1 ) {
		fprintf( stderr, "not enough args\n");
		exit(1);
	}

	port = 8888;//default

	char mode = 0;

	if (argc > 1) {

		if (!strcmp(argv[1], "-f")) {

			mode = 'f';
		}

		else if (!strcmp(argv[1], "-t")){

			mode = 't';

		}

		else if (!strcmp(argv[1], "-p")) {

			mode = 'p';

		}

		if (mode && argc != 2) {

			port = atoi( argv[2] );
		}

		else if (!mode && argc == 2) {

			port = atoi( argv[1] );
		}
	}

	// Set the IP address and port for this server
	struct sockaddr_in serverIPAddress;
	memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
	serverIPAddress.sin_family = AF_INET;
	serverIPAddress.sin_addr.s_addr = INADDR_ANY;
	serverIPAddress.sin_port = htons((u_short) port);

	// Allocate a socket
	int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
	if ( masterSocket < 0) {
		perror("socket");
		exit( -1 );
	}

	// Set socket options to reuse port. Otherwise we will
	// have to wait about 2 minutes before reusing the sae port number
	int optval = 1;
	int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR,
			(char *) &optval, sizeof( int ) );

	// Bind the socket to the IP address and port
	int error = bind( masterSocket,
			(struct sockaddr *)&serverIPAddress,
			sizeof(serverIPAddress) );
	if ( error ) {
		perror("bind");
		exit( -1 );
	}

	// Put socket in listening mode and set the
	// size of the queue of unprocessed connections
	error = listen( masterSocket, QueueLength);
	if ( error ) {
		perror("listen");
		exit( -1 );
	}

	/*pool mode*/

	if (mode == 'p') {

		pthread_t tid[QueueLength];

		for (int i = 0; i < QueueLength; i ++) {
			pthread_attr_t attr;

			pthread_attr_init(&attr);
			pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

			pthread_create(&tid[i], &attr, (void *(*)(void *))poolSlave,
					(void *)masterSocket); 
		}

		pthread_join(tid[0], NULL);
		poolSlave(masterSocket);
	}

	else {

		while ( 1 ) {
			
			struct sockaddr_in clientIPAddress;
			int alen = sizeof( clientIPAddress );
			int slaveSocket = accept( masterSocket,
					(struct sockaddr *)&clientIPAddress,
					(socklen_t*)&alen);

			if ( slaveSocket < 0 ) {
				perror( "accept" );
				exit( -1 );
			}

			/*process method*/

			if (mode == 'f') {

				pid_t slave = fork();

				if (slave == 0) {

					processRequest( slaveSocket );

					shutdown(slaveSocket, 2);

					_exit(2);
				}

				close( slaveSocket );
			}

			/*thread method*/

			else if (mode == 't') {
				pthread_t t;
				pthread_attr_t attr;

				pthread_attr_init(&attr);
				pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
				pthread_create(&t, &attr, (void * (*)(void*))processRequestThread, (void *)slaveSocket);
				
			}

			else {
				processRequest( slaveSocket );
				close( slaveSocket );
			}

		}
	}

}

/*thread handler*/

void processRequestThread (int socket) {

	processRequest(socket);
	shutdown(socket, 2);
	close(socket);
}

/*thread pool handler*/

void poolSlave(int masterSocket) {

	while(1) {

		// Accept incoming connections
		struct sockaddr_in clientIPAddress;
		int alen = sizeof( clientIPAddress );

		pthread_mutex_lock(&m);
		int slaveSocket = accept( masterSocket,
				(struct sockaddr *)&clientIPAddress,
				(socklen_t*)&alen);
		pthread_mutex_unlock(&m);
		processRequest(slaveSocket);
		shutdown(slaveSocket, 2);
		close(slaveSocket);
	}
}

/*process request*/

void processRequest( int socket )
{
	/*set statistics values*/
	clock_t start_t, end_t, current_t;

	start_t = clock();

	countRequests ++;
			
	/*read from socket and parse the file path*/

	char * path = (char *)malloc(sizeof(char) * 1024);
	char * protocol = (char *)malloc(sizeof(char) * 16);

	char newChar = 0;
	char oldChar = 0;

	int n = 0;

	int pathLength = 0;
	int protocolLength = 0;

	int isPath = 0;
	int isPrototcol = 0;

	while(read(socket, &newChar, sizeof(newChar))) {

		if (isPath) {

			if(newChar == ' ') {

				isPath = 0;
				isPrototcol = 1;

			}

			else {
				path[pathLength] = newChar;
				pathLength ++;
			}
		}

		else if (isPrototcol) {

			if (newChar == '\r') {

				isPrototcol = 0;
			}

			else {
				protocol[protocolLength] = newChar;
				protocolLength ++;

			}
		}

		else {

			if(newChar == ' ') {

				if (n == 3) {

					isPath = 1;
				}
			}

			else if(oldChar == '\r' && newChar == '\n') {

				read(socket, &oldChar, sizeof(char));

				read(socket, &newChar, sizeof(char));

				if (oldChar == '\r' && newChar == '\n') {

					break;
				}
			}
		}
		n ++;
		oldChar = newChar;
	}

	path[pathLength] = '\0';
	protocol[protocolLength] = '\0';


	if (path[0] == '/' && pathLength == 1) {

		strcat(path, "index.html");
	}

	
	/*Map the document path to the real file*/

	char * cwd = (char *)malloc(sizeof(char) * 512);

	for (int i = 0; i < 512; i ++) {

		cwd[i] = '\0';
	}

	cwd = getcwd(cwd, 512);

	char cmp [10] = {0};

	int needMake = 1;

	int CGIPathStart = 0; 

	for (int i = 0; i < 8; i ++) { 

		cmp[i] = path[i];

		if ((i == 5 && !strcmp(cmp, "/icons")) || 
				(i == 6 && !strcmp(cmp, "/htdocs") || 
				 (i == 7 && !strcmp(cmp, "/cgi-bin") || 
				  (i == 4 && ! strcmp(cmp, "/logs") ||
				   (i == 5 && !strcmp(cmp, "/stats")))))) {

			strcat(cwd, "/http-root-dir");

			strcat(cwd, path);

			needMake = 0;
			break;
		}
	}

	if (needMake) {

		strcat(cwd, "/http-root-dir/htdocs");
		strcat(cwd, path);
	}

	/*write to logs*/

	sourceHost[1023] = '\0';
	gethostname (sourceHost, 1023);
	strcat(sourceHost, ":");

	char convertPort[16];
	snprintf(convertPort, 16, "%d", port);
	strcat(sourceHost, convertPort);
	strcat(sourceHost, path);
	strcat(sourceHost, "\n");

	FILE * f = fopen("/homes/xu411/Desktop/lab5-src/http-root-dir/logs", "a");

	fwrite(sourceHost, sizeof(char), strlen(sourceHost), f);

	fclose (f);

	/*if cgi bin, execute command*/

	char * start;

	if (strstr(path, "cgi-bin")) {

		write(socket, protocol, strlen(protocol));
		write(socket, space, 1);

		start = strchr(cwd, '?');

		if (start) {

			*start = '\0';
		}


		if (!open(cwd, O_RDONLY, 0644)) {

			writeFail(socket, "text/plain");
		}

		else {

			char ** args = (char **)malloc(sizeof(char *) * 2);
			args[0] = (char *)malloc(sizeof(char) * strlen(path));

			for (int i = 0; i < strlen(path); i ++) {

				args[0][i] = '\0';
			}

			args[1] = NULL;

			start = strchr(path, '?');

			if (start) {

				start ++;

				strcpy(args[0], start);
			}

			writeCGIHeader(socket);

			int STDOUT = dup(1);
			dup2(socket, 1);
			close(socket);

			pid_t p = fork();

			if (p == 0) {
			//	printf("here\n");
				setenv("REQUEST_METHOD","GET",1);
				setenv("QUERY_STRING", args[0],1);

				//int STDOUT = dup(1);

				//dup2(socket, 1);
				//close(socket);

				execvp(cwd, args);

				//dup2(STDOUT, 1);
				//close(STDOUT);

				exit(2);
			}

			dup2(STDOUT,1);
			close(STDOUT);
		}
	}

	else if (strstr(path, ".so")) {

	}

	else if (!endsWith(path, "stats") && !endsWith(path, "stats/")){

		/*if sorting directory, make a new path*/

		int isDirectory = 0;

		char sortingMode = 0;
		char sortingOrder = 0;

		if (endsWith(cwd, "?C=M;O=A") ||  
				endsWith(cwd, "?C=M;O=D") ||  
				endsWith(cwd, "?C=N;O=A") || 
				endsWith(cwd, "?C=N;O=D") ||  
				endsWith(cwd, "?C=S;O=A") ||  
				endsWith(cwd, "?C=S;O=D") ||
				endsWith(cwd, "?C=D;O=A") ||
				endsWith(cwd, "?C=D;O=D")) {

			sortingMode = cwd[strlen(cwd) - 5];
			sortingOrder = cwd[strlen(cwd) - 1];
			cwd[strlen(cwd) - 8] = '\0';
			path[strlen(path) - 8] = '\0';

			isDirectory  = 1;
		}

		if (endsWith(cwd, "?C=M;O=A/") ||  
				endsWith(cwd, "?C=M;O=D/") ||  
				endsWith(cwd, "?C=N;O=A/") || 
				endsWith(cwd, "?C=N;O=D/") ||  
				endsWith(cwd, "?C=S;O=A/") ||  
				endsWith(cwd, "?C=S;O=D/") || 
				endsWith(cwd, "?C=D;O=A/") ||
				endsWith(cwd, "?C=D;O=D/")) {

			sortingMode = cwd[strlen(cwd) - 6];
			sortingOrder = cwd[strlen(cwd) - 2];
			cwd[strlen(cwd) - 9] = '\0';

			path[strlen(path) - 9] = '\0';

			isDirectory = 1;
		}

		/*distinguish from file and directory*/

		DIR * dir = opendir(cwd);

		int isFile = 0;

		if (dir != NULL) {

			isDirectory = 1;
		}

		else {

			isFile = 1;
		}

		/*Determine Content type*/

		char contentType [12] = {0};

		if(endsWith(cwd, ".html") || endsWith(cwd, ".html/") || (isDirectory)) {

			strcpy(contentType, "text/html");
		}

		else if(endsWith(cwd, ".gif") || endsWith(cwd, ".gif/")) {

			strcpy(contentType, "image/gif");
		}

		else if (endsWith(cwd, ".svg") || endsWith(cwd, ".svg/")) {

			strcpy(contentType, "image/svg+xml");
		}

		else {
			strcpy(contentType, "text/plain") ;
		}

		/*Send HTTP Reply Header + Document data*/

		write(socket, protocol, strlen(protocol));
		write(socket, space, 1);

		int fd = 0;

		char in = 0;

		if (isFile) {

			fd = open (cwd, O_RDONLY, 0644);

			/*404*/

			if (fd == -1) {

				writeFail(socket, "text/plain");
			}

			/*200*/

			else {
				writeSuccess(socket, contentType);
			}

			if (fd != -1) {

				while (read(fd, &in, sizeof(in))) {

					write (socket, &in, 1);
				}
			}
			close(fd);
		}

		else if (isDirectory) {

			struct dirent * ent;

			char ** content = (char **)malloc(sizeof(char *) * 1024);

			int count_files = 0;

			while((ent = readdir(dir))!= NULL) {

				if (strcmp(ent -> d_name ,".") && strcmp(ent -> d_name, "..")) {

					content[count_files] = (char *)malloc(sizeof(char) * (strlen(ent -> d_name)+ strlen(cwd) + 2));

					content[count_files][0] = '\0'; 

					strcat(content[count_files], cwd);

					if (!endsWith(cwd, "/")) {
						strcat(content[count_files], "/");
					}

					strcat(content[count_files], ent -> d_name);

					count_files ++;
				}
			}

			/*sort by size*/

			if (sortingMode == 'S') {

				if (sortingOrder == 'A') {

					qsort(content, count_files, sizeof(char *), sortSizeA);
				}

				if (sortingOrder == 'D') {

					qsort(content, count_files, sizeof(char *), sortSizeD);

				}
			}

			/*sort by modified time*/

			else if (sortingMode == 'M') {

				if (sortingOrder == 'A') {

					qsort(content, count_files, sizeof(char *), sortModifiedTimeA);
				}

				if (sortingOrder == 'D') {

					qsort(content, count_files, sizeof(char *), sortModifiedTimeD);

				}
			}

			/*sort by name*/

			else {

				if (sortingOrder == 'D' && sortingMode == 'N') {

					qsort(content, count_files, sizeof(char *), sortNameD);
				}

				else {

					qsort(content, count_files, sizeof(char *), sortNameA);
				}
			}


			/*write header*/

			writeSuccess(socket, contentType);

			write(socket, firstPart, strlen(firstPart));
			write(socket, "index of ", 9);
			write(socket, cwd, strlen(cwd));
			write(socket, secondPart_1, strlen(secondPart_1));

			/*changing links based on current sorting Mode*/

			char * N = secondPart_2_ND;
			char * M = secondPart_2_MD;
			char * S = secondPart_2_SD;
			char * D = secondPart_2_DD;

			if (sortingOrder == 'D') {

				if (sortingMode == 'N') {

					N = secondPart_2_NA;
				}

				if (sortingMode == 'M') {

					M = secondPart_2_MA;
				}

				if (sortingMode == 'S') {

					S = secondPart_2_SA;
				}

				if (sortingMode == 'D') {

					D = secondPart_2_DA;
				}
			}

			write (socket, N, strlen(N));
			write (socket, M, strlen(M));
			write (socket, S, strlen(S));
			write (socket, D, strlen(D));

			write (socket, secondPart_3, strlen(secondPart_3));

			/*make parent directory path*/

			char * parentPath = (char *) malloc(sizeof(char) * (strlen(path) + 3));

			for (int i = 0; i < strlen(path); i ++) {

				parentPath [i] = path[i];
			}

			if (!endsWith(path, "/")) {

				parentPath[strlen(path)] = '/';
				parentPath[strlen(path) + 2] = '.';
				parentPath[strlen(path) + 3] = '\0';
			}
			else {
				parentPath[strlen(path)] = '.';
				parentPath[strlen(path) + 2] = '\0';
			}

			parentPath[strlen(path) + 1] = '.';

			writeLink(socket, parentPath, "Parent Directory", "DIR", path);

			/*list files*/

			for (int i = 0; i < count_files; i ++) {


				/*extract file name*/

				char * name = (char *)malloc(sizeof(char ) * strlen(content[i]));

				int FileNameBegin = 0;

				for (int j = (strlen(content[i]) - 1); j >= 0; j --) {


					if (content[i][j] == '/') {

						FileNameBegin = j;
						break;
					}
				}




				for (int j = FileNameBegin + 1; j < strlen(content[i]); j ++) {

					name[j - FileNameBegin - 1] = content[i][j];

					if (j == strlen(content[i]) - 1) {

						name[j - FileNameBegin] = '\0';
					}
				}

				/*determine file type*/

				char * fileType;

				if (opendir(content[i]) != NULL) {

					fileType = "DIR";
				}

				else {
					fileType = "   ";
				}

				writeLink(socket, content[i], name, fileType, path);

				free(name);
			}

			write(socket, fourthPart, strlen(fourthPart));
		}
	}

	end_t = clock();
	current_t = (double)(end_t - start_t);

	if (current_t < minimumTime) {
	
		minimumTime = current_t;
		strcpy(fastestRequest, path);
		fastestRequest[strlen(path)] = '\0';
	}

	else if (current_t > maximumTime) {

		maximumTime = current_t;
		strcpy(slowestRequest, path);
		slowestRequest[strlen(path)] = '\0';
	}

	if (endsWith(path, "stats") || endsWith(path, "stats/")) {
	
		write(socket, protocol, strlen(protocol));
		write(socket, space, 1);
		writeCGIHeader(socket);
		writeContentType(socket, "text/plain");

		write(socket, MYNAME, strlen(MYNAME));

		write(socket, startTime, strlen(startTime));
		write(socket, "\n", 1);

		char * nr = (char *)malloc(sizeof(char) * 16);
		snprintf(nr, 16, "%d", countRequests);
		write(socket, nr, strlen(nr));
		free(nr);
		write(socket, "\n", 1);

		char * mit = (char *)malloc(sizeof(char) * 256);
		snprintf(mit, 256, "%f", minimumTime);
		write(socket, mit, strlen(mit));
		free(mit);
		write(socket, "\n", 1);

		write(socket, fastestRequest, strlen(fastestRequest));
		write(socket, "\n", 1);

		char * mat = (char *)malloc(sizeof(char) * 256);
		snprintf(mat, 256, "%f", maximumTime);
		write(socket, mat, strlen(mat));
		free(mat);
		write(socket, "\n", 1);

		write(socket, slowestRequest, strlen(slowestRequest));
		write(socket, "\n", 1);
	}

	free(path);
	free(protocol);
	free(cwd); 
}

/*write link to the file*/

void writeLink (int socket, char * path, char * name, char * fileType, char * parent) {

	write(socket, thirdPart_1, strlen(thirdPart_1));

	char * gif = icon_unknown;
	char * alt = strdup(fileType);

	if (!strcmp(name, "Parent Directory")) {

		gif = icon_back;
	}

	else if (!strcmp(fileType, "DIR")) {

		gif = icon_menu;
	}


	write(socket, gif, strlen(gif));
	write(socket, thirdPart_2, strlen(thirdPart_2));
	write(socket, alt, strlen(alt));
	write(socket, thirdPart_3, strlen(thirdPart_3));


	if (strcmp(name, "Parent Directory")) {

		if (!endsWith(parent, "/")) {

			char * PD = (char *)malloc(sizeof(char) * strlen(path));

			PD[0] = '\0';
			strcat(PD, parent);
			strcat(PD, "/");
			strcat(PD, name);

			write(socket, PD, strlen(PD));

			free(PD);
		}

		else {
			write(socket, name, strlen(name));
		}
	}

	else {
		write(socket, path, strlen(path));
	}

	write(socket, thirdPart_4, strlen(thirdPart_4));
	write(socket, name, strlen(name));
	write(socket, thirdPart_5, strlen(thirdPart_5));

	char * m_time = (char *)malloc(sizeof(char) * 20);

	int s = 0;

	if (!strcmp(name, "Parent Directory")) {

		write(socket, nbsp, strlen(nbsp));

		m_time = strdup(empty);
	}

	else {
		struct stat name_stat;

		stat(path, &name_stat);

		struct tm * timeInfo;
		timeInfo = localtime(&(name_stat.st_mtime));

		strftime(m_time, 20, "%F %H:%M", timeInfo);

		s = name_stat.st_size;

	}

	write(socket, alignRight, strlen(alignRight));
	write(socket, m_time, strlen(m_time));

	if (strcmp(name, "Parent Directory")) {

		write(socket,alignRight, strlen(alignRight));

		if (strcmp(fileType, "DIR")) {

			char * _s = (char *)malloc(sizeof(char ) * 16);

			snprintf(_s, 16, "%d", s);

			write(socket, _s, strlen(_s));

			free(_s);
		}

		else {
			write(socket, empty, strlen(empty));
		}
	}

	write (socket, thirdPart_6, strlen(thirdPart_6));

	free(alt);
	free(m_time);
}


/*write server and content type to socket*/

void writeServerAndContentType (int socket, char * contentType) {

	writeServerType(socket);
	writeContentType(socket, contentType);
}

/*write server type*/

void writeServerType (int socket) {

	write(socket, "Server:", 7);
	write(socket, space, 1);
	write(socket, ServerType, strlen(ServerType));
	write(socket, clrf, 2);

}

/*write content type*/

void writeContentType (int socket, char * contentType) {

	write(socket, "Content-type:", 13);
	write(socket, space, 1);
	write(socket, contentType, strlen(contentType));
	write(socket, clrf, 2);
	write(socket, clrf, 2);

}

/*write success header*/

void writeSuccess (int socket, char * contentType) {

	write(socket, "200", 3);
	write(socket, space, 1);
	write(socket, "Document", 8);
	write(socket, space, 1);
	write(socket, "follows", 7);
	write(socket, clrf, 2);

	if (contentType != NULL) {
		writeServerAndContentType(socket, contentType);
	}
}

/*write fail header*/ 

void writeFail (int socket, char * contentType) {

	write(socket, "404", 3);
	write(socket, "File", 4);
	write(socket, "Not", 3);
	write(socket, "Found", 5);
	write(socket, clrf, 2);
	writeServerAndContentType(socket, contentType);
	write(socket, notFound, strlen(notFound));
}

/*write CGI header*/

void writeCGIHeader (int socket) {

	writeSuccess(socket, NULL);
	writeServerType(socket);
}

/*check if path ends with suffix*/

int endsWith (char * path, char * suffix) {

	int pathLen = strlen(path);
	int suffixLen = strlen(suffix);

	for (int i = suffixLen; i > 0;i --) {

		if (suffix[suffixLen - i] != path[pathLen - i]) {

			return 0;
		}	
	}

	return 1;
}

/*ascending comparation function*/

int sortNameA (const void * name1, const void * name2)
{
	const char * name1_ = *(const char **)name1;
	const char * name2_ = *(const char **)name2;
	return strcmp(name1_, name2_);
}

int sortModifiedTimeA (const void * name1, const void * name2)
{
	const char * name1_ = *(const char **)name1;
	const char * name2_ = *(const char **)name2;

	struct stat name1_stat, name2_stat;

	stat(name1_, &name1_stat);
	stat(name2_, &name2_stat);

	return difftime(name1_stat.st_mtime, name2_stat.st_mtime);

}

int sortSizeA (const void * name1, const void * name2)
{
	const char * name1_ = *(const char **)name1;
	const char * name2_ = *(const char **)name2;

	struct stat name1_stat, name2_stat;

	stat(name1_, &name1_stat);
	stat(name2_, &name2_stat);

	int s1 = name1_stat.st_size;
	int s2 = name2_stat.st_size;

	return s1 - s2;
}

/*descending comparation function*/

int sortNameD (const void * name2, const void * name1)
{
	const char * name1_ = *(const char **)name1;
	const char * name2_ = *(const char **)name2;
	return strcmp(name1_, name2_);
}

int sortModifiedTimeD (const void * name2, const void * name1)
{
	const char * name1_ = *(const char **)name1;
	const char * name2_ = *(const char **)name2;

	struct stat name1_stat, name2_stat;

	stat(name1_, &name1_stat);
	stat(name2_, &name2_stat);

	return difftime(name1_stat.st_mtime, name2_stat.st_mtime);

}

int sortSizeD (const void * name2, const void * name1)
{
	const char * name1_ = *(const char **)name1;
	const char * name2_ = *(const char **)name2;

	struct stat name1_stat, name2_stat;

	stat(name1_, &name1_stat);
	stat(name2_, &name2_stat);

	return name1_stat.st_size - name2_stat.st_size;
}


