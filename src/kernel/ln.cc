#include "Kernel.h"
#include "DirectoryEntry.h"
#include "Stat.h"
#include <stdlib.h>
#include <string.h>
#include <cstdlib>

int main(int argc, char ** argv)
{
	char PROGRAM_NAME[8];
	strcpy(PROGRAM_NAME, "ln");

	// initialize the file system simulator kernel
	if(Kernel::initialize() == false)
	{
		cout << "Failed to initialized Kernel" << endl;
		Kernel::exit(1);
	}
	// print a helpful message if no command line arguments are given
	if(argc != 3)
	{
		cout << PROGRAM_NAME << ": wrong number of arguments" << endl;
		Kernel::exit( 1 ) ;
	}

	// create a buffer for writing directory entries
	char directoryEntryBuffer[DirectoryEntry::DIRECTORY_ENTRY_SIZE];
	memset(directoryEntryBuffer, '\0', DirectoryEntry::DIRECTORY_ENTRY_SIZE);

	char pathname1[512];
	char pathname2[512];
	// for each argument given on the command line
	strcpy(pathname1, argv[1]);
	strcpy(pathname2, argv[2]);

	int status;

	status = Kernel::link(pathname1,pathname2);

	if(status<0){
		//Kernel::perror( PROGRAM_NAME );
		cout << PROGRAM_NAME <<": Unable to create link. Link already exists." << endl;
		Kernel::exit( 2 );
	}

	// exit with success if we process all the arguments
	Kernel::exit( 0 ) ;
}
