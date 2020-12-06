/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "debug.h"
#include "sfmm.h"
 // !!! 4 words (i.e. 64 bits or 1 memory row or 8 bytes) !!!
int last_block_allocated = 0;
/* these vars count how many items are in each quick list */
int quick0 = 0;
int quick1 = 0;
int quick2 = 0;
int quick3 = 0;
int quick4 = 0;
int quick5 = 0;
int quick6 = 0;
int quick7 = 0;
int quick8 = 0;
int quick9 = 0;

/* this helper is for initializing the free list */
void initialize_free_lists(){

	for (int i = 0; i < NUM_FREE_LISTS; i++) {
			// make the sentinals point to themselves first
		    sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
		    sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
		}
}

/* this clears the alloc'd bits in a header so you can see actual size*/
size_t clear_bits(sf_block *block){
	size_t size = (block->header^MAGIC);
	size &= ~(1 << 1);
	size &= ~(1 << 2);
	return size;
}

/* helps get the aligned block */
size_t get_aligned_block(size_t size){
	// block is 8 bytes + the payload you'd need

	size += 8; //add the 8 bytes for header
	if(size < 32){ //if the block size is less than 32, add padding to fit min req.
		size += (32 - size);
	}
	else {
		if(size % 16 != 0){ //if the block size is not 16-byte aligned, make it aligned
			size += (16 - (size%16));
		}
	}
	// return 16-byte aligned block
	return size;
}

/* helps get the index of the quick list we should start at. (returns -1 if block is too big) */
int get_quick_list_index(size_t size){
	switch(size){
		case 32:
			return 0; //index 0 of quick list
		case 48:
			return 1;
		case 64:
			return 2;
		case 80:
			return 3;
		case 96:
			return 4;
		case 112:
			return 5;
		case 128:
			return 6;
		case 144:
			return 7;
		case 160:
			return 8;
		case 176:
			return 9;
		default:
			return -1; // if the block size is too big then we won't check quick lists
	}
}

/* return the index of the appropriate free list */
int get_free_list_index(size_t size){
	if(size == 32)
		return 0; //the first index of free list for min size blocks
	else if(size > 32 && size <= 64)
		return 1;
	else if(size > 64 && size <= 128)
		return 2;
	else if(size > 128 && size <= 256)
		return 3;
	else if(size > 256 && size <= 512)
		return 4;
	else if(size > 512 && size <= 1024)
		return 5;
	else if(size > 1024 && size <= 2048)
		return 6;
	else if(size > 2048 && size <= 4096)
		return 7;
	else if(size > 4096 && size <= 8192)
		return 8;
	else
		return 9; //the size is bigger than any of the previous
}

/* returns the size from the block after reading its header */
size_t get_block_size_from_header(sf_block *block){
	size_t blk_sz = block->header;
	blk_sz ^= MAGIC; //take off mask to see true contents
	return blk_sz;
}

/* removes block from free list and returns its pointer */
sf_block* remove_from_free_list(sf_block* free_list_head, size_t actual_block_size){
	sf_block* block = free_list_head->body.links.next;
	if(block ==  free_list_head->body.links.prev){ //meaning there is only one element in list
		free_list_head->body.links.next = free_list_head; //dummy.prev = dummyhead
		free_list_head->body.links.prev = free_list_head; //dummy.next = dummyhead
	}
	else{ //find the block then remove and return it
		while((block->header^MAGIC) != actual_block_size){
			block = block->body.links.next;
		}
		sf_block *temp = block->body.links.prev; //hold prev
		block->body.links.prev->body.links.next = block->body.links.next; //set prev's next to next
		block->body.links.next->body.links.prev = temp; //set next's prev to prev

	}
	//set links to null?
	block->body.links.next = NULL;
	return block;
}

/* update header */
void update_header(sf_block *block, size_t needed_block_size, int allocate, int prev_alloc){
	size_t header = block->header;
	header ^= MAGIC; //unmask
	header = needed_block_size;
	debug("header size: %zu", header);
	if(allocate == 1)
		header |= THIS_BLOCK_ALLOCATED;
	if(prev_alloc == 1)
		header |= PREV_BLOCK_ALLOCATED;
	header ^= MAGIC; //mask
	block->header = header;
}

