#define _GNU_SOURCE
#include <arpa/inet.h> // Used for internet operations, functions - htons(), ntohs()
#include <stdio.h> // File input and output
#include <stdlib.h> // Standard library 
#include <errno.h>  // Define integer value 'errno' which is defined by functions in event of an error
#include <string.h> // Manipulate strings and arrays
#include <sys/types.h> // Defines collection of typedef symbols and structures for threads
#include <netinet/in.h> // Defines 'sockaddr_in' structure to store addresses for Internet protocol family
#include <sys/socket.h> // Used for 'socklen_t', size of address  
#include <sys/wait.h>
#include <unistd.h> // Provide accesss to POSIX operating system API 
#include <stdbool.h> // Defines boolean types and values
#include <ctype.h> // Defines function 'toupper()', to convert lowercase alphabet to uppercase
#include <stdint.h> // Define set of integer types
#include <pthread.h> // Defines symbols for threads
#include <time.h> // Defines functions to manuipulate date and time information

#define PORT_NO 12345 // Default port number
#define BACKLOG 10     /* how many pending connections queue will hold */
#define RETURNED_ERROR -1 // Variable if function returns error
#define NUM_TILES_X 9 // Number of tiles in x-axis
#define NUM_TILES_Y 9 // Number of tiles in y-axis
#define NUM_MINES 10 // Total number of mines
#define RANDOM_NUMBER_SEED 42 // Seed for rand() function 
#define MAX_CLIENTS 10 // Maximum number of clients server can serve

/*----------------------------------GLOBALS----------------------------------------------*/
bool client_connected; // bool to determine if a client is connected to server
int xcoords, ycoords, tileoption, revealed_tile, flagged_tile; // bools for game 
int *coordinates; // integer array with a pointer to hold coordinates
bool win; // bool for leaderboard
int client_position[MAX_CLIENTS]; // int array to store the client socket descriptor
int new_fd[MAX_CLIENTS]; // intger array to hold socket descriptor for clients. Has a max size if MAX_CLIENTS
int number_of_connected_client = 0; // int storing current number of clients
pthread_t client_thread[MAX_CLIENTS], client_thread_sighand, ctrlc_check_thread, connection_check_thread; // Threads 
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cv = PTHREAD_COND_INITIALIZER;

static pthread_mutex_t gameTimer = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
static int leaderboarCount = 0;
// struct to store states of game tiles. Re-defined as 'Tile'
typedef struct 
{
   int adjacent_mines; // int to store number of adjacent mines 
   bool revealed;// bool to determine if tile is revealed 
   bool is_mine; // bool to determine if tile is mine 
   bool flagged; // bool to determine if tile is flagged 
} Tile; // typedef name

// struct to store game state and game grid of type 'Tile'. Re-defined as 'GameState'
typedef struct 
{
   Tile tiles[NUM_TILES_X][NUM_TILES_Y]; // 2D-array of type 'Tile'
   bool gameover; // bool to determine if game is over
   int remaining_mines; //  integer which is used to store the current number of mines
} GameState; // typedef name

/*
	Declaring 'game' as an array of structs of type 'GameState'. It has a max size of 
	MAX_CLIENTS. Each index in the array will serve as a struct and will ensure each 
	client has its own playfield
*/
GameState game[MAX_CLIENTS]; 

//struct to store the game informaiton
typedef struct NumWinCount
{
	int time;// time the game has been played
	struct NumWinCount* next;// Get the number of winns
} NumWinCount; //typedef for the number of wins

//struct for getting the players information
typedef struct PlayerRecord
{
	char PlayerName[20];//get the players name
	int NumberOfPlays;//get the number of times the game has been played
	int WinCount;//get the total win count
	NumWinCount* TotalWins;//total wins
	struct PlayerRecord* next;
} PlayerRecord;//typedef name

static PlayerRecord* PlayerTotalWins = NULL;


/*
declare functions for the leaderboard
*/
void removeLeaderboardEntries();
int displayLeaderboard();
void gameRecord(char* PlayerName, bool win, long int time);



/*----------------------------------GAMESTATE----------------------------------------------*/


/*
	This function is used to determine the position of a client in the array 'client_position'.
	This is done by using the arguement 'socket_id', which is the clients socket descriptor.
	The function returns the index in the array when its found. If its not found, the function
	returns a -1.
*/
int index_client_position(int socket_id){

	for (int i=0; i<MAX_CLIENTS; i++){

		// if-statement to check if client socket descriptor was found in the array
		if (client_position[i] == socket_id)
		{
			return(i);  /* it was found */
		}
	}
	return(-1);  /* if it was not found */
}


/*
	This function takes in two coordinates and checks if the corresponding tile contains a mine. 
	The  third arguement 'client_num' is used to determine which client is accessing the funciton.
	If the tile does contains a mine, the function returns true, else it returns false.
*/
bool tile_contains_mine(int x,int y, int client_num){
   
   	if(game[client_num].tiles[x][y].is_mine == true){
      	return true;
   	}
   	else{
    	return false;
   	}
}
	
