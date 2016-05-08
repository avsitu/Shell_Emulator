//Victor Wu, Jianfu Situ
#include <unistd.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <ctype.h>
#include <string>
#include <string.h>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>

using namespace std;

vector<char> v;
vector< vector<string> > args;
vector< vector<char> > history;
int esc= 0;
int arrowCounter=0;

void ResetCanonicalMode(int fd, struct termios *savedattributes){
	tcsetattr(fd, TCSANOW, savedattributes);
}

void SetNonCanonicalMode(int fd, struct termios *savedattributes){
	struct termios TermAttributes;
	//	char *name;

	// Make sure stdin is a terminal. 
	if(!isatty(fd)){
		fprintf (stderr, "Not a terminal.\n");
		exit(0);
	}

	// Save the terminal attributes so we can restore them later. 
	tcgetattr(fd, savedattributes);

	// Set the funny terminal modes. 
	tcgetattr (fd, &TermAttributes);
	TermAttributes.c_lflag &= ~(ICANON | ECHO); // Clear ICANON and ECHO. 
	TermAttributes.c_cc[VMIN] = 1;
	TermAttributes.c_cc[VTIME] = 0;
	tcsetattr(fd, TCSAFLUSH, &TermAttributes);
}

vector<string> tokenize(string str) { //parse str into vector of strings
    bool wait = false;
    string temp = "";
    vector<string> tokens;
    char* cstr = new char[str.length()+1];
    strcpy(cstr, str.c_str());
    char* token = strtok(cstr, " ");
	while(token != NULL) {
		if(token[strlen(token) - 1] == 0x5C) {
			temp = string(token);
		}
		else {
			if(temp.empty()) {
				tokens.push_back(token);
			}
			else {
				temp.erase(temp.length()-1);
				string s = temp + " " + string(token);
				tokens.push_back(s);
				temp = "";
			}			
		}
		token = strtok(NULL, " ");
	}
	return tokens;
}

void parse() { //parse input into vectors of string vectors 
	args.erase(args.begin(), args.end());
	string input(v.begin(), v.end());
    string str = "";
    bool space = false;
	for(unsigned int i = 0; i < input.length(); i++) {
	    if(input[i] == '|') {  //push to new vector for each pipe
	        args.push_back(tokenize(str));
	        str = "";
	        str.push_back(input[i]);
	        str.push_back(' ');
	    } 
	    else if(input[i] == '>' || input[i] == '<') {
	    	str.push_back(' ');
	    	str.push_back(input[i]);
	    	str.push_back(' ');
	    }
	    else {
	        str.push_back(input[i]);
	    }   
	}
	args.push_back(tokenize(str));
}

void printcd(){ //prints on every line, the current directory for shell prompt
	string result;
	string cwd(getcwd(NULL, 0));
	cwd.append("% ");
	if(cwd.length() > 16) {
		 int index = cwd.rfind("/");
		 result = "/...";
		 result.append(cwd.substr(index)); 		   
	}
	else {
		 result = cwd;
	}
	write(1, result.c_str(), result.length());
}

void pwd(){	
	if(args[0].size() > 1) {
		write(1, "Too many arguments for pwd\n", 27);
		return;
	}
	else {
		char* cwd = getcwd(NULL, 0);
		strcat(cwd, "\n");
		write(1, cwd, strlen(cwd));
	}	
}

void cd() {
	if(args.size() > 1 || args[0].size() > 2) {
		write(1, "Too many arguments for cd\n", 26);
		return;
	}	
	else if(args[0].size() == 1) {
		string str = "/home";
		chdir(str.c_str());
	}
	else if(args[0].size() == 2) {
		if(chdir(args[0][1].c_str()) == -1) {
			string error = "Invalid directory " + args[0][1] + "\n";
			write(1, error.c_str(), error.length()+1);
			return;
		}
	}
}