/* returns the block that can fit the required size */
int get_from_free_list(sf_block *free_list_head, sf_block **block, size_t needed_block_size, int* found_block){
	sf_block *blk = free_list_head->body.links.next;
	while(blk != free_list_head){
		if((blk->header^MAGIC) >= needed_block_size){ //if the free block is big enough, return
			*found_block = 1;
			*block = blk;
			// debug("block's header: %zu", *block->header^MAGIC);
			return 1;
		}
		blk = blk->body.links.next;
	}
	return -1; //this means that the function did not find a big enough free block
}

/* inserts into a free list */
void insert_to_free_list(sf_block *block) {
	size_t size = (block->header^MAGIC); //unmask
	size &= ~(1 << 1);
	debug("size when inserting to free list: %zu", size);
	int index = get_free_list_index(size);
	debug("index to be inserted to: %d", index);
	if(sf_free_list_heads[index].body.links.next == &sf_free_list_heads[index]){ //empty list rn
		debug("empty free list");
		sf_free_list_heads[index].body.links.next = block;
		sf_free_list_heads[index].body.links.prev = block;
		block->body.links.next = &sf_free_list_heads[index];
		block->body.links.prev = &sf_free_list_heads[index];
	}
	else{//there is at least 1 element in the list
		debug("non empty free list");

		block->body.links.next = sf_free_list_heads[index].body.links.next; //adding to beginning of list
		block->body.links.prev = &sf_free_list_heads[index];
		block->body.links.next->body.links.prev = block; //set the prev also
		sf_free_list_heads[index].body.links.next = block;
	}
	// sf_show_heap();
	debug("done adding");

}

/* adds footer to free block */
void add_footer(sf_block *block){
	// debug("masked header: %zu", block->header);
	size_t header = block->header^MAGIC;
	// debug("header in add_footer: %zu", header);
	header &= ~(1 << 2); // clear these bits to properly get to next block
	header &= ~(1 << 1);
	char* temp = (char *)block;
	temp += header;
	sf_block *next = (sf_block*) temp;
	next->prev_footer = block->header;
	// debug("next block's footer: %zu", next->prev_footer);
}

void clear_prev_alloc_in_next_block(sf_block *block){
	size_t header = block->header^MAGIC;
	header &= ~(1 << 2); // clear these bits to properly get to next block
	header &= ~(1 << 1);
	char* temp = (char *)block;
	temp += header;
	sf_block *next = (sf_block*) temp;
	debug("address of next: %p", next);
	debug("address of end of heap: %p", sf_mem_end());
	header = next->header^MAGIC;
	header &= ~(1 << 1); //reset prev alloc bit
	next->header = header^MAGIC;
	if((header & THIS_BLOCK_ALLOCATED)>>2 == 0){
		//if this block is free, update footer
		add_footer(next);
	}
}

/* checks if this allocated block is the lost block in heap */
int check_last_block(sf_block *block){
	size_t header = block->header^MAGIC;
	header &= ~(1 << 2); // clear these bits to properly get to next block
	header &= ~(1 << 1);
	char* temp = (char *)block;
	temp += header;
	sf_block *next = (sf_block*) temp;
	if(next == sf_mem_end()-16)
		return 1;
	else
		return 0;

}

/* pops from the head of quick list */
sf_block* remove_from_quick_list(sf_block *head, int index){
	sf_block *block = head;
	if(head->body.links.next == NULL){ //there is only one element in the quick list
		sf_quick_lists[index].first = NULL; //make list empty
		debug("there's one item in quick list");
	}
	else{
		sf_quick_lists[index].first = block->body.links.next; //set head to the next node
	}

	//remove next link??
	block->body.links.next = NULL;

	return block;
}

/* allocated block w/o splitting */
sf_block* allocate_no_splitting(int index, size_t actual_block_size){
	debug("this block cannot be split");
	//remove from free list
	sf_block *block = remove_from_free_list(&sf_free_list_heads[index], actual_block_size);
	update_header(block, actual_block_size, 1, ((block->header^MAGIC)&PREV_BLOCK_ALLOCATED)>>1);

	char* temp = (char*) block;
	size_t header = block->header^MAGIC;
	header &= ~(1 << 2); // clear these bits to properly get to next block
	header &= ~(1 << 1);
	temp += header;

	sf_block *next = (sf_block*)temp; // need to set the prev_alloc of next block
	if(!check_last_block(next)){ //if this is not the last block in heap
		update_header(next, (next->header^MAGIC), 0, 1); //set prev_alloc for next block
	}
	else{
		last_block_allocated = 1;
	}
	return block;
}

