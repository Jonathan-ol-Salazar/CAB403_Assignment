#include <stdio.h> // File input and output
#include <stdlib.h> // Standard library 
#include <errno.h> // Define integer value 'errno' which is defined by functions in event of an error
#include <string.h> // Manipulate strings and arrays
#include <netdb.h> // Definitions for network database operations
#include <sys/types.h> // Defines collection of typedef symbols and structures for threads
#include <netinet/in.h> // Defines 'sockaddr_in' structure to store addresses for Internet protocol family
#include <sys/socket.h> // Used for 'socklen_t', size of address  
#include <unistd.h> // Provide accesss to POSIX operating system API
#include <stdbool.h> // Defines boolean types and values
#include <ctype.h> // Defines function 'toupper()', to convert lowercase alphabet to uppercase
#include <stdint.h> // Define set of integer types
#include <time.h> // Defines functions to manuipulate date and time information
#include <pthread.h> // Defines symbols for threads
#define PORT_NO 12345 // Default port number
#define MAXDATASIZE 300 // Maximum number of bytes client can recieve at once 
#define RETURNED_ERROR -1 // Variable for returning an error
#define NUM_TILES_X 9 // Number of tiles in x-axis
#define NUM_TILES_Y 9 // Number of tiles in x-axis

/*--------------------------------------GLOBAL VARIABLES----------------------------------------*/

bool gameover; // Variable for game loop, bool is 'true' if game is over
bool win_game; // bool to determine if game is won. 'true' if won
pthread_t client_thread; // thread for executing client procedure
pthread_attr_t attr; // decalring pthread attributes

//struct for holding the valeus for the leaderbaord list
typedef struct leaderboardValues {
	char playerName[20];//name of the current player
	int numOfPlays;//number of plays
	long int time;//game tiem
	struct leaderboardValues* previouos;//previous leaderboard value
	int numOfWins;//number of wins from the player
	struct leaderboardValues* next;
} leaderboardValues;

/*--------------------------------------FUNCTIONS-----------------------------------------------*/

/*
	This function executes the shutdown procedure, for when the the user chooses to quit the program.
	Uses argument 'socket_identifier' to get socket id of server
*/
void Shutdown_Procedure(int socket_identifier){

	close(socket_identifier); // Socket the socket to connecting to server
	exit(0); // Exit program
}




/*
	This function displays a blank grid that the user sees when first starting the game
*/
void Starting_Grid(){

	// Printing remaining mines
	printf("\n\n\nRemaining mines: 10\n\n");
	printf("      ");

	// Printing number x-axis
	for (int number_axis = 1; number_axis < 10; number_axis++)
	{
		printf("%d ", number_axis);
	}
	printf("\n  ");

	// Printing dashed border under number x-axis
	int dash = 45;
	for (int i = 0; i <21; i++)
	{				
		printf("%c", dash);
	}
	printf("\n");

	// Printing letter y-axis
	for (char number_axis = 'A'; number_axis < 'J'; number_axis++)
	{
		printf("  %c | ", number_axis);
		printf("\n");
	}
	printf("\n");
}

/*
	This function receives the current number of mines that havent been flagged in the game from the server 
	and prints it to the screen. It uses argument 'socket_identifier' to get the socket id of server
*/
void Remaining_Mines(int socket_identifier){
	uint16_t remaining_mines; // unsigned integer to store data from server
	int bytesofint; // int to store number of bytese recieved from server

	// if-statement to check if 'recv' function executed correctly
	if ((bytesofint = recv(socket_identifier, &remaining_mines, sizeof(uint16_t), 0 )) == RETURNED_ERROR)
	{
		perror("Receiving integer from server: remaining_mines");
	} 	
	// Printing data from server to screen
	printf("\n\n\nRemaining Mines:%d\n", remaining_mines);
}