void ls() { 
	/*code snippets taken from http://stackoverflow.com/a/3554147 
	and http://stackoverflow.com/a/20777069*/
	struct dirent *myDirent;
	struct stat fileStat;
	char perms[11];
	DIR *pDir;
	//set up params
	if(args[0].size() >= 3) {
		write(1, "Too many arguments for ls\n", 26);
		return;
	}
	if(args[0].size() == 1)
		pDir = opendir(getcwd(NULL, 0));
	else if(args[0].size() == 2) {
		pDir = opendir(args[0][1].c_str());
		if (pDir == NULL) {
			string error = "Invalid directory " + args[0][1] + "\n";
			write(1, error.c_str(), error.length()+1);
			return;
		}
	}

	while ((myDirent = readdir(pDir)) != NULL) {
		if(args[0].size() == 1) //looking at current dir
			stat(myDirent->d_name, &fileStat);
		else { //looking at specified path
			string str = args[0][1] + "/" + myDirent->d_name;
			stat(str.c_str(), &fileStat);
		}
		perms[0] = (S_ISDIR(fileStat.st_mode))  ? 'd' : '-';
		perms[1] = (fileStat.st_mode & S_IRUSR) ? 'r' : '-';
		perms[2] = (fileStat.st_mode & S_IWUSR) ? 'w' : '-';
		perms[3] = (fileStat.st_mode & S_IXUSR) ? 'x' : '-';
		perms[4] = (fileStat.st_mode & S_IRGRP) ? 'r' : '-';
		perms[5] = (fileStat.st_mode & S_IWGRP) ? 'w' : '-';
		perms[6] = (fileStat.st_mode & S_IXGRP) ? 'x' : '-';
		perms[7] = (fileStat.st_mode & S_IROTH) ? 'r' : '-';
		perms[8] = (fileStat.st_mode & S_IWOTH) ? 'w' : '-';
		perms[9] = (fileStat.st_mode & S_IXOTH) ? 'x' : '-';
		perms[10] = ' ';
		write(1, perms, 11);
		write(1, myDirent->d_name, strlen(myDirent->d_name));
		write(1, "\n", 1);
	}
	closedir (pDir);		
}

void ff2(string dir, string fname) { //recusively search subdirs
	struct dirent *myDirent;
	DIR *pDir = opendir(dir.c_str());	
	while ((myDirent = readdir(pDir)) != NULL) {	
		if(myDirent->d_type == DT_DIR && strcmp(myDirent->d_name, ".") != 0 && 
		strcmp(myDirent->d_name, "..") != 0) {
			string newDir = dir + string(myDirent->d_name)+ "/";
			ff2(newDir, fname);
		}
		else if(fname.compare(myDirent->d_name) == 0) {
			string str = dir + fname + "\n";
			write(1, str.c_str(), str.length());
		}
	}
}

void ff() {
	struct dirent *myDirent;
	DIR *pDir;
	string fname, dir;
	if(args[0].size() == 1 || args[0][1] == ">") {
		write(1, "Please specify a file name to match\n", 36);
		return;
	}
	if(args[0].size() == 2 || args[0][2] == ">") {
		pDir = opendir(getcwd(NULL, 0));
		fname = args[0][1];
		dir = "./";
	}
	else if(args[0].size() == 3 || args[0][3] == ">") {
		//fprintf(stderr, "%s\n", args[0][2].c_str());
		pDir = opendir(args[0][2].c_str());
		if(pDir == NULL)
			return;
		fname = args[0][1];
		dir = args[0][2] + "/";
	}
	else {
		write(1, "Too many arguments for ff\n", 26);
		return;
	}
	while ((myDirent = readdir(pDir)) != NULL) {	
		if(myDirent->d_type == DT_DIR && strcmp(myDirent->d_name, ".") != 0 && 
		strcmp(myDirent->d_name, "..") != 0) {
			string newDir = dir + string(myDirent->d_name) + "/";
			fprintf(stderr, "%s\n", newDir.c_str());
			ff2(newDir, fname);
		}
		else if(fname.compare(myDirent->d_name) == 0) {
			string str = dir + fname + "\n";
			write(1, str.c_str(), str.length());
		}
	}
}

void execOther() { //try to exec first command
	int pid = fork();
	if(pid == 0) {
		char* argv[3];
		argv[0] = new char[args[0][0].length()+1];
		strcpy(argv[0], args[0][0].c_str());
		if(args[0].size() == 1) {
			execvp(argv[0], argv);
			exit(0);
		}
		else {
			argv[1] = new char[args[0][1].length()+1];
			strcpy(argv[1], args[0][1].c_str());
			argv[2] = NULL;
			execvp(argv[0], argv);
			exit(0);
		}
	}
	else {
		waitpid(0,0,0);
	}

}