sf_block* allocate_splitting(int index, size_t actual_block_size, size_t needed_block_size){
	//first remove this block from the circular linked list
	sf_block* block = remove_from_free_list(&sf_free_list_heads[index], actual_block_size); //this block we will allocate
	//spltting begins: make lower part the block to be allocated, upper for new free block
	debug("successfully removed block size: %zu", block->header^MAGIC);
	debug("the prev_alloc bit is: %ld", (block->header^MAGIC)>>1);
	update_header(block, needed_block_size, 1, (((block->header^MAGIC)&PREV_BLOCK_ALLOCATED)>>1)); // set alloc bit to 1

	char* temp = (char*) block;
	size_t header = block->header^MAGIC;
	header &= ~(1 << 2); // clear these bits to properly get to next block
	header &= ~(1 << 1);
	debug("bits to skip: %zu", header);
	debug("address of first: %p", temp);
	temp += header;
	sf_block *new_free_block = (sf_block*)temp;
	debug("address of new: %p", new_free_block);
	debug("actual_block_size: %zu", actual_block_size);
	debug("needed_block_size: %zu", needed_block_size);

	update_header(new_free_block, (actual_block_size-needed_block_size), 0, 1); //set prev_alloc to 1
	//now set the prev_footer field? - no

	add_footer(new_free_block);
	//insert new free block to appropriate free list
	insert_to_free_list(new_free_block);
	// sf_show_free_lists();
	return block; //return allocated block
}

void decrement_quick_list_counter(int index){
	switch(index){
		case 0:
			quick0--;
			break;
		case 1:
			quick1--;
			break;
		case 2:
			quick2--;
			break;
		case 3:
			quick3--;
			break;
		case 4:
			quick4--;
			break;
		case 5:
			quick5--;
			break;
		case 6:
			quick6--;
			break;
		case 7:
			quick7--;
			break;
		case 8:
			quick8--;
			break;
		case 9:
			quick9--;
			break;

	}
}

sf_block* quick_list_allocate(int index, int needed_block_size){
	sf_block *block = remove_from_quick_list(sf_quick_lists[index].first, index);
	decrement_quick_list_counter(index); //decrement the counts
	//set allocated bit and return payload
	update_header(block, needed_block_size, 1, ((block->header^MAGIC)&PREV_BLOCK_ALLOCATED)>>1);

	char* temp = (char*) block;
	size_t header = block->header^MAGIC;
	header &= ~(1 << 2); // clear these bits to properly get to next block
	header &= ~(1 << 1);
	temp += header;

	sf_block *next = (sf_block*)temp; // need to set the prev_alloc of next block
	if(!check_last_block(next)){ //if this is not the last block in heap
		update_header(next, (next->header^MAGIC), 0, 1); //set prev_alloc for next block
	}
	else{
		last_block_allocated = 1;
	}
	return block;

}