/*
	This function uses two arguments which are coordinates for a tile. The third arguement
	is to determine which client is accessing function. Two for-loops are used to scan
	the eight surrounding tiles. If one of the tiles is not revealed and contains 0 adjacent mines, the 
	tile is revealed and the function is then recalled using those coordinates. This is using recursion.
	If a scanned tile contains adjacent mines then it is not revealed. 
*/
void tile_no_mine(int x, int y, int client_num){	

	for(int i=0; i<3; i++){
		for(int j=0; j<3; j++){

			// coordinates which itterate through the eight neighbouring tiles
			int xneighbour = x-1+i;
			int yneighbour = y-1+j;

			// if else-statement to check if neighbouring tile contains adjacent mines or not
			if(xneighbour > -1 && xneighbour<9 && yneighbour <9 && yneighbour >-1 && game[client_num].tiles[xneighbour][yneighbour].adjacent_mines == 0 && game[client_num].tiles[xneighbour][yneighbour].revealed == false)
			{
				game[client_num].tiles[xneighbour][yneighbour].revealed = true; // Reveal tile if coordinates are on grid bounaries, has an adjacent mines value of 0 and is not already revealed
				tile_no_mine(xneighbour, yneighbour, client_num); // Using recursion to recall the function, using coordinates of neighbouring tile with 0 adjacent mines
			}
			else if(xneighbour >-1 && xneighbour<9 && yneighbour <9 && yneighbour >-1 && game[client_num].tiles[xneighbour][yneighbour].adjacent_mines > 0 && game[client_num].tiles[xneighbour][yneighbour].revealed == false)
			{	
				game[client_num].tiles[xneighbour][yneighbour].revealed = false; // Do not reveal tile if it has an adjacent mines value greater than 0
			}
		}
    }
}


/*
	This function is used to increase the number of adjacent mines for a given tile coordinate. It takes in 
	argument 'client_num' to determine which client 'GameState' struct should be used. It also take in integer
	'x_adjacentmine' and 'y_adjacentmine' for the coordinates of tiles adjacent to the mines
*/
void adjacent_mines(int client_num, int x_adjacentmine, int y_adjacentmine){

	// if-statement to increase neighbouring tiles 'adjacent mines' value. Value is only increase is selected tile coordinate is 
	// on the game grid and is not a mine
	if(x_adjacentmine >=0 && x_adjacentmine<9 && y_adjacentmine <9 && y_adjacentmine >=0 && game[client_num].tiles[x_adjacentmine][y_adjacentmine].is_mine == false ){
		game[client_num].tiles[x_adjacentmine][y_adjacentmine].adjacent_mines +=1; // Increasing adjacement mines value
	}
}

/*
	This function is used to randomly place mines on the playing field. A rand() function uses a fixed random seed which will ensure 
	each game will have different locations for the mines, however will contain the same mine locations if the same 
	itteration of the game is played. The arguement 'client_num' is used to determine which client is accessing the function.

	It calls a function 'tile_contains_mine()' which checks if the given coordinates 
	contains a mine and function 'adjacent_mines()' which assigns the 8 neighbouring tiles an integer defining the 
	number of mines the tile is adjacent to
*/ 
void place_mines(int client_num){

	// for-loop to loop total number of mines
 	for(int i = 0; i < NUM_MINES; i++){

		int x, y; // int to store x and y coordinates of mines
		do
		{
			// Mines coordinates are randomised and the remainder is taken to ensure coordinate fits in grid
			pthread_mutex_lock(&lock); // lock mutex
			y = rand() % NUM_TILES_X;
			x = rand() % NUM_TILES_Y;
			pthread_mutex_unlock(&lock); // unlock mutex
		} while (tile_contains_mine(x,y, client_num)); // while-loop which keeps running until a randomised tile is found that doesnt contain a mine

		// Two for-loops to itterate through the mines neighbouring tiles and increase their number of 'adjacent mines' value by 1
		for(int i=0; i<3; i++){
			for(int j=0; j<3; j++){

				// Coordinates used to itterate though the mines neighbouring tiles
				int x_adjacentmine = x-1+i;
				int y_adjacentmine = y-1+j;
				adjacent_mines(client_num, x_adjacentmine, y_adjacentmine);
			}
	    }

      // Using 'game' struct to set coordinates as a mine
      game[client_num].tiles[x][y].is_mine = true;
	}
}


/*
	This function is used to reset all the tiles in the game grid. This is done by 
	itterating through the entire grid using two for-loops and reseting the tile 
	structure for each tile. The arguement 'client_num' is used to determine which 
	client is accessing the function.
*/
void Reset_Tiles(int client_num){

	for (int i = 0; i < NUM_TILES_Y; i++)
	{
		for (int j = 0; j < NUM_TILES_X; j++)
		{
			game[client_num].tiles[i][j].adjacent_mines = 0; // All tiles set to 0 adjacent mines 
			game[client_num].tiles[i][j].revealed = false; // All tiles are not revealed
			game[client_num].tiles[i][j].is_mine = false; // All tiles are not mines
			game[client_num].tiles[i][j].flagged = false; // All tiles are not flagged

			revealed_tile = 0; // Variable is reset
		}
	}
	game[client_num].remaining_mines = 10;
	game[client_num].gameover = false; // Bool for game loop is reset
}

