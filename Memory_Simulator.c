#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
        int pageNo;
        int modified;
		int lruCount;
		int useBit;
        int trackOldest;
} page;
enum    repl { my_random, fifo, lru, my_clock};
int     createMMU( int);
int     checkInMemory( int ) ;
int     allocateFrame( int ) ;
page    selectVictim( int, enum repl) ;
const   int pageoffset = 12;            /* Page size is fixed to 4 KB */
int 	numFrames;
page * pageTable = NULL;
time_t t;
static int fifoPointer = 0;
int clockHand = 0;

/* Creates the page table structure to record memory allocation */
int     createMMU (int frames)
{

    // Dynamically allocate the pageTable array
    pageTable = (page *)malloc(frames * sizeof(page));
    if (pageTable == NULL) {
        printf("Memory allocation failed.\n");
        exit(-1);
    }

    // Initialize the pageTable
    for (int i = 0; i < frames; i++) {
        pageTable[i].pageNo = -1;  // -1 to show that it's empty
        pageTable[i].modified = 0; // Set value to 0 because no page is initialized at first
        pageTable[i].useBit = 1;   // Set all useBits to recently used
        pageTable[i].lruCount = 0; 
        pageTable[i].trackOldest = 0;
    }

    numFrames = frames;

    return 0;

}

//Increment values that help determine which page has been in the table the longest
void incOldest(){
    for (int i = 0; i < numFrames; i++) {
        if (pageTable[i].pageNo != -1){
            pageTable[i].trackOldest ++;
        }
    }
}

//Returns oldest page in page table
int returnOldest(){
    int pageIdx = 0; // Index of the victim page in the pageTable array
    int oldest = pageTable[0].trackOldest;

    for (int i = 0; i < numFrames; i++){
        if (pageTable[i].trackOldest > oldest){
            oldest = pageTable[i].trackOldest;
            pageIdx = i; // Update the index of the victim page
        }
    }
    return pageIdx;
}





/* Checks for residency: returns frame no or -1 if not found */
int     checkInMemory( int page_number)
{
        int result = -1;

		for (int i=0; i<numFrames; i++) {			
			if (pageTable[i].pageNo == page_number) {
				pageTable[i].useBit = 1; //set use bit to recently used
                result = i;
                break;
			}
		}
        return result ;
}

/* allocate page to the next free frame and record where it put it */
int     allocateFrame( int page_number)
{
        // to do
		int frameNo;
		
		for (int i=0; i<numFrames; i++) {
			
			if (pageTable[i].pageNo == -1) {
				pageTable[i].pageNo = page_number;
				pageTable[i].useBit = 1;  //set use bit to recently used
                pageTable[i].modified = 0;
				frameNo = i;
                break;
			}

		}
        
        clockHand = returnOldest();

        return frameNo;
}

/* Selects a victim for eviction/discard according to the replacement algorithm,  returns chosen frame_no  */
page    selectVictim(int page_number, enum repl  mode )
{
        page    victim;
        // to do 
        victim.pageNo = 0;
        victim.modified = 0;

		srand((unsigned) time (&t)); // https://www.javatpoint.com/random-function-in-c
		int randVal = (rand() % numFrames); //https://www.tutorialspoint.com/c_standard_library/c_function_rand.htm

		if (mode == my_random) {

			victim = pageTable[randVal];
			victim.pageNo = pageTable[randVal].pageNo;
			victim.modified = pageTable[randVal].modified;

		} else if (mode == fifo) {

			victim = pageTable[fifoPointer]; // Select the page at the current FIFO pointer
			fifoPointer = (fifoPointer + 1) % numFrames; // Move the pointer in a circular manner

        } 
        
        else if (mode == lru){

        int lruPageIdx = 0; // Index of the victim page in the pageTable array
        int lruMax = pageTable[0].lruCount;

        for (int i = 0; i < numFrames; i++){

            if (pageTable[i].lruCount > lruMax){

                lruMax = pageTable[i].lruCount;
                lruPageIdx = i; // Update the index of the victim page

            }

        }

        victim = pageTable[lruPageIdx];
		victim.lruCount = 0; 

		} else if (mode == my_clock) {

			int exit = 0;
			while (exit == 0) {

                //If page has not been used recently: Save page into victim, set use bit to 1, increment clock hand, exit
				if (pageTable[clockHand].useBit == 0) {
					victim = pageTable[clockHand];
					victim.pageNo = pageTable[clockHand].pageNo;
					victim.modified = pageTable[clockHand].modified;
					pageTable[clockHand].useBit = 1;
                    clockHand ++;
                    clockHand = clockHand % numFrames;
					exit = 1;

				} else {  //Set use bit to 0, increment clock hand,
					pageTable[clockHand].useBit = 0;
					clockHand ++;
					clockHand = clockHand % numFrames;
					
				}
			}			
		}

        return (victim) ;
}





    