void *sf_malloc(size_t size) {
	size_t needed_block_size;
	int quick_index;
	int found_block = 0; // flag to see if we found the block we were looking for
	size_t actual_block_size; //when we read the header of the block we are abt to allocate
	sf_block *block = &sf_free_list_heads[0]; //temporarily set

	if(size == 0){
		return NULL; //without setting errno
	}

	// After each call to sf_mem_grow, you must attempt to coalesce the newly allocated page with any free
	// block immediately preceding it, in order to build blocks larger than one page.
	// Insert the new block at the beginning of the appropriate main freelist (not a quick list)
	if(sf_mem_start() == sf_mem_end()){ //this means that heap is uninitialized on the first malloc
		debug("heap is unitialized, need to grow heap");
		void* expand = sf_mem_grow();
		sf_block *start = (expand); // skip first row
		size_t first_header = ((size_t)4080);
		first_header ^= MAGIC;
		debug("masked header 1: %zu", first_header);


		start->prev_footer = ((size_t)0^MAGIC); //idk if this is right yet
		start->header = first_header;
		add_footer(start);

		initialize_free_lists();
		// a block size of 4080 goes in index 7 of free list heads (2048, 4096]
		if(start != NULL){
			start->body.links.next = &sf_free_list_heads[7];
			start->body.links.prev = &sf_free_list_heads[7];
			sf_free_list_heads[7].body.links.next = start;
			sf_free_list_heads[7].body.links.prev = start;
		}
		block = start;
	}
	// sf_show_heap();

	// now that the heap is initialized, first determine size of block necessary
	needed_block_size = get_aligned_block(size);
	//now we have a 16-byte aligned block
	debug("\n\nblock size determined is: %zu", needed_block_size);
	debug("checking quick list...");

	//check what index for quick lists
	quick_index = get_quick_list_index(needed_block_size);
	if(quick_index != -1 && sf_quick_lists[quick_index].first != NULL){
		debug("quick list found: [%d]", quick_index);
		//check the quick list at the index the function returns
		//if is not null, pop from head of linked list index (quick index)
		found_block = 1;
		block = quick_list_allocate(quick_index, needed_block_size);

		return block->body.payload;

	}
	debug("cannot be in quick list, searching free lists..."); // so go directly to free list bc if it was in quick list it would have returned
	//determine the index of the first free list that would satisfy the request
	quick_index = get_free_list_index(needed_block_size);
	debug("the appropriate free list to start at is: %d", quick_index);
	for(int i = quick_index; i < NUM_FREE_LISTS; i++){
		//if no block is found in this list, move onto larger class size
		if(sf_free_list_heads[i].body.links.prev == &sf_free_list_heads[i] &&
			sf_free_list_heads[i].body.links.next == &sf_free_list_heads[i]){
			// a free list is empty when a sentinel points to itself both ways
			// debug("not found yet");
			continue; //move onto next class
		}
		else{ // found a block, check if it can fit the required size
			quick_index = i;
			if(get_from_free_list(&sf_free_list_heads[i], &block, needed_block_size, &found_block)==1){
				debug("found block");
				break;
			}
			else{
				continue;
			}
		}
	}


	if(found_block == 1){ // at this point we have the block to (maybe split) and return its payload
		// debug("we found a suitable block at index %d", quick_index);
		//first need to check if we can split the block w/o a splinter
		actual_block_size = get_block_size_from_header(block);
		debug("the size of suitable block is: %zu at index %d", actual_block_size, quick_index);
		if(actual_block_size - needed_block_size < 32){
			//cannot split this block, allocate it as is
			debug("no splitting");
			block = allocate_no_splitting(quick_index, actual_block_size);

			return block->body.payload;
		}
		else{
			debug("splitting");
			block = allocate_splitting(quick_index, actual_block_size, needed_block_size);
			// sf_show_heap();
			debug("success");
			return block->body.payload; //return payload of allocated block

		}

	}
	else{ //otherwise if found_block is still 0; we need more heap space
		//then here check for the other errno error

		while(1){
			// sf_show_heap();
			debug("\n\nprevious end of heap: %p", sf_mem_end());
			void *expand = sf_mem_grow();

			if(expand == NULL){ // no more mem left
				sf_errno = ENOMEM;
				return NULL;
			}
			// debug("expand is valid");

			sf_block *new_block = expand-16;
			debug("%zu", new_block->prev_footer);
			size_t header = 4096;
			header ^= MAGIC;
			new_block->header = header;
			add_footer(new_block);

			if(last_block_allocated){
				//set prev alloc to 1
				size_t h = new_block->header^MAGIC;
				h |= PREV_BLOCK_ALLOCATED;
				new_block-> header = (h^MAGIC);
				last_block_allocated = 0;
			}
			else{
				//coalesce
				//remove the previous free block from free list
				sf_block* combined = remove_from_free_list(&sf_free_list_heads[get_free_list_index((new_block->prev_footer^MAGIC))], (new_block->prev_footer^MAGIC));
				size_t h = (combined->header^MAGIC) + (new_block->header^MAGIC);
				debug("combined: %zu", h);
				combined->header = (h^MAGIC);
				add_footer(combined);
				insert_to_free_list(combined);
				new_block = combined;

			}

			actual_block_size = get_block_size_from_header(new_block);

			if(needed_block_size > actual_block_size) //if block is not big enough, get more space
				continue;

			if(actual_block_size - needed_block_size < 32){
				new_block = allocate_no_splitting(get_free_list_index(actual_block_size), actual_block_size);

			}
			else{
				new_block = allocate_splitting(get_free_list_index(actual_block_size), actual_block_size, needed_block_size);

			}
			// sf_show_heap();
			return new_block->body.payload;
		}


	}

	//it shouldn't ever reach this point
	debug("huhhhh");
    return NULL;
}