void exitShe(){
   struct termios SavedTermAttributes;
    ResetCanonicalMode(1, &SavedTermAttributes);
    exit(0); 
}

void saveCommand(){	//save history of commands, newest commands are inserted to the front
	history.insert(history.begin(), v);
	if(history.size() > 10)	//if history has 10 commands, delete oldest command after adding new one to keep size 10
		history.pop_back();
}

void getCommand(){							//retrieve a command in the history in which is currently pointed to
	if(history.size()>0 && arrowCounter >=0){						//special case to prevent segfaults
		for(int j = 0; j < history[arrowCounter].size(); ++j){
			write(1,&history[arrowCounter][j],1);
			v.push_back(history[arrowCounter][j]);
		}
	}
}

void clearLine(){							//clear the input on current line 
	for(int i=0;i<v.size();i++)
		write(1, "\b \b", 3);
	v.clear();
}

void incrementArrow(){							//keep track of which command it's pointing to in the history
	if(arrowCounter<history.size()-1 || arrowCounter == -1)
		arrowCounter++;
}

void decrementArrow(){
	if(arrowCounter>=0)
		arrowCounter--;	
}

void arrowDetect(char RXChar){
	if(RXChar == 0x5b && esc==1)//[ is detected, (2nd char in arrow sequence)
	    esc=2;
	else if(esc == 2 ) {    //last element for up or down arrow is detected
	    if(RXChar == 0x41 && history.size()>0){         //up arrow
	        if(arrowCounter == history.size()-1)
                        write(1,"\a",1);
	        clearLine();
	        incrementArrow();
	        getCommand();
		}
		else if(RXChar ==0x42 && history.size()>0){     //down arrow
		    if(arrowCounter <0)
                    write(1,"\a",1);
	        clearLine();
	        decrementArrow();
	        getCommand();
		}
		/*else if(RXChar == 0x43 || RXChar==0x44){        //left and right arrow
	        write(1, "\b \b\b \b", 6);
	        v.pop_back();v.pop_back();
		}*/
		else if((RXChar == 0x41 || RXChar == 0x42) && history.size()==0){
		    write(1,"\a",1);
	        write(1, "\b \b\b \b", 6);
	        v.pop_back();v.pop_back();
		}
	}
}

void execInternal() {
	if(!args[0][0].compare("pwd")) {
		pwd();
	}
	else if(!args[0][0].compare("ls")) {
		ls();
	}
	else if(!args[0][0].compare("ff")) {
		ff();
	}
	else 
		execOther();
}

int isRedir(vector<string>& arg) {
	int count = 0;
	for(unsigned int i = 0; i < arg.size(); i++) {
		if(arg[i] == ">" || arg[i] == "<")
			count++;
	}
	return count;
}

int findSym(vector<string>& arg) {
	for(unsigned int i = 0; i < arg.size(); i++) {
		if(arg[i] == ">" || arg[i] == "<")
			return i;
	}
}

void pRedirect(vector<string>& arg) {
	int s = findSym(arg); //find first redir symbol
	int fd;
	if(isRedir(arg) > 1) { //redirect read and write 
		fd = open(arg[arg.size()-1].c_str(), O_WRONLY|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR);
		dup2(fd, 1);
		fd = open(arg[s+1].c_str(), O_RDONLY);
		dup2(fd, 0);
	}
	else if(arg[s] == "<") { //redirect read
		fd = open(arg[s+1].c_str(), O_RDONLY);
		dup2(fd, 0);
	}
	else if(arg[s] == ">") { //redirect write
		fd = open(arg[s+1].c_str(), O_WRONLY|O_TRUNC|O_CREAT, S_IRUSR|S_IWUSR);
		dup2(fd, 1);				
	}
	else {
		write(1, "Failed to redirect\n", 21);
		exit(0);
	}
	close(fd);
	if(arg[0] == "ff") { 
		ff();
		exit(0);
	}
	else {
		char* argv[3];
		argv[0] = new char[arg[0].length()+1];
		strcpy(argv[0], arg[0].c_str());
		argv[2] = NULL;	
		if(s == 2) {
			argv[1] = new char[arg[s-1].length()+1];
			strcpy(argv[1], arg[s-1].c_str());						
		}
		else {
			argv[1] = NULL;
		}
		execvp(argv[0], argv);		
		exit(0);
	}
}