/*
	This function is used to send confirmation messages to the client after the user has 
	selected to the reveal a tile. The arguements 'socket_id' and 'client_num' are used 
	to connected to the clients socket descriptor and determine which client is using the function.
	Messages include: valid coordinate input, invalid coordinate input and if tile revealed is a mine. 
	Messages sent depend on value of the global variable 'revealed_tile'. If a mine has been reveal,
	the function Reset_Tiles() is called and the game loop is ended.
*/
void Reveal_Tile_Check(int socket_id, int client_num){

	// char buffers for messages, which contain max size of 200
	char invalid_coord_revealed[200];
	char valid_coord_revealed[200];
	char mine_revealed[200];

	// Corresponding messages being assigned to char buffer variables 
	strcpy(invalid_coord_revealed, "INVALID COORDINATE: THAT TILES ALREADY BEEN REVEALED!");
	strcpy(valid_coord_revealed, "VALID COORDINATE");
	strcpy(mine_revealed, "Game over! You hit a mine");


	/*
		if-statements are used to determine which message will be sent. They are also used 
		to check if the send() function executed correctly. If not, an error message will be 
		send to the screen
	*/
	if(revealed_tile == 1) // Sending invalid coordinate message
	{			
		if (send(socket_id, invalid_coord_revealed, strlen(invalid_coord_revealed), 0) == RETURNED_ERROR)
		{
			perror("Reveal_Tile_Check: Sending invalid_coord_revealed");
		}		
	}
	else if(revealed_tile == 2) // Sending valid coordinate message
	{	
		if (send(socket_id, valid_coord_revealed, strlen(valid_coord_revealed), 0) == RETURNED_ERROR)
		{
			perror("Reveal_Tile_Check: Sending valid_coord_revealed");
		}	
	}
	else if(revealed_tile == 3) // Sending revealed mine message
	{		
		if (send(socket_id, mine_revealed, strlen(mine_revealed), 0) == RETURNED_ERROR)
		{
			perror("Reveal_Tile_Check: Sending mine_revealed");
		}	
		
		Reset_Tiles(client_num); // Calling function to reset tiles
		game[client_num].gameover = true; // Ending game loop 
	}
}

/*
	This function is takes void arguments and returns are bool value. It uses and if else-statement 
	to check if the global variable 'remaining_mines' is equal to 0. If it is, function returns true, 
	else it returns false
*/
bool win_condition(int client_num){
	
	if (game[client_num].remaining_mines == 0)
	{
		return true;
	}
	else{
		return false;
	}
}

/*
	This function is used to send confirmation messages to the client after the user has selected
	to flag a tile. It takes arguement 'socket_id' to send message to client socket descriptor. 
	The arguement 'client_num' is used to determine which client is accessing the function.
	Messages for successful placement of flag - flagging all mines, flagging a mine.
	Messages for unsuccessful placement of flage - no mine at location, tile is already flagged.
	After a flag has successfully been placed, the function win_condition() is called, to check if 
	all the mines have been flagged. If it has, the game loop will be ended.
*/
void Place_Flag_Check(int socket_id, int client_num){

	// char buffers for messages, which contain max size of 200
	char no_mine[200];
	char yes_mine[200];
	char winning_game[200];
	char already_mine[200];

	// Corresponding messages being assigned to char buffer variables 
	strcpy(no_mine, "THERE'S NO MINE THERE!");
	strcpy(yes_mine, "NICE THERES A MINE THERE!");
	strcpy(winning_game, "Congratulations! You have located all the mines.");
	strcpy(already_mine, "INVALID COORDINATE: THERES ALREADY A FLAG THERE!");

	/*
		if-statements are used to determine which message will be sent. They are also used 
		to check if the send() function executed correctly. If not, an error message will be 
		send to the screen
	*/
	if(flagged_tile == 1) // Sending message that no mine at location 
	{		
		if (send(socket_id, no_mine, strlen(no_mine), 0) == RETURNED_ERROR)
		{
			perror("Place_Flag_Check: Sending no_mine");
		}		
	}
	else if(flagged_tile == 2) // Sending message that a mine is at location 
	{
		if (send(socket_id, yes_mine, strlen(yes_mine), 0) == RETURNED_ERROR)
		{
			perror("Place_Flag_Check: Sending yes_mine");
		}		
	}
	else if(flagged_tile == 3) // Sending message that all mines have been flagged
	{
		if (send(socket_id, winning_game, strlen(winning_game), 0) == RETURNED_ERROR)
		{
			perror("Place_Flag_Check: Sending winning_game");
		}		
	}
	else if(flagged_tile == 4) // Sending message that tile is already flagged
	{
		if (send(socket_id, already_mine, strlen(already_mine), 0) == RETURNED_ERROR)
		{
			perror("Place_Flag_Check: Sending already_mine");
		}		
	}

	game[client_num].gameover = win_condition(client_num); // Using win_condition() function to check if all mines have been flagged
}

/*
	This function is used to send the value of the current number of remaining mines to the server.
	It takes an argument 'socket_id' to send the data to the clients socket descriptor. The value 
	of the remaining mines is stored in the an global unsigned integer called 'remaining mines'.
*/
void Mines_Remaining(int socket_id, int client_num){

	uint16_t remaining_mines_send = game[client_num].remaining_mines;
	// if-statement used to check is send() function executed correctly. Prints error message if error occured
	if (send(socket_id, &remaining_mines_send, sizeof(uint16_t), 0) == RETURNED_ERROR){
		perror("Mines_Remaining: Sending remaining_mines");
	}
}