/*
	This function is used to receive the updated game grid from the server and 
	print the grid to the screen
*/
void Grid_Print(int socket_identifier){
	
	int bytesofgrid; // interger to store number of bytes received from server
    uint16_t servergrid; // unsigned integer to store 2d array sent from server
    int *gamegrid[NUM_TILES_X]; // An array of pointers

	/*-----------------RECEIVING GRID FROM SERVER-------------------*/

	//for-loop used to allocate memory for every row in grid
    for (int i = 0; i < NUM_TILES_X; i++)
    {
    	gamegrid[i] = (int *)malloc(NUM_TILES_X * sizeof(int));
    }

    /*
		Use two for-loops to receive 2D array from server with networkd byte order.
		if-statment used to check if 'recv' function executed correctly. recv returns 
		number of bytes received on correct execution and -1 for an error
	*/
    for (int i = 0; i < NUM_TILES_Y; i++)
    {
    	for (int j = 0; j < NUM_TILES_X; j++)
    	{   		  	    	
	    	if ((bytesofgrid=recv(socket_identifier, &servergrid, sizeof(uint16_t), 0)) == RETURNED_ERROR){
				perror("Receiving grid from server");
				exit(EXIT_FAILURE);	    
			}
			gamegrid[i][j] = ntohs(servergrid);			
		}
    }



	/*-----------------PRINTING GRID FROM SERVER-------------------*/
	
	char letters = 'A'; // char variable used to print y-axis
	int dash = 45; // int variable used to print dash-axis below number x-axis

	
	// spacing for grid graphical representation
	printf("\n");
	printf("     ");
	
	//for-loop to print out number x-axis. Prints numbers  1-9
	for (int number_axis = 1; number_axis < 10; number_axis++)
	{
		printf("%d ", number_axis);
	}	

	// spacing for grid graphical representation
	printf("\n  ");

	//	for-loop to print out horizonatal dash-border below number axis: '-'
	//	Printing out char value of integer value 'dash', because ASCII decimal of 45 is '-'
	for (int i = 0; i <21; i++)
	{		
		printf("%c", dash);	
	}

	// spacing for grid graphical representation
	printf("\n");

	//This section prints out the game grid and the vertical letter axis
	for (int i=0; i < NUM_TILES_Y; i++) 
	{	
		//Printing out y-axis of letters
		printf("  %c| ", letters );
		if (letters <= 'Z')
		{
			++letters;
		}

		//for-loop to print tiles
		for (int j = 0; j < NUM_TILES_X; j++)
		{	
			// Printing out white-space tiles, ' ' which is ASCII decimal of 32
			if(gamegrid[i][j] == 32)
			{
				printf("%c ",gamegrid[i][j]);
			}
			// Printing out adjacent tiles 
			else if (gamegrid[i][j] >= 0 && gamegrid[i][j] <=8)
			{
				printf("%d ",gamegrid[i][j]);
			}
			// Printing out flagged tiles, '+' which is ASCII decimal of 43
			else if(gamegrid[i][j] == 43)
			{
				printf("%c ",gamegrid[i][j]);
			}	
			// Printing out mines, '*' which is ASCII decimal of 42
			else if(gamegrid[i][j] == 42)
			{
				printf("%c ",gamegrid[i][j]);
			}	
		}
		printf("\n"); 	// spacing for grid graphical representation
	}
	printf("\n"); 	// spacing for grid graphical representation
}