/* returns 0 if pointer is valid */
int validate_pointer(void *pp){
	if(pp == NULL)
		return -1;
	if(((unsigned long) pp & 15) != 0) // if pointer address is not aligned to 16bytes
		return -1;
	pp -= 16;
	sf_block *block = (sf_block *) pp;

	if(pp < sf_mem_start()) //if the address of the start of block is before start of heap
		return -1;



	size_t header = (block->header^MAGIC);

	if((header & THIS_BLOCK_ALLOCATED) == 0) //allocated bit is 0
		return -1;

	header &= ~(1 << 1); //clear allocated bits to check if actual size is 16 aligned
	header &= ~(1 << 2);
	if(header < 32) // block size is at least 32 bytes
		return -1;
	if(header % 16 != 0) //if block size is not multiple of 16;
		return -1;

	char *temp = (char*)pp;
	temp += header;
	if((void*)temp > sf_mem_end()) //if address of footer is past end of heap
		return -1;

	//not entirely sure about this
	if(pp != sf_mem_start()){ //as long as the ptr is not to the first block, we can check prev block
		int prev_alloc = (block->header^MAGIC) & PREV_BLOCK_ALLOCATED; //1 if prev is alloc'd, 0 if not
		int prev_block_header_alloc_bit = (block->prev_footer^MAGIC) & THIS_BLOCK_ALLOCATED;
		if(prev_alloc == 0 && prev_block_header_alloc_bit == 1){
			return -1;
		}

	}


	return 0;
}

/* returns the index if can fit in quick list, -1 if not */
int fit_in_quick_list(size_t size){
	switch(size){
		case 32:
			return 0;
		case 48:
			return 1;
		case 64:
			return 2;
		case 80:
			return 3;
		case 96:
			return 4;
		case 112:
			return 5;
		case 128:
			return 6;
		case 144:
			return 7;
		case 160:
			return 8;
		case 176:
			return 9;
		default:
			return -1;
	}
}

/* increments the counter for quick list if not at capacity (0 on success, -1 on failure)*/
int increment_quick_list_counter(int index){
	switch(index){
		case 0:
			if(quick0 < 5){
				quick0++;
				return 0;
			} else{
				return -1;
			}
		case 1:
			if(quick1 < 5){
				quick1++;
				return 0;
			} else{
				return -1;
			}
		case 2:
			if(quick2 < 5){
				quick2++;
				return 0;
			} else{
				return -1;
			}
		case 3:
			if(quick3 < 5){
				quick3++;
				return 0;
			} else{
				return -1;
			}
		case 4:
			if(quick4 < 5){
				quick4++;
				return 0;
			} else{
				return -1;
			}
		case 5:
			if(quick5 < 5){
				quick5++;
				return 0;
			} else{
				return -1;
			}
		case 6:
			if(quick6 < 5){
				quick6++;
				return 0;
			} else{
				return -1;
			}
		case 7:
			if(quick7 < 5){
				quick7++;
				return 0;
			} else{
				return -1;
			}
		case 8:
			if(quick8 < 5){
				quick8++;
				return 0;
			} else{
				return -1;
			}
		case 9:
			if(quick9 < 5){
				quick9++;
				return 0;
			} else{
				return -1;
			}

	}
	return -1;
}

/* inserts to quick list like a stack to the top */
void insert_to_quick_list(sf_block *block, int index){
	// if(sf_quick_lists[index].first == NULL){ //if quick list is empty
	// 	block->body.links.next = NULL;

	// }
	block->body.links.next = sf_quick_lists[index].first;
	sf_quick_lists[index].first = block;


}