/*
	This function is used to create and update the game grid and then send it to the client. 
	It uses argument 'socket_id' to send the grid to the clients socket descriptor. 
	The arguement 'client_num' is used to determine which client is accessing the function.
	It calls function Mines_Remaining(), to send the client the current number of mines. It then uses 
	two for-loops which contains a range if-statements to determine the state of the tiles. This
	procedure will create the game grid. After the grid is created, another two for-loops are 
	used to send the 2d-array to the client in network byte order form.
*/
void Printing_Grid(int socket_id, int client_num){

	Mines_Remaining(socket_id, client_num); // Send client current number of mines

	int game_grid[NUM_TILES_X][NUM_TILES_Y]; // 2D integer array to store updated game grid
	uint16_t game_grid_send; // unsigned integer used to send game grid to client 

	// Two for-loops to update grid state
	for (int i = 0; i < NUM_TILES_X; i++)
	{
		for (int j = 0; j < NUM_TILES_Y; j++)
		{	
			// if-statement for tiles being flagged. 
			if (flagged_tile == 1 || flagged_tile == 2 || flagged_tile == 3 || flagged_tile == 4)
			{
				// If tile is flagged and is not revealed, set tile to 43 (ASCII value = '+')
				if (game[client_num].tiles[i][j].flagged == true && game[client_num].tiles[i][j].revealed == false)
				{					
					game_grid[i][j] = 43;
				}
				// If tile is not revealed, set tile to 32 (ASCII value = ' ')
				else if (game[client_num].tiles[i][j].revealed == false)
				{
					game_grid[i][j] = 32;
				}				
			}

			// if-statement for tiles being revealed
			if (revealed_tile == 1 || revealed_tile == 2)
			{
				// If tile is revealed, is not a mine and is not flagged, set tile value to number of adjacent mines
				if (game[client_num].tiles[i][j].revealed == true && game[client_num].tiles[i][j].is_mine == false && game[client_num].tiles[i][j].flagged == false)
				{
					game_grid[i][j] = game[client_num].tiles[i][j].adjacent_mines;
				}
				// if tile is not revealed and is not flagged, set tile value to 32 (ASCII value = ' ')
				else if (game[client_num].tiles[i][j].revealed == false && game[client_num].tiles[i][j].flagged == false)
				{
					game_grid[i][j] = 32;
				}
			}
			// If tile revealed is a mine, set tile to 42 (ASCII value 42 = '*'), otherwise sst tile to 32 (ASCII value 32 = ' ')
			else if (revealed_tile == 3)
			{
				if (game[client_num].tiles[i][j].is_mine == true)
				{
					game_grid[i][j] = 42;
				}
				else
				{
					game_grid[i][j] = 32;
				}
			}
			if (flagged_tile == 3)
			{
				if (game[client_num].tiles[i][j].revealed == true && game[client_num].tiles[i][j].is_mine == false && game[client_num].tiles[i][j].flagged == false)
				{
					game_grid[i][j] = game[client_num].tiles[i][j].adjacent_mines;
				}
				if (game[client_num].tiles[i][j].flagged == true)
				{
					game_grid[i][j] = 43;
				}
			}
		}
	}

	// Two for-loops to send grid to client with network byte order.
	// if-statement used to check if send() executed correctly. Prints error message if error occured
	for (int i = 0; i < NUM_TILES_X; i++)
	{
		for (int j = 0; j < NUM_TILES_Y; j++)
		{	
			game_grid_send = htons(game_grid[i][j]); // Network byte order
			if(send(socket_id, &game_grid_send, sizeof(uint16_t),0) == RETURNED_ERROR){
				perror("Printing_Grid: Sending game_grid_send");
			}
		}			
	}
}



/*
	This function is used to reveal the tile that corresponds to the user input coordinates. 
	This is done by using the global variables xcoords and ycoords. The function calls multiple 
	functions and passes the argument 'socket_id' to send data to the clients socket descriptor.
	The arguement 'client_num' is used to determine which client is accessing the function.

	A range of if-statements are used to determine what confirmation message should be sent to the 
	client after a tile is selected to be revealed. This is done by using the global variable 'revealed_tile' 
	which will be used in the function Revealed_Tile_Check().

	Another set of if-statements are used to reveal the tile if its not a mine. If the revealed tile 
	has an adjacent mines value of 0, the recursive function adjacent_mines() is called, which will 
	reveal surrounding tiles with an adjacent mines value of 0.

	The functions Printing_Grid() and Reveal_Tile_Check() are then called to update the grid with the
	revealed tiles and send the client a confirmation message respectively. 
*/
void Reveal_Tile(int socket_id, int client_num){

	// If tile is revealed, send client error message
	if(game[client_num].tiles[xcoords][ycoords].revealed == true)
	{
		revealed_tile = 1; // Variable to send message that tile is already revealed
	}
	// If tile is not revealed and is not a mine, send client message of correct input
	else if(game[client_num].tiles[xcoords][ycoords].revealed == false && game[client_num].tiles[xcoords][ycoords].is_mine == false)
	{
		revealed_tile = 2; // Variable to send message that user input valid
	}
	// If tile is a mine, send client game over message
	else if(game[client_num].tiles[xcoords][ycoords].is_mine == true)
	{
		revealed_tile = 3; // Variable to send message that user has lost the game
	}

	// If tile is not a mine and has an adjacent mines value greater than 0, reveal tile
	if (game[client_num].tiles[xcoords][ycoords].is_mine == false && game[client_num].tiles[xcoords][ycoords].adjacent_mines > 0)
	{
		game[client_num].tiles[xcoords][ycoords].revealed = true;
	}
	// If tile is not a mine and has an adjacent mines value of 0, run adjacent_mines() function
	else if (game[client_num].tiles[xcoords][ycoords].is_mine == false && game[client_num].tiles[xcoords][ycoords].adjacent_mines == 0)
	{			
		tile_no_mine(xcoords,ycoords, client_num);
	}

	Printing_Grid(socket_id, client_num); // Update grid with new revealed tiles 

	Reveal_Tile_Check(socket_id, client_num); // Send client confirmation message of input
}