/*
	This function is used to receive and print a confirmation message from the server to
	alert the user of the game outcome of their chosen input.
	It will also detect if the server sends a 'win/lose' message which will end the game
*/
void Input_Confirmation(int socket_identifier){

	char confirmation_buffer[MAXDATASIZE]; // char variable to hold message from server. Has a max size of MAXDATASIZE
	char lose[] = "Game over! You hit a mine"; // char array to print game over message
	char win[] = "Congratulations! You have located all the mines."; // char array to print win game message
	int bytesofstring; // variable to hold number of bytes received from server


	// Receiving string from server
	if((bytesofstring = recv(socket_identifier, &confirmation_buffer, MAXDATASIZE, 0)) == RETURNED_ERROR){
		perror("Receiving string from Server: confirmation_buffer");
		exit(EXIT_FAILURE);
	}
	// Printing message from server
	confirmation_buffer[bytesofstring] = '\0';
	printf("%s\n", confirmation_buffer );
	
	//String compare function to detect if the server sends a message indicating to user of win or lose game
	if(strcmp(confirmation_buffer,lose) == 0 || strcmp(confirmation_buffer,win) == 0)
	{
		gameover = true;
		if (strcmp(confirmation_buffer,win) == 0)
		{
			win_game = true;
		}
	}
}




/*
	This function is used to get the users input for the tile option or quit the game, and returns that input
*/
int Game_Option(void){

	bool check = true; // bool for the while loop
	char gameoption; // char which stores the users input
	int asciioption; // ASCII form of variable 'gameoption'

	// Printing instuctions to user
	printf("\nChoose an option:\n");
	printf("<R> Reveal tile\n");
	printf("<P> Place flag\n");
	printf("<Q> Quit game\n\n");

	// A while-loop is used to ensure the user enters a valid input
	while(check){

		printf("Option (R,P,Q):"); // Printing instructions to user
		scanf("%c",&gameoption); // Scanning for users input and storing it in 'gameoption'
		while ((getchar()) != '\n'); // Clearing the input buffer

		// Casting 'gameoption' to an integer and converting it to upper case, which is stored in 'asciioption'
		asciioption = toupper((int)gameoption); 

		/*
		if-statement to check if user input is equal to one of the game options.
		80,81,82 are the ASCII values of P,Q,R respectively, which are the game options.
		If user input does not equal one of the integers, printf is used to alert user to 
		enter a valid input. If user enters valid option, the while loop is broken
		*/
		if (asciioption == 80 || asciioption == 81 || asciioption == 82){			
			break;				
		}
		else{
			printf("ENTER A VALID OPTION!\n");
		}
	}
	return asciioption; // Returning the value of 'asciioption'
}

/* 
	This function is used to send the users input for the 'tile option' and the corresponding coordinates
	to the server. The coordinates are sent as an array.
*/ 
void Sending_Game_Option(int socket_identifier){
		
	int input[2]; // Array with a size of 2. Used to store input coordinates 
	uint16_t optionsend; // unsigned integer to store tile option and send to server 
	uint16_t inputsend; // unsigned integer to store tile coordinates and send to server 
	int option; // integer to store input for tile option 
	char x; // char to store input x-value
	char y; // char to store input y-value
	int asciix; // int to store ASCII value of 'x'
	int asciiy; // int to store ASCII value of 'y'
	bool check = false; // bool for while loop

	option = Game_Option(); // 'option' is equal to the int that 'Game_Option()' returns

	optionsend = option; // Assigning 'option' to a variable that can be sent through the socket

	/* 
		Sending user input for tile option through socket, using the 'socket_identifier' arguement of the function
	   	if-statment is used to check if 'send' function executed correctly. 'send' returns -1 if error occured
	*/
	if (send(socket_identifier, &optionsend,sizeof(uint16_t),0 ) == RETURNED_ERROR)
	{
		perror("Sending User Input: optionsend");
	}

	// If user input 'Q', quit game, else ask for coordinates
	if(option == 81) // 81 = ASCII Q
	{
		gameover = true; 
	}
	else if(option != 81)
	{
		while(!check) //Loop to ensure user enters valid input
		{
			printf("Enter tile coordinates:");
			scanf("%[^,],%[^\n]", &x,&y); // Store user input in variables 'x' and 'y'
			while ((getchar()) != '\n'); //Clear input buffer
			
			/* 	
				Use integer casts to convert input coordinates to corresponding ASCII values.
				y-coordinate is converted to upper case because its a letter input
			*/
			asciix = (int)x;
			asciiy = toupper((int)y);

			/*
				if-statement to check user input is within game board limits
				x-axis limits: 1-9 (ASCII values: 49-57)
				y-axis limits: 1-9 (ASCII values: 65-73)
				If input is within boundaries, loop is broken. Else, user is notified
			*/
			if (asciix >= 49 && asciix <= 57 && asciiy >= 65 && asciiy <= 73)
			{
				break;
			}
			else
			{
				printf("ENTER A VALID COORDINATE!\n");
			}
		}
		
		//Minus ASCII numbers to get corresponding numbers on game play grid
		asciix -= 49;
		asciiy -= 65;
		
		//Put coordinates into array to be sent to server
		input[0] = asciix;
		input[1] = asciiy;

		//for-loop to loop through array and send coordinates to server with network byte order
		for (int i = 0; i < 2; i++) {
			inputsend = htons(input[i]);
			if (send(socket_identifier, &inputsend,sizeof(uint16_t),0 ) == RETURNED_ERROR)
			{
				perror("Sending User Input: inputsend");
			}
		}
	}
}