void redirect(vector<string>& arg) { //redirect with no pipes
	int pid = fork();
	if(pid == 0) {
		pRedirect(arg);
	}
	else {
		waitpid(0,0,0);
	}
}

void piping() {	
	//based on pseudo code from discussion
	int numPipes = 2*(args.size()-1);
	int pipefd[numPipes];
  	int pid;		

	//create all pipes
	for(unsigned int i = 0; i < args.size()-1; i++) 
		pipe(pipefd + i*2);
	
	//fork and exec for each command 
	for(unsigned int i = 0; i < args.size(); i++) {
		pid = fork();
		if(pid == 0) {	//if child process
			if(i != 0 ) { //if not first command, redirect stdread
				dup2(pipefd[(i-1)*2], 0);
			}
			if(i != args.size() - 1) { //if not last command, redirect stdout
				dup2(pipefd[i*2+1], 1);	
			}
			//close all pipes
			for(int j = 0; j < numPipes; j++) {
				close(pipefd[j]);
			}
			//if redirect is found
			if(isRedir(args[i]) > 0) {
				if(i != 0)
					args[i].erase(args[i].begin());
				pRedirect(args[i]);	
			}

			//exec pwd, ls, or ff
			else if(!args[i][0].compare("pwd") || !args[i][0].compare("ls") || 
			!args[i][0].compare("ff")) { 
				execInternal();
				exit(0);
			}
			
			else {	//exec external commands				
				char* argv[3];
				if(i == 0) {
					argv[0] = new char[args[i][0].length()+1];
					strcpy(argv[0], args[i][0].c_str());
					argv[1] = new char[args[i][1].length()+1];
					strcpy(argv[1], args[i][1].c_str());
					argv[2] = NULL;	
				}
				else {
					argv[0] = new char[args[i][1].length()+1];
					strcpy(argv[0], args[i][1].c_str());
					if(args[i].size() == 3) {
						argv[1] = new char[args[i][2].length()+1];
						strcpy(argv[1], args[i][2].c_str());
					}
					else
						argv[1] = NULL;
					argv[2] = NULL;
				}
				execvp(argv[0], argv);
				exit(0);					
			}
		}
		else if (pid < 0) {
			exit(0);
		}
	}	
	//close pipes and wait for child processes
	for(int i = 0; i < numPipes; i++) {
		close(pipefd[i]);
	}
	for(unsigned int i = 0; i < args.size(); i++)
		waitpid(0,0,0);	
}

int main(int argc, char *argv[]){
	struct termios SavedTermAttributes;
	char RXChar;
	SetNonCanonicalMode(STDIN_FILENO, &SavedTermAttributes);
	printcd();

	while(1){
		read(0, &RXChar, 1);
		if(0x04 == RXChar){ // C-d
			break;
		}
		else{
			if(RXChar == 0x0A) { //ENTER, process command					
				write(1, "\n", 1);				
				if(v.empty()) { //empty cmd
					printcd();
					continue;
				}
				saveCommand(); //save cmd to history
				parse(); 
				v.clear();
				if(args[0][0].compare("exit") == 0) //exit
					break;
				else if(args[0][0].compare("cd") == 0) //cd
					cd();
				else {
					if(isRedir(args[0]) > 0 && args.size() == 1) //redirect but no pipes
						redirect(args[0]);
					else if(args.size() == 1) //no pipes needed
						execInternal();
					else
						piping();
				}	
				printcd(); //print cwd on new line
				arrowCounter=-1;        //reset the arrow
			}
			else if(RXChar == 0x7F){		//BACKSPACE
				if(v.size() > 0){
					v.pop_back();
					write(1, "\b \b", 3);
				}
				else
                    write(1,"\a",1);
			}
			else if(isprint(RXChar)){		//CHARACTERS
				write(1, &RXChar, 1);
				v.push_back(RXChar);
			}
			else if(RXChar == 0x1B && esc!=1)//esc is detected so arrow is possible
				esc=1;
			arrowDetect(RXChar);
		}
	}
	ResetCanonicalMode(STDIN_FILENO, &SavedTermAttributes);
	return 0;
}
