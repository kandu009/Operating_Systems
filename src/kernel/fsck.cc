#include "Kernel.h"
#include "IndexNode.h"
#include "BitBlock.h"
#include "FileSystem.h"
#include <stdlib.h>
#include <string.h>

int *nlinks;
int *inode_nlinks;
void countNlinks(char* parentdir, IndexNode cIndexNode);
void checkBlocks(char* parentdir, IndexNode cIndexNode);
int maxinodenum = 0;

int blockSize = 0;
int blockCount = 0;

bool *blocks;
int dataBlockOffset = 10;
BitBlock *freeListBitBlock;

int main(){

	if(Kernel::initialize() == false)
	{
		cout << "Failed to initialized Kernel" << endl;
		Kernel::exit(1);
	}

	
	blockCount = Kernel::openFileSystems->getBlockCount();
	blockSize = Kernel::openFileSystems->getBlockSize();

	freeListBitBlock = Kernel::openFileSystems->getFreeListBitBlock();

	blocks = new bool[blockCount];

	nlinks =  new int[1024];
	inode_nlinks = new int[1024];
	for(int i=0; i<1024; i++){
		nlinks[i] = 0;
		inode_nlinks[i] = 0;
	}
	for(int i=0; i<blockCount; i++){
		blocks[i] = false;
	}
	char parentdir[1024];
	memset(parentdir, '\0', 1024);
	strcpy(parentdir, "/" );
	IndexNode currIndexNode;
	Kernel::openFileSystems->getRootIndexNode()->copy(currIndexNode);
	countNlinks(parentdir,currIndexNode);
	checkBlocks(parentdir,currIndexNode);

	cout << "................................Nlinks count..............................." << "\n" << endl;
	for(int i=0; i<=maxinodenum; i++){
		if(nlinks[i]!=inode_nlinks[i]);
		if(nlinks[i] !=0){
			cout << "inode number: " << i << "     from Directory entries: " << nlinks[i] << ", from index node : " << inode_nlinks[i] << endl;
		}
	}

	cout << "\n" << ".............................*****..............................." << "\n" << endl;

	cout << ".............................Block allocations..........................." << endl;
	cout << " 	Total number of blocks: " << blockCount << ". if Block is allocated, Allocation is 1. Else 0" << "\n" << endl;
	for(int i=0; i<blockCount; i++){
		Kernel::openFileSystems->loadFreeListBlock(i);
		bool alloc = freeListBitBlock->isBitSet( 
				i % (blockSize * 8));
		if(blocks[i] == alloc){
			cout << "Block " << i << " has good state. Allocation is: " << alloc << endl;
		}
		else{
			cout << "Block " << i << " has bad state. Allocation is: " << alloc << endl;
		}
	}

	cout << "\n" << ".............................*****..............................." << "\n" << endl;
	
}

void countNlinks(char* parentdir, IndexNode cIndexNode){
	IndexNode currIndexNode = cIndexNode;
	IndexNode emptyIndexNode;
	IndexNode prevIndexNode;
	int dir;
	int read_status = 0 ;
	DirectoryEntry currentDirectoryEntry;
	dir = Kernel::open(parentdir , Kernel::O_RDWR);
	while(true){
		read_status = Kernel::readdir(dir, currentDirectoryEntry);
		if(read_status < 0)
		{
			cout << Kernel::PROGRAM_NAME << ": error reading directory in link";
			exit( EXIT_FAILURE ) ;
		}
		else if( read_status == 0 )
		{
			// if no entry read, write the new item at the current 
			// location and break
			break ;
		}
		currIndexNode = cIndexNode;
		currIndexNode.copy(prevIndexNode);
		emptyIndexNode.copy(currIndexNode);
		int indexNodeNumber = Kernel::findNextIndexNode(Kernel::openFileSystems, prevIndexNode, currentDirectoryEntry.getName(), currIndexNode);

		if(indexNodeNumber < 0){

		}
		else if((currIndexNode.getMode()&Kernel::S_IFMT) != Kernel::S_IFDIR)
		{
			if(maxinodenum < indexNodeNumber){
				maxinodenum = indexNodeNumber;
			}
			nlinks[indexNodeNumber]++;
			if(inode_nlinks[indexNodeNumber] == 0){
				inode_nlinks[indexNodeNumber] = currIndexNode.getNlink();
			}
		}
		else if(strcmp(currentDirectoryEntry.getName(),".")!=0 && strcmp(currentDirectoryEntry.getName(),"..")!=0){
			char childdir[1024];
			memset(childdir, '\0', 1024);
			strcpy(childdir,"");
			strcat(childdir,parentdir);
			strcat(childdir,"/");
			strcat(childdir,currentDirectoryEntry.getName());
			strcat(childdir,"/");
			countNlinks(childdir, currIndexNode);
		}	
	}
}

void checkBlocks(char* parentdir, IndexNode cIndexNode){
	IndexNode currIndexNode = cIndexNode;
	IndexNode emptyIndexNode;
	IndexNode prevIndexNode;
	int dir;
	int read_status = 0 ;
	DirectoryEntry currentDirectoryEntry;
	dir = Kernel::open(parentdir , Kernel::O_RDWR);
	while(true){
		read_status = Kernel::readdir(dir, currentDirectoryEntry);
		if(read_status < 0)
		{
			cout << Kernel::PROGRAM_NAME << ": error reading directory in link";
			exit( EXIT_FAILURE ) ;
		}
		else if( read_status == 0 )
		{
			// if no entry read, write the new item at the current 
			// location and break
			break ;
		}
		currIndexNode = cIndexNode;
		currIndexNode.copy(prevIndexNode);
		emptyIndexNode.copy(currIndexNode);
		int indexNodeNumber = Kernel::findNextIndexNode(Kernel::openFileSystems, prevIndexNode, currentDirectoryEntry.getName(), currIndexNode);

		if(indexNodeNumber < 0){

		}
		else{
			for(int i=0; i<74; i++){
				int blockAddress = currIndexNode.getBlockAddress(Kernel::openFileSystems->file,i);
				if(blockAddress == FileSystem::NOT_A_BLOCK){
				}
				else{
					blocks[blockAddress] = true;
				}
			}
			int indir = currIndexNode.getIndirectBlock();
			if(indir != FileSystem::NOT_A_BLOCK){
				blocks[indir] = true;
			}
			if((currIndexNode.getMode()&Kernel::S_IFMT) != Kernel::S_IFDIR){

			}
			else if(!strcmp(currentDirectoryEntry.getName(),".") && !strcmp(currentDirectoryEntry.getName(),"..")){
				char childdir[1024];
				memset(childdir, '\0', 1024);
				strcpy(childdir,"");
				strcat(childdir,parentdir);
				strcat(childdir,"/");
				strcat(childdir,currentDirectoryEntry.getName());
				strcat(childdir,"/");
				checkBlocks(childdir, currIndexNode);
			}	

		}
		
	}
}