sf_block* get_next_block(sf_block *block){
	size_t header = block->header^MAGIC; //this is correct here
	header &= ~(1 << 2);
	header &= ~(1 << 1);
	debug("size of current block is %zu", header);
	char* temp = (char *)block;
	temp += header;
	sf_block *next = (sf_block*) temp;
	debug("header in next_block is %zu", next->header^MAGIC);
	return next;
}

int get_next_block_alloc_bit(sf_block *block){
	sf_block *next = get_next_block(block);
	if((void*)next > sf_mem_end()){
		return -1;
	}

	return ((next->header^MAGIC) & THIS_BLOCK_ALLOCATED) >> 2;

}

void coalesce(sf_block **block){
	debug("inside coalesce");
	debug("%zu", (*block)->header^MAGIC);
	int prev_alloc = (((*block)->header^MAGIC) & PREV_BLOCK_ALLOCATED)>>1;
	int next_alloc = get_next_block_alloc_bit(*block);
	if(next_alloc == -1){ //meaning we passed the end of the heap
		next_alloc = 1;
	}

	sf_block *next, *combined;
	size_t prev_size, next_size;
	if(((*block)->prev_footer^MAGIC) == 0){
		prev_alloc = 1;
	}
	debug("the block to be freed is size: %zu", ((*block)->header^MAGIC));
	debug("prev and next alloc are: %d, %d", prev_alloc, next_alloc);

	if(!prev_alloc && !next_alloc){ //coalesce from both sides
		debug("coalesce from both sides");
		prev_size = (*block)->prev_footer^MAGIC;

		prev_size &= ~(1 << 1);
		next = get_next_block(*block);
		next_size = clear_bits(next);
		debug("prev and next block sizes are: %zu, %zu", prev_size, next_size);
		size_t combined_size = prev_size + ((*block)->header^MAGIC) + next_size;
		//remove prev and next blocks from free list
		combined = remove_from_free_list(&sf_free_list_heads[get_free_list_index(prev_size)], ((*block)->prev_footer^MAGIC));
		remove_from_free_list(&sf_free_list_heads[get_free_list_index(next_size)], get_next_block(*block)->header^MAGIC);
		combined->header = (combined_size^MAGIC);
		add_footer(combined);
		*block = combined;
		return;

	}
	else if(!prev_alloc && next_alloc){ //coalesce w/ prev block only
		debug("coalesce w/ prev block only");
		prev_size = (*block)->prev_footer^MAGIC;
		prev_size &= ~(1 << 1);
		debug("prev size is %zu", prev_size);
		size_t combined_size = prev_size + ((*block)->header^MAGIC);
		//just remove prev block from free list
		combined = remove_from_free_list(&sf_free_list_heads[get_free_list_index(prev_size)], ((*block)->prev_footer^MAGIC));
		combined->header = (combined_size^MAGIC);
		add_footer(combined);
		*block = combined;
		return;

	}
	else if(prev_alloc && !next_alloc){ //coalesce w/ next block only
		debug("coalesce w/ next block only");
		next = get_next_block(*block);
		next_size = clear_bits(next);
		size_t combined_size = ((*block)->header^MAGIC) + next_size;
		combined = *block; //earlier address
		remove_from_free_list(&sf_free_list_heads[get_free_list_index(next_size)], get_next_block(*block)->header^MAGIC);
		combined->header = (combined_size^MAGIC);
		add_footer(combined);
		*block = combined;
		return;

	}
	else{ //dont coalesce
		debug("dont coalesce");
		return;
	}

}