/*
	This function executes the game loop of Minesweeper. It calls several functions to 
	send game coordinates to the server and receive the updated game grid from the server. 
	It contains a timer which calculates the total game play of the user and prints the time 
	once the game is over. It uses argument 'socket_identifier' to pass the server socket id
	to the functions it calls
*/
void Play_Minesweeper (int socket_identifier){	

    time_t start, end; // timer variables for start and end times
    double elapsed; // variable of type double to store total elapsed tiime
	Starting_Grid(); // Printing the blank grid to the screen
   	time(&start);  /* start the timer */

	// while-loop for game 
	while(!gameover)
	{	

		// Get user input and send to server
		Sending_Game_Option(socket_identifier);

		// if-statement to check if user quits game
		if (gameover == true)
		{
			break;
		}
		else
		{
			Remaining_Mines(socket_identifier); // Receive and print remaining mines from server
			Grid_Print(socket_identifier); // Receive and print grid from server
			Input_Confirmation(socket_identifier); // Printing string for confirmation of user input
		}	
	    time(&end); // Storing current time in variable 'end'
    	
    	
	}
	// if-statment to print elapsed time if game is won. This is determined by the bool 'win_game'
	if(win_game == true){
		elapsed = difftime(end, start); // using function difftime to find difference between 'start' and 'end' times
		printf("You won in %g seconds!\n", elapsed); // Printing elapsed time
	}
	// Waiting for user input
	printf("Please press ENTER to continue to main menu\n"); 
	getchar();	
}

void leaderBoard(char* data){

	//print the header for the leaderboard
	printf("================================== LEADERBOARD =================================\n\n");

	//set all values to null
	leaderboardValues* entries = NULL;
	leaderboardValues* lastleaderboardValues = NULL;
	int leaderboardValuesCount = 0;
	int dataLen = strlen(data);
	int i = 0;

	//make sure there are's no entries in the leaderboard
	while(i < dataLen) {
		leaderboardValues* leaderboardValues = malloc(sizeof(leaderboardValues));
		leaderboardValues->next = NULL;
		leaderboardValues->previouos  = NULL;

		//assign value to the variables
		if (entries == NULL) {
			entries = leaderboardValues;
			lastleaderboardValues = leaderboardValues;
		}

		else {
			leaderboardValues->previouos = lastleaderboardValues;
			lastleaderboardValues->next = leaderboardValues;
			lastleaderboardValues = leaderboardValues;
		}
		leaderboardValuesCount++;
	}

	if (leaderboardValuesCount > 0) {
		int winningTime = -1;
		leaderboardValues* highestleaderboardValues = NULL;
		leaderboardValues* leaderboardValues = entries;

		if(leaderboardValues != NULL) {
			if (leaderboardValues->time == winningTime) {
				if (leaderboardValues->numOfWins < highestleaderboardValues->numOfWins) {
					highestleaderboardValues = leaderboardValues;
				}
			else if (leaderboardValues->numOfWins == highestleaderboardValues->numOfWins) {
				if (strcmp(leaderboardValues->playerName, highestleaderboardValues->playerName) < 0)
					  	highestleaderboardValues = leaderboardValues;
				}
			else if (leaderboardValues->time > winningTime) {
					winningTime = leaderboardValues->time;
					highestleaderboardValues = leaderboardValues;
				}


			}
			leaderboardValues = leaderboardValues->next;
		}

	}
	printf("================================== LEADERBOARD =================================\n\n");

}