/*
	This function is used to place a flag on the corresponding tile coordinates given by variables 
	xcoords and ycoords. The function calls multiple functions and passes the argument 'socket_id' 
	to send data to the clients socket descriptor.

	A range of if-statements are used to determine what confirmation message should be sent to the 
	client after a tile is selected to be flagged. This is done by using the global variable 'flagged_tile' 
	which will be used in the function Place_Flag_Check().	

	Another if-statement is used to decrease the number of remaining mines and flag the tile, if the 
	tile is a mine and is not flagged. This is done by minusing the global varable 'remaining_mines'
	by 1. An additional if-statement is used to check if the 'remaining mines' is equal to 0.

	The functions Printing_Grid() and Place_Flag_Check() are then called to update the grid with the
	flagged tiles and send the client a confirmation message respectively. 
*/
void Place_Flag(int socket_id, int client_num){


	// If tile is not a mine, send client message that user input is incorrect
	if (game[client_num].tiles[xcoords][ycoords].is_mine == false)
	{
		flagged_tile = 1; // Variable to send message that tile contains no mine
	}
	// If tile is a mine, send client message that user input is correct
	else if (game[client_num].tiles[xcoords][ycoords].is_mine == true)
	{
		flagged_tile = 2; // Variable to send message that tile contains mine
	}
	
	// If tile is already flagged, send client message that tile is already flagged
	if (game[client_num].tiles[xcoords][ycoords].flagged == true)
	{
		flagged_tile = 4; // Variable to send message that tile is already flagged
	}

	// If tile is a mine and is not flagged, decrease remaining mines by 1 and flag the tile
	// If remaining_mines = 0, send client message that user flagged all the mines
	if (game[client_num].tiles[xcoords][ycoords].is_mine == true && game[client_num].tiles[xcoords][ycoords].flagged == false)
	{
		flagged_tile = 2; // Variable to send message that tile contains mine
		game[client_num].remaining_mines -=1; // Decrease remaining_mines by 1
		game[client_num].tiles[xcoords][ycoords].flagged = true; // Flag tile

		if (game[client_num].remaining_mines == 0)
		{
			flagged_tile = 3; // Variable to send message that user won game
			revealed_tile = 2;
			for (int i = 0; i < NUM_TILES_Y; i++)
			{
				for (int j = 0; j < NUM_TILES_X; j++)
				{
					game[client_num].tiles[i][j].revealed = true; // All tiles are not revealed					
				}
			}
		}
	}	

	Printing_Grid(socket_id, client_num); // Updating grid with new flagged tiles 

	Place_Flag_Check(socket_id, client_num); // Sending client confirmation messages 
}

/*
	This function is used to receive tile option and tile coordinates from the client. The argument 'socket_id'
	is used to receive data from the clients socket descriptor and is passed on as arguements to the 
	functions that are called.A while-loop is used to encase the entire procedure and is only broken when 
	the game is over.

	The function first receives data from the client which specifies which tile option the user has chosen.
	An if else-statement is used to determine if the user has chosen to quit the game, place a flag or
	reveal a tile. If the user has chosen to quit the game, the while-loop is broken, otherwise two 
	for-loops are used to allocate memory and receive an integer array containing the coordinatees
	sent by the client. Network byte order is used in these process. 

	Two if-statements are used to run the functions Reveal_Tile() and Place_Flag(), if the user 
	has selected to either reveal a tile or place a flag.
*/ 
void Receiving_Coords(int socket_id, int client_num){


	// while-loop, which is broken when game is over
	while(!game[client_num].gameover){

		// unsigned integers to receive data from client 
		uint16_t input_tileoption; 
		uint16_t input_coords;
		
		 

		// int to store number of bytes received from client 
		int bytesofinput, bytesofoption;

		// Receiving tile option 
		// if-statement to check if recv() executed correctly. Prints error message if error occured
		if ((bytesofoption = recv(socket_id, &input_tileoption,sizeof(uint16_t) , 0 )) == RETURNED_ERROR)
		{
			perror("Receiving_Coords: Receving input_tileoption");
		} 

		tileoption = input_tileoption; // Assigning user option to global variable

		// if-statement to check if user has chosen to quit game (ASCII value of 81 = 'Q')
		if (tileoption == 81)
		{
			game[client_num].gameover = true; // End game 
		}

		// If user has chosen to place flag or reveal tile, allocate memory and receive coordinates from client 
		else if(tileoption !=81)
		{
			// for-loop to allocate memeory for an array of size 2
		    	coordinates = (int *)malloc(2 *sizeof(int));
		  	
		
			// for-loop to receive coordinates and store into global variable 'coordinates' in network byte order
			for (int i =0; i < 2; i++)
			{
				// Receiving coordinates from client
				// if-statement to check if recv() executed correctly. Prints error message if error occured
				if ((bytesofinput = recv(socket_id, &input_coords,sizeof(uint16_t) , 0 )) == RETURNED_ERROR)
				{
					perror("Receiving_Coords: Receiving input_coords");
				} 
				coordinates[i] = ntohs(input_coords); // Storing data in network byte order
			}

			// Assigning values to global variables xcoords and ycoords
			xcoords = coordinates[1];
			ycoords = coordinates[0];
		}

		// If user chose to reveal tile, execute Reveal_Tile() function
		if (tileoption == 82)
		{
			Reveal_Tile(socket_id, client_num); // Function to reveal tile 
		}
		// If user chose to place flag, execute Place_Flag() function
		if (tileoption == 80)
		{
			Place_Flag(socket_id, client_num); // Function to place flag on tile 
		}
	}
}