void sf_free(void *pp) {
	debug("inside free");

	if(validate_pointer(pp)){
		debug("invalid pointer!");
		abort();
	}
	pp -= 16;
	//pointer is validated
	debug("\n\nvalid pointer");
	sf_block *block = (sf_block*) pp;
	size_t size = (block->header^MAGIC);
	size &= ~(1 << 1);
	size &= ~(1 << 2);

	//what does free mean?
	int index = fit_in_quick_list(size);
	debug("index to be inserted to in qList: %d", index);

	if(index != -1){ //put into quick list and do not set as unalloc'd
		if(!increment_quick_list_counter(index)){ //do not neet to flush quick list yet
			insert_to_quick_list(block, index);
			// sf_show_quick_lists();
			return;
		}
		else{ //flush quick list
		// adding them to one of the main freelists, coalescing at that time if possible
			while(sf_quick_lists[index].first != NULL){
				debug("\n\nflushing quick list");
				sf_block *temp = remove_from_quick_list(sf_quick_lists[index].first, index);
				size_t temp_size = clear_bits(temp);
				debug("temp size is %zu", temp_size);
				update_header(temp, temp_size, 0, ((temp->header^MAGIC) & PREV_BLOCK_ALLOCATED)>>1);
				add_footer(temp);
				clear_prev_alloc_in_next_block(temp);
				coalesce(&temp);
				debug("size of coalesced block: %zu", (temp->header^MAGIC)& ~0xf);
				clear_prev_alloc_in_next_block(temp);
				insert_to_free_list(temp);

			}
			//now insert original block to quick list
			debug("\n\nflushing done");
			insert_to_quick_list(block, index);
			// sf_show_heap();


			return;
		}

	}
	else { //mark block as free, add footer, coalesce, insert to free list
		debug("prev alloc is %ld", ((block->header^MAGIC) & PREV_BLOCK_ALLOCATED)>>1);
		update_header(block, size, 0, ((block->header^MAGIC) & PREV_BLOCK_ALLOCATED)>>1);
		debug("header set in free");
		add_footer(block);
		//clear next block's prev_alloc bit;
		clear_prev_alloc_in_next_block(block);
		debug("footer added in free");
		coalesce(&block);
		debug("coalesced in free");
		insert_to_free_list(block);
		debug("inserted to free list in free");

		clear_prev_alloc_in_next_block(block);
		debug("next block's prev set in free");
		// sf_show_heap();

	}

    return;
}

void *sf_realloc(void *pp, size_t rsize) {
	debug("\n\ninside sf_realloc");

	if(validate_pointer(pp)){
		debug("invalid pointer!");
		abort();
	}
	pp-=16;
	if(rsize == 0){
		debug("size is 0, freeing block");
		pp += 16;
		sf_free(pp);
		return NULL;
	}
	debug("parameters validated");

	sf_block *block = (sf_block*) pp;

	size_t actual_block_size = (block->header^MAGIC);
	actual_block_size &= ~(1 << 1);
	actual_block_size &= ~(1 << 2);

	// debug("size of current block is %zu", actual_block_size);
	// debug("needed size is %zu", rsize);

	if(rsize > actual_block_size){ //realloc a larger block
		debug("reallocing a larger block");
		char* temp = sf_malloc(rsize);
		if(temp == NULL){ //if malloc returns null
			return NULL;
		}

		temp -= 16;// i want the entire block
		sf_block *new_block = (sf_block*) temp;

		debug("the size of the payload of the original block is %zu", actual_block_size-8);
		memcpy(new_block->body.payload, block->body.payload, (actual_block_size-8));
		pp+= 16;
		sf_free(pp);
		// sf_show_heap();
		return new_block->body.payload;

	}
	else{ //realloc to smaller block
		debug("reallocing a smaller block");
		size_t smaller_header = get_aligned_block(rsize);
		debug("the difference is %zu", actual_block_size - smaller_header);

		if(actual_block_size - smaller_header < 32){ //will not split
			debug("realloc w/ no splitting"); //basically return same block

			return block->body.payload;

		}
		else{
			debug("realloc w/ splitting");
			update_header(block, smaller_header, 1, ((block->header^MAGIC)&PREV_BLOCK_ALLOCATED)>>1);
			char *temp = (char*)block;
			temp += smaller_header;

			sf_block *new_block = (sf_block*) temp;
			update_header(new_block, (actual_block_size-smaller_header), 0, 1); //new block is free
			//coalesce only with next block if you can
			if(get_next_block_alloc_bit(new_block) == 0){
				sf_block *next = get_next_block(new_block);
				size_t next_size = next->header^MAGIC;
				next_size &= ~(1 << 1);
				next_size &= ~(1 << 2);

				size_t combined_size = (actual_block_size-smaller_header) + next_size;

				remove_from_free_list(&sf_free_list_heads[get_free_list_index(next_size)], (next->header^MAGIC));
				debug("removed from free list");
				update_header(new_block, combined_size, 0, 1);
				add_footer(new_block);
				insert_to_free_list(new_block);
			}
			// sf_show_heap();
			return block->body.payload;

		}


	}

	//shouldn't ever reach this point
    return NULL;
}