/*
	This function displays the Main Menu of the game. It displays instructions to 
	the user which specify what game options can be selected. After the user input 
	is verified, the input is send to the server and the corresponding function to
	the selectedd game option will be run. The function uses arguement 'socket_identifier'
	which is passed to the various functions that are called.
*/
void Main_Menu(int socket_identifier){

	// while-loop to run forever
	while(1){

		uint16_t input_mainmenu; // unsigned integer to store user input and send to server
		int bytesof_input_main; // store number of bytes of data being sent to server
		char option; // char to store user input
		int asciioption; // int to store ACSII decimal value of 'option'
		bool check = false; // bool for while-loop
		
		// while-loop to ensure user enters valid input
		while(!check){

			// Printing instructions to screen
			printf("\nWelcome to the minesweeper gaming system\n\n");
			printf("Please enter a selection:\n");
			printf("<1> Play Minesweeper\n");
			printf("<2> Show Leaderboard\n");
			printf("<3> Quit\n\n");
			printf("Selection option (1-3):");

			// Storing user input in 'option', and clearing input buffer
			scanf("%c", &option);
			while ((getchar()) != '\n');
			
			input_mainmenu = (int)option; // Casting 'option' of type char to an int

			/* 
				if-statement to check if input is valid. Input must be between 49-51,
				which are the corresponding ASCII char values of the game options - 1,2,3.
				The loop is broken if a valid option is selected otherwise a message is
				sent to the screen to alert the user
			*/
			if (input_mainmenu >= 49 && input_mainmenu <= 51 )
			{
				break;
			}
			else
			{
				printf("ENTER A VALID option!\n");
			}
		}

		input_mainmenu -= 48; // Minus 48 to get ASCII decimal value of 1,2 or 3

		// if-statment to check if data was sent to the server properly
		if ((bytesof_input_main = send(socket_identifier, &input_mainmenu,sizeof(uint16_t) , 0 )) == RETURNED_ERROR)
		{
			perror("Sending input for Main Menu: input_mainmenu");
		} 

		// if option 1 chosen, execute Minesweeper game
		if (input_mainmenu == 1)
		{		
			gameover = false;
			Play_Minesweeper(socket_identifier);
		}
		// if option 2 chosen, display leaderboard
		else if (input_mainmenu == 2)
		{
			char leaderBoardBuffer[30];
			memset(leaderBoardBuffer, 0, sizeof(leaderBoardBuffer)/sizeof(char));
			leaderBoard(leaderBoardBuffer);

		}
		// if option 3 chosen, execute shutdown procedure
		else if (input_mainmenu == 3)
		{
			Shutdown_Procedure(socket_identifier);
		}
	}
}