int main(int argc, char *argv[])
{
  
	char	*tracename;
	int	page_number,frame_no, done ;
	int	do_line, i;
	int	no_events, disk_writes, disk_reads;
	int     debugmode;
 	enum	repl  replace;
	int	allocated=0; 
	int	victim_page;
        unsigned address;
    	char 	rw;
	page	Pvictim;
	FILE	*trace;


        if (argc < 5) {
             printf( "Usage: ./memsim inputfile numberframes replacementmode debugmode \n");
             exit ( -1);
	}
	else {
        tracename = argv[1];	
	trace = fopen( tracename, "r");
	if (trace == NULL ) {
             printf( "Cannot open trace file %s \n", tracename);
             exit ( -1);
	}
	numFrames = atoi(argv[2]);
        if (numFrames < 1) {
            printf( "Frame number must be at least 1\n");
            exit ( -1);
        }
        if (strcmp(argv[3], "lru\0") == 0)
            replace = lru;
	    else if (strcmp(argv[3], "rand\0") == 0)
	     replace = my_random;
	          else if (strcmp(argv[3], "clock\0") == 0)
                       replace = my_clock;		 
	               else if (strcmp(argv[3], "fifo\0") == 0)
                             replace = fifo;		 
        else 
	  {
             printf( "Replacement algorithm must be rand/fifo/lru/clock  \n");
             exit ( -1);
	  }

        if (strcmp(argv[4], "quiet\0") == 0)
            debugmode = 0;
	else if (strcmp(argv[4], "debug\0") == 0)
            debugmode = 1;
        else 
	  {
             printf( "Replacement algorithm must be quiet/debug  \n");
             exit ( -1);
	  }
	}
	
	done = createMMU (numFrames);
	if ( done == -1 ) {
		 printf( "Cannot create MMU" ) ;
		 exit(-1);
        }
	no_events = 0 ;
	disk_writes = 0 ;
	disk_reads = 0 ;

        do_line = fscanf(trace,"%x %c",&address,&rw);
	while ( do_line == 2)
	{
		page_number =  address >> pageoffset;
		frame_no = checkInMemory( page_number) ;    /* ask for physical address */

		if ( frame_no == -1 )
		{
		    disk_reads++ ;			/* Page fault, need to load it into memory */
		    if (debugmode) 
		        printf( "Page fault %8d \n", page_number) ;
		    
            if (allocated < numFrames)  			/* allocate it to an empty frame */
		    {
                frame_no = allocateFrame(page_number);
                
                //increments variable to track which page was inserted the earliest 
                incOldest();
		        
                allocated++;
            }
            else{
            
		        Pvictim = selectVictim(page_number, replace) ;   /* returns page number of the victim  */
		        frame_no = checkInMemory( Pvictim.pageNo) ;    /* find out the frame the new page is in */
               
                //increments variable to track which page was inserted the earliest 
                incOldest();

                //Updates page table with new entry
                pageTable[frame_no].pageNo = page_number; 
                pageTable[frame_no].modified = 0; 
			    pageTable[frame_no].useBit = 1; 
                pageTable[frame_no].lruCount = 0;
                pageTable[frame_no].trackOldest = 0;

            

                if (Pvictim.modified)           /* need to know victim page and modified  */
                    {
                            disk_writes++;			    
                            if (debugmode) printf( "Disk write %8d \n", Pvictim.pageNo) ;
                    }
                else
                            if (debugmode) printf( "Discard    %8d \n", Pvictim.pageNo) ;
		   }
		}


		if ( rw == 'R'){
            
		    if (debugmode) printf( "reading    %8d \n", page_number) ;
		}
		else if ( rw == 'W'){
		    // mark page in page table as written (modified) 
            int pageIndex = checkInMemory(page_number);
            pageTable[pageIndex].modified = 1; 
		    if (debugmode) printf( "writting   %8d \n", page_number) ;
            

            
		}
		 else {
		      printf( "Badly formatted file. Error on line %d\n", no_events+1); 
		      exit (-1);
		}


        //increment LRU 
        for (int i=0; i<numFrames; i++) {
            if(pageTable[i].pageNo != -1){
                pageTable[i].lruCount++ ;
            }
        }
        //Reset Current Frame LRU
        if (frame_no != -1){
            pageTable[frame_no].lruCount = 0;
        }


		no_events++;
        	do_line = fscanf(trace,"%x %c",&address,&rw);

	}

	printf( "total memory frames:  %d\n", numFrames);
	printf( "events in trace:      %d\n", no_events);
	printf( "total disk reads:     %d\n", disk_reads);
	printf( "total disk writes:    %d\n", disk_writes);
	printf( "page fault rate:      %.4f\n", (float) disk_reads/no_events);
}
				