/*
	This function is used by executing functions that are used to play the Minesweeper game. 
	The place_mines() function is used to preallocate the mines in the playing field and 
	Receiving_Coords is used to receive user inputs and update playing field. The functions are
	held in a while loop which is broken when the game is over. The function also takes an 
	arguement 'socket_id', which is passed on to Receving _Coords
*/
void Play_Minesweeper(int socket_id, int client_num){


	while(!game[client_num].gameover) 
	{	
		place_mines(client_num);
		Receiving_Coords(socket_id, client_num);
	}
}



PlayerRecord* newPlayer(const char* PlayerName)
{

  //assign values to the player variables
	PlayerRecord* User = malloc(sizeof(PlayerRecord));
	strcpy(User->PlayerName, PlayerName);
	User->WinCount = 0;
	User->NumberOfPlays = 0;
	User->TotalWins = NULL;
	User->next = NULL;
	PlayerRecord* current = PlayerTotalWins;

  //set the amount of wins to the users number of wins
	if (current == NULL) {
		PlayerTotalWins = User->WinCount;
	}

  //update the user in the list
	if (User) {
		while(current->next != NULL)
			 current->next = User;
	}
	return User;
}
/*
Function to update the user information
*/
void updateUser(const char* PlayerName, bool win, NumWinCount* record)
{

  //set local variables to update the players details
	PlayerRecord* User = PlayerTotalWins;
	bool playerPlayed = false;
	NumWinCount* latest;

  //check if the game has been played
	while(User != NULL) {
		if (strncmp(PlayerName, User->PlayerName, 20) == 0) {
			playerPlayed = true;
			break;
		}
		User = User->next;
	}

  //the the number of wins is null update for the current record
	if (latest == NULL) {
		User->TotalWins = record;
		return;
	}

  //create new player if game hasn't been played
	if (playerPlayed == false){
		User = newPlayer(PlayerName);
		User->NumberOfPlays++;
	}

  //update the current record
	while(latest->next != NULL){
		latest = latest->next;
		latest->next = record;
	}
}

/*
Function for setting the players record to add to the leaderboard
*/
void gameRecord(char* PlayerName, bool win, long int time)
{
	int err = pthread_mutex_lock(&lock);
  //if the player doesn't win don't update
	if (!win) {
		updateUser(PlayerName, win, NULL);
		return;
	}
  //update player details when the user wins
	NumWinCount* record = malloc(sizeof(NumWinCount));
	record->next = NULL;
	record->time = time;
	updateUser(PlayerName, win, record);
	err = pthread_mutex_unlock(&lock);
}

/*
function to display the leaderboard to the client
*/
int displayLeaderboard()
{
  //define variables for the leaderboard
	PlayerRecord* User = PlayerTotalWins;
	int error = pthread_mutex_lock(&gameTimer);
	leaderboarCount++;
	int LengthOfReply = 0;
	NumWinCount* record = User->TotalWins;
	char buffer[100];

	if (leaderboarCount == 1){
		error = pthread_mutex_unlock(&gameTimer);
		error = pthread_mutex_lock(&lock);

  //if there is a user with a record add the data to the list
	while(User != NULL) {
		while(record != NULL) {
      //print the linked list to display the game informaiton
			sprintf(buffer, "l,%s,%d,%d,%d,", User->PlayerName, record->time, User->WinCount, User->NumberOfPlays);
			record = record->next;
		}
		User = User->next;
	}
	leaderboarCount--;
}
	//if there are no entries in the leadboard display that information
	if (leaderboarCount == 0){
    printf("\n""================================================================================");
    printf("%s\n","No data to display" );
    printf("================================================================================");
	}

	return buffer;
}
/*
Function to reset the leaderboard to have no entries
*/
void removeLeaderboardEntries()
{
	PlayerRecord* currentUser = NULL;//set the current user to null
	PlayerRecord* nextUser = PlayerTotalWins;//set the value of the next player to the total number of wins

  //set all values to null
	while(nextUser != NULL) {
		currentUser = nextUser;
		nextUser = currentUser->next;
		NumWinCount* currentWin = NULL;
		NumWinCount* nextWin = currentUser->TotalWins;

		if(nextWin != NULL) {
			currentWin = nextWin;
			nextWin = currentWin->next;
		}
	}
}



