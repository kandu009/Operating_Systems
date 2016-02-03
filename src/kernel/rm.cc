/**
 * An mkdir for a simulated file system.
 * @author Ray Ontko -> Converted to C++ by Kwangsung
 */
#include "Kernel.h"
#include "DirectoryEntry.h"
#include "Stat.h"
#include <stdlib.h>
#include <string.h>

int main(int argc, char ** argv)
{
	char PROGRAM_NAME[8];
	strcpy(PROGRAM_NAME, "rm");

	char name[64];
	memset(name, '\0', 64);

	// initialize the file system simulator kernel
	if(Kernel::initialize() == false)
	{
		cout << "Failed to initialized Kernel" << endl;
		Kernel::exit(1);
	}
	// print a helpful message if no command line arguments are given
	if(argc < 2)
	{
		cout << PROGRAM_NAME << ": too few arguments" << endl;
		Kernel::exit( 1 ) ;
	}


	// for each argument given on the command line
	for( int i = 1 ; i < argc; i ++ )
	{
		// given the argument a better name
		strcpy(name, argv[i]);

		// call creat() to create the file
		int status = Kernel::unlink( name ) ;
		
	}

	// exit with success if we process all the arguments
	Kernel::exit( 0 ) ;
}