/*
	This function displays the login procedure that occurs every time a client
	is connected to the server. It prompts the user for a Username and a Password, 
	which is sent to the server. It then waits for the server to send data back, 
	which will detemine if the user has entered valid login credentials. If so,
	the program will execute game, otherwise, the client will disconnect from server.
*/
void *Login_Prompt(void* sockfd){

	int socket_identifier = (intptr_t)sockfd; // Casting void arguement as a useable int variable
	char username[100]; // char to store username with max size of MAXDATASIZE
	int password; // int to store password
	//uint64_t passwordsend; // unsigned integer to store password and send to server

	/*--------------------------Displaying startup graphical interface--------------------------------*/
	for (int i = 0; i < 56; i++)
	{
		printf("=");
	}
	printf("\n");

	printf("Welcome to the online Minesweeper gaming system\n");

	for (int i = 0; i < 56; i++)
	{
		printf("=");
	}
	printf("\n\n");

	printf("You are required to log on with your registered username and password.\n\n");
	/*-----------------------------------------------------------------------------------------------*/

	// Prompting user for username and password and storing input in respective variables
	printf("Username: ");
	scanf("%s", username); // Clearing input buffer
	while ((getchar()) != '\n');

	printf("Password: ");
	scanf("%d", &password);
	while ((getchar()) != '\n'); // Clearing input buffer

	// if-statment to check if username and password were sent to the server 
	if (send(socket_identifier, &username,sizeof(username),0 ) == RETURNED_ERROR)
	{
		perror("Sending Username: username");
	}
	
	uint64_t passwordsend = htonl(password); // Network byte order used to correctly send data
	if (send(socket_identifier, &passwordsend,sizeof(uint64_t),0 ) == RETURNED_ERROR)
	{
		perror("Sending Password: passwordsend");
	}
	
	int bytesofaccept; // number of bytes received from server
	int accept_reject; // data from server in correct network byte order
	uint16_t accept; // unsigned int to store data from server

	// if-statement to check if data was corectly received from server
    if ((bytesofaccept=recv(socket_identifier, &accept, sizeof(uint64_t), 0)) == RETURNED_ERROR){
		perror("Receiving Accept/Reject from server: accept");
		exit(EXIT_FAILURE);	    
	}

	accept_reject = ntohs(accept); // converting 'accept' to network byter order
	
	// if-statement to check if server accepted or rejected client 
	// if accepted cient continue to game, else close connection to server and end program
	if(accept_reject == 0){
		Main_Menu(socket_identifier);		
	}
	else{
		printf("\nYou entered either an incorrect username or password. Disconnecting\n");
		close(socket_identifier);
		exit(1);
	}
}


int main(int argc, char *argv[]){

	int sockfd; // listen on sock_fd
	struct hostent *he; // struct for entry into host database
	struct sockaddr_in their_addr; /* connector's address information */
	// char leaderBoardBuffer[30];
	// memset(leaderBoardBuffer, 0, sizeof(leaderBoardBuffer)/sizeof(char));

	// leaderBoard(leaderBoardBuffer);
	if ((he=gethostbyname(argv[1])) == NULL) {  /* get the host info */
		herror("gethostbyname");
		exit(1);
	}

	/* generate the socket */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == RETURNED_ERROR) {
		perror("socket");
		exit(1);
	}

	their_addr.sin_family = AF_INET;      /* host byte order */
	if (argc == 3)
	{
		their_addr.sin_port = htons(atoi(argv[2]));     /* short, network byte order */
	}
	else if(argc < 3){
		their_addr.sin_port = htons(PORT_NO);     /* short, network byte order */
	}
	else if (argc > 3)
	{
		fprintf(stderr,"usage: client_hostname port_number\n");
		exit(1);
	}
	their_addr.sin_addr = *((struct in_addr *)he->h_addr);
	bzero(&(their_addr.sin_zero), 8);     /* zero the rest of the struct */

	// Connect to socket 
	if (connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == RETURNED_ERROR) {
		perror("connect");
		exit(1);
	}

	pthread_attr_init(&attr); /// Initiallising pthread attributes

	gameover = false; // bool for game loop

	// while-loop to execute game
	while(!gameover)
	{
		pthread_create(&client_thread, &attr, Login_Prompt, (void*) (intptr_t)sockfd); // Thread to execute user login procedure
		pthread_join(client_thread, NULL); // Wait for thread to end 
	}
	return 0;
}