/*
	This function is used to execute the Main Menu procedures. It takes the arguement
	'socket_id' to receive data from the client determining what menu option the 
	user selected. After the user input is received from the client, a set of
	if else-statement are used to execute a range of functions corresponding to
	user selection. 

	This procedure is incased in a while-loop and is broken when the user chooses to quit the game 		
*/
void Main_Menu(int socket_id){


	client_connected = true; // bool for while-loop

	// while-loop to receive user selection and execute corresponding functions
	while(client_connected)
	{
		// int used to store the clients position in global variable array 'client_position[]'
		// function 'index_client_position()' is used to determine position in array. 
		int client_num = index_client_position(socket_id);

		uint16_t input_mainmenu; // unsigned integer to store user menu option
		int bytesofinput_mainmenu; // Number of bytes received from client
		game[client_num].remaining_mines = 10; // Reseting remaining_mines to 10
		game[client_num].gameover = false; // Variable set to false means client can enter game loop

		// Receiving user input for main menu
		// if-statement to check if recv() executed correctly
		if ((bytesofinput_mainmenu = recv(socket_id, &input_mainmenu,sizeof(uint16_t) , 0 )) == RETURNED_ERROR)
		{
			perror("Main_Menu: Receiving input_mainmenu");
		} 
		
		// If user selected 1, execute functions to play Minesweeper game
		if (input_mainmenu == 1)
		{
			Reset_Tiles(client_num); // Reset the playfield for given client
			Play_Minesweeper(socket_id, client_num); // Play Minesweeper game
		}
		// If user selected 2, execute functions to display leaderboard
		else if (input_mainmenu == 2)
		{
			int displayTheLeaderboard;//create a variable to show call the function that shows the Leaderboard
	      	if (!win){//display no data if there aren't any wins

	     	}
	     	else {
	        	displayTheLeaderboard = displayLeaderboard();//display the leaderboard if there are wins
	      	}
		}
		// If user selected 3, execute functions to quit game
		else if (input_mainmenu == 3)
		{
			//free(coordinates); 
			close(socket_id); 
			pthread_mutex_lock(&lock); // Lock mutex
			number_of_connected_client--; // Decrease number of clients 
			pthread_cond_signal(&cv); // Send conditional signal
			pthread_mutex_unlock(&lock); // Unlock mutex
			client_connected = false;
		}
	}
}

/*
	This function is used to check if ctrl+c has been pressed. If it has, all clients sockets will be closed
	and the progam will terminate
*/
void CtrlC_Handler(int signal){

	// if-statement to check if SIGINT signal has been sent
	if (signal == SIGINT){

		printf(" Received termination signal\n");	// Prints message to screen to inform user of termination
		
		// for-loop to close all client sockets connected
		for (int i = 0; i < MAX_CLIENTS; i++){
			close(new_fd[i]); // close connection to client
		}
		free(coordinates);
		exit(0); // Successful termination of program
	}
}


/*
	This function is used to execute the login prompt when a client first connects with 
	the server. It takes a void argument 'socketid' which contains the clients socket 
	descriptor and is passed on to the functions it calls. 

	The function first opens a txt file 'Authentication.txt' which contains a list of 
	vaild login credentials. The usernames and passwords are separated and put into 
	separate variables.

	Two recv() function are used to receive login credentials from the client. The user inputs 
	are then compared to the credentials from the txt file and an integer is sent back 
	to the client to specify if the login was valid or not (1 = invalid, 0 = valid).
	If login was valid, main menu for Minesweeper is executed, if invalid, client socket is closed
*/
void *Authentication(void* socketid){

	int socket_id = (intptr_t)socketid; // Casting void arguement as a useable int variable

	const int NUMBER_OF_USERS=11; // const int for number of users in txt file 
	const int LENGTH_OF_USERNAME=20; // const int for length of user name in txt file 
	const int LENGTH_OF_PWORD=20; // const int for length of password in txt file 
	
	FILE *AUTH_FILE; // Pointer to file name
	char USER[LENGTH_OF_USERNAME]; // char with max size of LENGTH_OF_USERNAME. Used to store user name
	char PASS; // char used to store password
	char buffer[200]; // buffer to hold data from txt file 
	char username[NUMBER_OF_USERS][LENGTH_OF_USERNAME]; // char with max size of NUMBER_OF_USERS and LENGTH_OF_USERNAME. Used to store username from txt file 
	int passwordtxt[NUMBER_OF_USERS]; // Used to store passwords from txt file 
	int NAME=0; // Used as counter variable when scaning through txt file 

	AUTH_FILE=fopen("Authentication.txt","r"); // Open txt file 
	// if-statement to check if file was opened correctly. 
	if (!AUTH_FILE){
		perror("ERROR OPENING FILE");
	}
		
	// while-loop to scann through txt file and store credentials in variables 'username' and 'passwordtxt'
	while(fgets(buffer,sizeof(buffer),AUTH_FILE)!=NULL){
		int PWD=0; // Counter for 'username'
		sscanf(buffer,"%s %d",&username[NAME][PWD], &passwordtxt[NAME]);
		NAME++; // Increase NAME for username and passwordtxt
		PWD++; // Increase PWD for username
	}
	
	int bytesofuser, bytesofpass; // int to store number of bytes received from client 
	int  password; // int to store user input password
	char user[100]; // char to store user input for username 
	uint64_t pass; // unsigned int to store received password from client
	bool accept; //  bool to accept or reject client attempt to be connected 
	uint16_t accept_reject_send; // unsigned int sent to client specifying if client will be accepted or rejected
	int accept_reject; // int specifying if client will be accepted or rejected

	// if-statements used to check if recv() executed correctly, message printed if error occurs
    if ((bytesofuser=recv(socket_id, &user, sizeof(user), 0)) == RETURNED_ERROR) {
		perror("Authentication: Receiving user");
		exit(EXIT_FAILURE);	    
	}
    if ((bytesofpass=recv(socket_id, &pass, sizeof(uint64_t), 0)) == RETURNED_ERROR) {
		perror("Authentication: Receiving pass");
		exit(EXIT_FAILURE);	    
	}

	password = ntohl(pass); // storing password received from client in network byte order

	// for-loop to check if user login matches any credentials from txt file
	for(int i = 0; i < NUMBER_OF_USERS; i++){

		// if-statment to check if user input matches txt credentials, if so send client accept confirmation (accept_reject = 0)
		if(strcmp(username[i], user) == 0 && passwordtxt[i] == password){
			accept = true; // true if client accepted
			accept_reject = 0; // variable to notfiy client that credentials are correct
			accept_reject_send = htons(accept_reject); // 'accept_reject' being converted to network byte order to be sent to client 

			// if-statement to check if send() executed correctly, message printed if error occurs
			if (send(socket_id, &accept_reject_send,sizeof(accept_reject_send),0 ) == RETURNED_ERROR)
			{
				perror("Authentication: Sending accept_reject_send");
			}			
		}
	}
	fclose(AUTH_FILE); // Closing txt file 

	// if-statement if client credentials incorrect, send client reject confirmation and close client socket
	if(accept == false){
		accept_reject = 1; // variable to notfiy client that credentials are incorrect
		accept_reject_send = htons(accept_reject); // 'accept_reject' being converted to network byte order to be sent to client 
		
		// if-statement to check if send() executed correctly, message printed if error occurs		
		if (send(socket_id, &accept_reject_send,sizeof(accept_reject_send),0 ) == RETURNED_ERROR)
		{
			perror("Authentication: Sending accept_reject_send");
		}	

		close(socket_id); // Close client socket 
		pthread_mutex_lock(&lock); // Lock mutex
		number_of_connected_client--; // Decrease number of clients 
		pthread_cond_signal(&cv); // Send conditional signal
		pthread_mutex_unlock(&lock); // Unlock mutex
	}
	// If client credentials are valid, execute Main_Menu function to play Minesweeper game
	else if (accept == true) 
	{
		Main_Menu(socket_id); // Main menu of game
	}
} 






/*-------------------------------------MAIN------------------------------------------------*/

int main(int argc, char *argv[]) {

	srand(RANDOM_NUMBER_SEED); // Random number generator with a seed of RANDOM_NUMBER_SEED
	
	int sockfd;  // listen on sock_fd
	struct sockaddr_in my_addr;    /* my address information */
	struct sockaddr_in their_addr; /* connector's address information */
	socklen_t sin_size; // Variable for length of socket


	/* generate the socket */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == RETURNED_ERROR) {
		perror("socket");
		exit(1);
	}

	/* generate the end point */
	my_addr.sin_family = AF_INET;         /* host byte order */
	if (argc == 2)
	{
		my_addr.sin_port = htons(atoi(argv[1]));     /* short, network byte order */
	}
	else if(argc <2){
		my_addr.sin_port = htons(PORT_NO);     /* short, network byte order */
	}
	else if (argc > 2)
	{
		fprintf(stderr,"usage: client port_number\n");
		exit(1);
	}
	my_addr.sin_addr.s_addr = INADDR_ANY; /* auto-fill with my IP */

	// Server binds socket to end point
	// if-statement to check if bind() executes correctly. Program ends if error occurs
	if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == RETURNED_ERROR) {
		perror("bind");
		exit(1);
	}

	// Server starts listening for clients.
	// if-statement to check if listen() executes correctly. Program ends if error occurs
	if (listen(sockfd, BACKLOG) == RETURNED_ERROR) {
		perror("Main: listen()");
		exit(1);
	}

	printf("server starts listnening ...\n"); // Print message to show server is listening to client sockets

	// if-statement to check if signal() function executed correctly, prints error message if error occurs
	if(signal(SIGINT, CtrlC_Handler) == SIG_ERR){
		perror("Main: Catching ctrl-c signal\n");
	}

	pthread_attr_t attr; // Declaring thread attributes 
	pthread_attr_init(&attr); // Initialising thread attributes


	// while-loop to accept client sockets and execute Minesweeper game. Runs forever
	while(1) {  


		// if-statement to check number of clients less than or equal to max number of clients
		if(number_of_connected_client >=MAX_CLIENTS){
			pthread_mutex_lock(&lock); // Lock mutex 
			pthread_cond_wait(&cv, &lock); // Wait for condition signal
			pthread_mutex_unlock(&lock); // Unlock mutex
		}

		// for-loop to create thread pool to accept clients from listen queue
		for (int i = 0; i < MAX_CLIENTS; i++){

			sin_size = sizeof(struct sockaddr_in); // Variable for accept() function 
			// if-statement to check if accept() function executed correctly, prints error message if error occured
			if ((new_fd[i] = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == RETURNED_ERROR) {
				perror("Main: Accepting client sockfd");		
				continue;
			}

			client_position[i] = new_fd[i]; // Assign client socket descriptor to a position in 'client_position' array
			printf("server: got connection from %s\n", inet_ntoa(their_addr.sin_addr)); // Prints ip address of new client 
			
			// if-statment to create thread to server client. If pthread_create() does not execute correctly, error message is printed,
			// else increase 'number_of_connect_clients'
			if( pthread_create(&client_thread[i], &attr, Authentication, (void*) (intptr_t)new_fd[i]) < 0){
				perror("Main: Creating thread pool");
			}
			else{
				pthread_mutex_lock(&lock); // Lock mutex
				number_of_connected_client++; // Increase number of connect clients 
				pthread_mutex_unlock(&lock); // Unlock mutex
			}
		
		}
	}
	removeLeaderboardEntries();//reset the leaderboard
	return 0;
}

