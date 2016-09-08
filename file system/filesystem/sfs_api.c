#include "sfs_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bitmap.h"
#include <libgen.h>
#include "disk_emu.h"

int seen = 0;

#define OS_DISK "sfs_disk.disk"
#define BLOCK_SZ 1024
#define NUM_BLOCKS 512 
#define NUM_INODES 10  

// verify that this is right...
#define NUM_INODE_BLOCKS (sizeof(inode_t) * NUM_INODES / BLOCK_SZ + 1)
#define NUM_OF_NORMAL_BLOCKS (NUM_BLOCKS -1 - NUM_INODE_BLOCKS)

superblock_t sb;
inode_t table[NUM_INODES];
root_dir root[NUM_INODES];
#define NUM_OF_ROOT_BLOCKS (sizeof(root_dir)*NUM_INODES/BLOCK_SZ +1)
uint8_t free_bit_map[BLOCK_SZ/8];
file_descriptor fdt[NUM_INODES];
int position;
int nextfile = 0;
void init_superblock() {
    sb.magic = 0xACBD0005;
    sb.block_size = BLOCK_SZ;
    sb.fs_size = NUM_BLOCKS * BLOCK_SZ;
    sb.inode_table_len = NUM_INODE_BLOCKS;
    sb.root_dir_inode = 0;
}
//four init functions
void init_inode_table(){
    int i;
    int j;
    for(i = 0;i<NUM_INODES;i++){
        for(j=0;j<12;j++){
            table[i].data_ptrs[j] = 0;
            table[i].used = 0;
            table[i].indirectptr = 0;
        }
    }
}
void init_free_bit_map(){
    int i;
    for(i=0;i<BLOCK_SZ/8;i++){
        free_bit_map[i] = 0xff;
    }
}
void init_root_directory(){
    int i;
    for(i=0;i < NUM_INODES;i++){
        root[i].filename ="\0";
        //printf("%d root directory initalized!\n",i );
        root[i].inoteindex = NUM_INODES+1;
    }
}
void init_file_descriptor(){
    int i;
    for(i=0;i<NUM_INODES;i++){
        fdt[i].inode = NUM_INODES+1;
        fdt[i].rwptr = 0;
        fdt[i].wptr = 0;
        fdt[i].open = 0;
    }
}

//update freebitmap to disk
void updatefreebitmap(){
    write_blocks(NUM_INODE_BLOCKS+1,1,free_bit_map);
}
//update inode table to disk
void updatetable(){
    write_blocks(1, sb.inode_table_len, table);
}
//update root directory to disk
void updateroot(){
    write_blocks(NUM_INODE_BLOCKS+2,NUM_OF_ROOT_BLOCKS,root);
}
void mksfs(int fresh) {
    int i;
	//Implement mksfs here
    if (fresh) {	
        printf("making new file system\n");
        // create super block
        init_superblock();
        // create inode table
        init_inode_table();
        // create free bit map
        init_free_bit_map();
        // create the disk
        init_fresh_disk(OS_DISK, BLOCK_SZ, NUM_BLOCKS);
        
        /* write super block
         * write to first block, and only take up one block of space
         * pass in sb as a pointer
         */
        write_blocks(0, 1, &sb);

        // write inode table

        write_blocks(1, sb.inode_table_len, table);
        //write free bit map to disk
        write_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
        //write root directory to disk
        write_blocks(NUM_INODE_BLOCKS+2,NUM_OF_ROOT_BLOCKS,root);
        //set these 4 kind of blocks to be used
        for(i =0;i<(NUM_INODE_BLOCKS+2+NUM_OF_ROOT_BLOCKS);i++){
            force_set_index(i);
        }

        updatefreebitmap();
        //initial root directory and file descriptor
        init_root_directory();
        init_file_descriptor();
        write_blocks(NUM_INODE_BLOCKS+2,NUM_OF_ROOT_BLOCKS,root);
        int i;
        //
        for(i=0;i<NUM_OF_ROOT_BLOCKS;i++){
            table[0].data_ptrs[i] = NUM_INODE_BLOCKS+2+i;
            table[0].used =1;
        }
        updatetable();
        position = -1;
    } else {
        position = -1;
        printf("reopening file system\n");
        // open super block
        init_disk(OS_DISK,BLOCK_SZ,NUM_BLOCKS);
        read_blocks(0, 1, &sb);
        printf("Block Size is: %lu\n", sb.block_size);
        // open inode table,freebitmap and rootdirectory.
        read_blocks(1, sb.inode_table_len, table);
        read_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
        read_blocks(NUM_INODE_BLOCKS+2,NUM_OF_ROOT_BLOCKS,root);
        // open free block list
    }
	return;
}

int sfs_getnextfilename(char *fname) {

	//Implement sfs_getnextfilename here
    int i; 
    //compare char array to find which element in root directory has the same filename.
    for (i = nextfile; i < NUM_INODES; i++){
        if (strcmp(root[i].filename, "\0") != 0){
            //copy the filename to fname with max file length.
            strncpy(fname, root[i].filename, MAXFILENAME); 
            nextfile ++;   
            return 1; 
        }
    }
    return 0;
}

int sfs_getfilesize(const char* path) {
    int i;
    int inodeindex;
    //take the pathname pointed to by path 
    //and return a pointer to the final component of the pathname,
    char* name = basename((char*)path);
    //get the size of file by checking root directory's each element's file name
    for(i=0;i<NUM_INODES;i++){
        if(strcmp(root[i].filename,name)==0){
            inodeindex = root[i].inoteindex;
            return table[inodeindex].size;
            break;
        }
    }
	return -1;
}

int sfs_fopen(char *name) {
    int i;
    int created = 0;
    //if the name exceed the max length then do nothing but return -1.
    if(strlen(name)>MAXFILENAME){
        return -1;
    }
    //check which element in root directory has the same name.
    for(i=0;i<NUM_INODES;i++){
        if(strncmp(name,root[i].filename,MAXFILENAME)==0){
            created = 1;
            int j;
            for(j=0;j<NUM_INODES;j++){
                if(fdt[j].inode ==root[i].inoteindex){
                    //if it has been open then return the index.
                    if(fdt[j].open ==1){
                        return j;
                    }
                    else{
                        //if it is not then open it initializethe pointers and return the index.
                        fdt[j].open = 1;
                        fdt[j].rwptr = 0;
                        fdt[j].wptr = table[fdt[j].inode].size;
                        return j;
                    }
                }
            }
        }
    }
    //if cannot the file then create this file.
    if(created==0){
        for(i=0;i<NUM_INODES;i++){
            //find the first availiable slot in root directory.
            if(strlen(root[i].filename) ==0 ){
                int j;
                for(j=0;j<NUM_INODES;j++){
                    //find the first unused slot in inote table.
                    if(table[j].used ==0){
                        //initialize 
                        table[j].used =1;
                        table[j].size = 0;
                        root[i].inoteindex = j;
                        root[i].filename = name;
                        updateroot();
                        break;
                    }
                }
                int k;
                for(k=0;k<NUM_INODES;k++){
                    //find the first unused slot in fdt.
                    if(fdt[k].inode == NUM_INODES+1){
                        //open it and define the inodeindex.
                        fdt[k].inode = j;
                        fdt[k].rwptr = 0;
                        fdt[k].open = 1;
                        fdt[k].wptr = 0;
                        return k;
                    }
                }

            }
        }
    }
    return -1;
}



int sfs_fclose(int fileID){

	//Implement sfs_fclose here
    //if the file in fdt is open then close it.
    if(fdt[fileID].open ==1){	
        fdt[fileID].open =0;
        return 0;
    }
    else{
        //if it has been closed then do nothing
        printf("this file has been closed before!\n");
	    return -1;
    }
}

int sfs_fread(int fileID, char *buf, int length){
	//Implement sfs_fread here
    //if this file is not open then dont read but just return 0
    if(fdt[fileID].open == 0){
        return 0;
    }
    else{
    //get inodeindex,readpointer and the file size.
    int inodeindex = fdt[fileID].inode;
    int pointer = fdt[fileID].rwptr;
    int size = table[inodeindex].size;
    //cannot read the data which hasnt been written.
    if(length+pointer>size){
        length = size - pointer;
    }

    int num_bytes_to_be_read = 0;
    //number of block to read
    int numblkrd;
    if(1){
        //the current block of read pointer
        int currentblk = pointer/BLOCK_SZ +1;
        //the last block that will be read.
        int endblk = (pointer+length)/BLOCK_SZ +1;

        //start portion before the read pointer
        int offset = pointer%BLOCK_SZ;
        //calculate how many blocks will be read.
        if((offset + length) < BLOCK_SZ){
            numblkrd = 1;
        }
        else{
            numblkrd = 2+(length-(BLOCK_SZ-offset))/BLOCK_SZ;
        }
        //create a temporary buffer to store the data we want to read.
        char read_temp[BLOCK_SZ*2];
        //if the last block is less than 13 the we neednt to get indirect pointer
        if(endblk <=12){
            if(endblk > currentblk){
                //reset the temp buffer
                memset(read_temp,0,BLOCK_SZ*2);
                read_blocks(table[inodeindex].data_ptrs[currentblk-1],1,read_temp);
                //copy the data after the start portion
                memcpy((char*)(buf+num_bytes_to_be_read),read_temp+offset,BLOCK_SZ-offset);
                //update the number of bytes to be read and the read pointer.
                num_bytes_to_be_read +=BLOCK_SZ-offset;
                fdt[fileID].rwptr +=BLOCK_SZ-offset;
                //continue the similiar procedures until the last block.
                int j;
                for(j=currentblk;j<endblk-1;j++){
                    memset(read_temp,0,BLOCK_SZ*2);
                    read_blocks(table[inodeindex].data_ptrs[j],1,read_temp);
                    memcpy((char*)(buf+num_bytes_to_be_read),read_temp,BLOCK_SZ);
                    num_bytes_to_be_read +=BLOCK_SZ;
                    fdt[fileID].rwptr +=BLOCK_SZ;
                }
                memset(read_temp,0,BLOCK_SZ*2);
                read_blocks(table[inodeindex].data_ptrs[endblk-1],1,read_temp);
                //copy the data before end portion to buffer.
                memcpy((char*)(buf+num_bytes_to_be_read),read_temp,(length+pointer)%BLOCK_SZ);
                num_bytes_to_be_read +=(length+pointer)%BLOCK_SZ;
                fdt[fileID].rwptr +=(length+pointer)%BLOCK_SZ;
                //printf("1num read minus pointer change = %d\n", num_bytes_to_be_read+pointer-fdt[fileID].rwptr);
                return num_bytes_to_be_read;
            }
            else if(endblk == currentblk){
                //if the last block is the first block then we only need to get one block's data.
                memset(read_temp,0,BLOCK_SZ*2);
                read_blocks(table[inodeindex].data_ptrs[currentblk-1],1,read_temp);
                memcpy((char*)(buf),read_temp+offset,length);
                num_bytes_to_be_read +=length;
                fdt[fileID].rwptr +=length;
                //printf("2num read minus pointer change = %d\n", num_bytes_to_be_read+pointer-fdt[fileID].rwptr);
                return num_bytes_to_be_read;
            }
        }
        //if the pointer is on the block with index of 13 or larger then we only need to use indirect pointer.
        else if(currentblk>12){
            if(currentblk<endblk){
                memset(read_temp,0,BLOCK_SZ*2);
                //a buff to store indirect pointer array.
                unsigned int indirectptrarray[BLOCK_SZ/sizeof(unsigned int)];
                //get the indirect pointer array to this buff
                read_blocks(table[inodeindex].indirectptr,1,indirectptrarray);
                read_blocks(indirectptrarray[currentblk-13],1,read_temp);
                memcpy((char*)(buf+num_bytes_to_be_read),read_temp+offset,BLOCK_SZ-offset);
                num_bytes_to_be_read +=BLOCK_SZ -offset;
                fdt[fileID].rwptr += BLOCK_SZ-offset;
                int j;
                for(j=currentblk;j<endblk-1;j++){
                    memset(read_temp,0,BLOCK_SZ*2);
                    read_blocks(indirectptrarray[j-12],1,read_temp);
                    memcpy((char*)(buf+num_bytes_to_be_read),read_temp,BLOCK_SZ);
                    num_bytes_to_be_read +=BLOCK_SZ;
                    fdt[fileID].rwptr += BLOCK_SZ;
                }
                memset(read_temp,0,BLOCK_SZ*2);
                read_blocks(indirectptrarray[endblk-13],1,read_temp);
                memcpy((char*)(buf+num_bytes_to_be_read),read_temp,(offset+length)%BLOCK_SZ);
                num_bytes_to_be_read +=(offset+length)%BLOCK_SZ;
                fdt[fileID].rwptr +=(offset+length)%BLOCK_SZ;
                //printf("3num read minus pointer change = %d\n", num_bytes_to_be_read+pointer-fdt[fileID].rwptr);
                return num_bytes_to_be_read;
            }
            else if(currentblk == endblk){
                memset(read_temp,0,BLOCK_SZ*2);
                unsigned int indirectptrarray2[BLOCK_SZ/sizeof(unsigned int)];
                read_blocks(table[inodeindex].indirectptr,1,indirectptrarray2);
                read_blocks(indirectptrarray2[currentblk-13],1,read_temp);
                memcpy((char*)(buf+num_bytes_to_be_read),read_temp+offset,length);
                num_bytes_to_be_read +=length;
                fdt[fileID].rwptr += length;
                //printf("4num read minus pointer change = %d\n", num_bytes_to_be_read+pointer-fdt[fileID].rwptr);
                return num_bytes_to_be_read;
            }
        }
        //if the first block is less than 12 and last block is greater than 12 then we need to use indirect pointer and direct pointer both.
        else if(currentblk <=12 && endblk > 12){
            //read first block's data
            unsigned int indirectptrarray3[BLOCK_SZ/sizeof(unsigned int)];
            read_blocks(table[inodeindex].indirectptr,1,indirectptrarray3);
            memset(read_temp,0,BLOCK_SZ*2);
            read_blocks(table[inodeindex].data_ptrs[currentblk-1],1,read_temp);
            memcpy((char*)(buf+num_bytes_to_be_read),read_temp+offset,BLOCK_SZ-offset);
            num_bytes_to_be_read += BLOCK_SZ-offset;
            fdt[fileID].rwptr += BLOCK_SZ-offset;
            int j;
            //read the middle blocks data by direct pointer
            for(j=currentblk;j<12;j++){
                memset(read_temp,0,BLOCK_SZ*2);
                read_blocks(table[inodeindex].data_ptrs[j],1,read_temp);
                memcpy((char*)(buf+num_bytes_to_be_read),read_temp,BLOCK_SZ);
                num_bytes_to_be_read +=BLOCK_SZ;
                fdt[fileID].rwptr += BLOCK_SZ;
            }
            //read the middle blocks data by indirect pointer.
            for(j=12;j<endblk-1;j++){
                memset(read_temp,0,BLOCK_SZ*2);
                read_blocks(indirectptrarray3[j-12],1,read_temp);
                memcpy((char*)(buf+num_bytes_to_be_read),read_temp,BLOCK_SZ);
                num_bytes_to_be_read +=BLOCK_SZ;
                fdt[fileID].rwptr +=BLOCK_SZ;
            }
            //read the last block data before end portion,
            memset(read_temp,0,BLOCK_SZ*2);
            read_blocks(indirectptrarray3[endblk-13],1,read_temp);
            memcpy((char*)(buf+num_bytes_to_be_read),read_temp,(offset+length)%BLOCK_SZ);
            num_bytes_to_be_read += (offset+length)%BLOCK_SZ;
            fdt[fileID].rwptr +=(offset+length)%BLOCK_SZ;
            //printf("5num read minus pointer change = %d\n", num_bytes_to_be_read+pointer-fdt[fileID].rwptr);
            return num_bytes_to_be_read;
        }

        }
    }
}

int sfs_fwrite(int fileID, const char *buf, int length){
    //get the inodeindex from fdt
    int inodeindex = fdt[fileID].inode;
    //get the current write pointer's position
    int pointer = fdt[fileID].wptr;
    //reset the number bytes to 0
    int num_bytes_to_be_written = 0;
    int numblkwr;
    //start portion before write pointer at the same block
    int offset = pointer % BLOCK_SZ;
    //calculate how many blocks need to written.
    if((offset + length) < BLOCK_SZ){
        numblkwr = 1;
    }
    else{
        numblkwr = 2+(length-(BLOCK_SZ-offset))/BLOCK_SZ;
    }
    //if this file has not been writen before.
    if(table[inodeindex].data_ptrs[0] == 0){
        //if number of blocks to be written is less than 12 then we only need to find free block for direct pointer.
        if(numblkwr <=12){
            char write_temp[BLOCK_SZ*2];
            int j;
            for(j=0;j<numblkwr-1;j++){
                //reset the temp buffer.
                memset(write_temp,0,BLOCK_SZ*2);
                memcpy(write_temp,buf+j*BLOCK_SZ,BLOCK_SZ);
                table[inodeindex].data_ptrs[j] = get_index();
                updatetable();
                write_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
                int write = write_blocks(table[inodeindex].data_ptrs[j],1,write_temp);
                if(write >= 1){
                    num_bytes_to_be_written += BLOCK_SZ;
                    fdt[fileID].wptr +=BLOCK_SZ;
                }
            }
            memset(write_temp,0,BLOCK_SZ*2);
            //only copy the part of data which is before end portion.
            memcpy(write_temp,buf+(numblkwr-1)*BLOCK_SZ, length%BLOCK_SZ);
            table[inodeindex].data_ptrs[numblkwr-1] = get_index();
            updatetable();
            write_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
            int write = write_blocks(table[inodeindex].data_ptrs[numblkwr-1],1,write_temp);
            if(write>=1){
                num_bytes_to_be_written += (length%BLOCK_SZ);
                fdt[fileID].wptr +=(length%BLOCK_SZ);
            }
        
        table[inodeindex].size += num_bytes_to_be_written;
        updatetable();
        //update inodetable and freebitmap to disk.
        write_blocks(1, sb.inode_table_len, table);
        write_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
        //printf("1num write minus pointer change = %d\n", num_bytes_to_be_written+pointer-fdt[fileID].wptr);
        return num_bytes_to_be_written;
    }

    //if writing more than 12 blocks,then we need to use indirect pointer.
        else{
            int j;
            char write_temp[BLOCK_SZ];
            int indirectblknum = numblkwr-12;
            //use the same way to write data for first 12 blocks.
            for(j=0;j<12;j++){
                memset(write_temp,0,BLOCK_SZ*2);
                memcpy(write_temp,buf+j*BLOCK_SZ,BLOCK_SZ);
                table[inodeindex].data_ptrs[j] = get_index();
                updatetable();
                write_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
                int write = write_blocks(table[inodeindex].data_ptrs[j],1,write_temp);
                if(write>=1){
                    num_bytes_to_be_written += BLOCK_SZ;
                    fdt[fileID].wptr +=BLOCK_SZ;
                }
            }
            //get the free block to store the indirect pointer.
            table[inodeindex].indirectptr = get_index();
            updatetable();
            updatefreebitmap();
            write_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
            unsigned int indirectptrarray[BLOCK_SZ/sizeof(unsigned int)];
            //write the data to middle blocks
            for(j=0;j<indirectblknum-1;j++){
                indirectptrarray[j] = get_index();
                write_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
                memset(write_temp,0,BLOCK_SZ*2);
                memcpy(write_temp,buf+(12+j)*BLOCK_SZ,BLOCK_SZ);
                int write = write_blocks(indirectptrarray[j],1,write_temp);
                if(write>=1){
                    num_bytes_to_be_written +=BLOCK_SZ;
                    fdt[fileID].wptr +=BLOCK_SZ;
                }
            }
            indirectptrarray[indirectblknum-1] = get_index();
            write_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
            memset(write_temp,0,BLOCK_SZ*2);
            //write data to the last block,copy the part before end portion.
            memcpy(write_temp,buf+(numblkwr-1)*BLOCK_SZ,length%BLOCK_SZ);
            int write = write_blocks(indirectptrarray[indirectblknum-1],1,write_temp);
            if(write>=1){
                num_bytes_to_be_written += (length%BLOCK_SZ);
                fdt[fileID].wptr +=(length%BLOCK_SZ);
            }

            //update to disk.
            table[inodeindex].size += num_bytes_to_be_written;
            write_blocks(table[inodeindex].indirectptr,1,indirectptrarray);
            write_blocks(1, sb.inode_table_len, table);
            write_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
            //printf("2num write minus pointer change = %d\n", num_bytes_to_be_written+pointer-fdt[fileID].wptr);
            return num_bytes_to_be_written;
        }
    }
    else{
        
        int write; 
        int currentblk = pointer/BLOCK_SZ +1;
        int endblk = (pointer+length)/BLOCK_SZ +1;
        char write_temp[BLOCK_SZ];
        //if the last block is less than 13 then no indirect pointer will be used.
        if(endblk <=12){
            if(endblk>currentblk){
                //write data to first block
                memset(write_temp,0,BLOCK_SZ*2);
                read_blocks(table[inodeindex].data_ptrs[currentblk-1],1,write_temp);
                memcpy(write_temp+offset,buf,BLOCK_SZ-offset);
                write = write_blocks(table[inodeindex].data_ptrs[currentblk-1],1,write_temp);
                if(write >=1){
                    num_bytes_to_be_written +=(BLOCK_SZ - offset);
                    fdt[fileID].wptr +=(BLOCK_SZ-offset);
                }
                int j;
                //write data to middle blocks.
                for(j=currentblk;j<endblk-1;j++){
                    memset(write_temp,0,BLOCK_SZ*2);
                    memcpy(write_temp,buf+BLOCK_SZ-offset+(j-currentblk)*BLOCK_SZ,BLOCK_SZ);
                    if(table[inodeindex].data_ptrs[j] == 0){
                        table[inodeindex].data_ptrs[j] = get_index();
                        updatetable();
                        write_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
                    }
                    write = write_blocks(table[inodeindex].data_ptrs[j],1,write_temp);
                    if(write>=1){
                        num_bytes_to_be_written+=BLOCK_SZ;
                        fdt[fileID].wptr +=BLOCK_SZ;
                    }
                }
                //write data to the last block.
                memset(write_temp,0,BLOCK_SZ*2);
                memcpy(write_temp,buf+(endblk-currentblk)*BLOCK_SZ-offset,(offset+length)%BLOCK_SZ);
                if(table[inodeindex].data_ptrs[endblk-1] == 0){
                    table[inodeindex].data_ptrs[endblk-1] = get_index();
                    updatetable();
                    write_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
                }
                write = write_blocks(table[inodeindex].data_ptrs[endblk-1],1,write_temp);
                if(write>=1){
                    num_bytes_to_be_written += ((length+offset)%BLOCK_SZ);
                    fdt[fileID].wptr +=((length+offset)%BLOCK_SZ);
                }
                //update the size.
                if(length+pointer > table[inodeindex].size){
                    table[inodeindex].size = length+pointer;
                    updatetable();
                }
                //update the freebit map and inote table.
                write_blocks(1, sb.inode_table_len, table);
                write_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
                //printf("3num write minus pointer change = %d\n", num_bytes_to_be_written+pointer-fdt[fileID].wptr);
                return num_bytes_to_be_written;
            }
            //if the last one is the first one then we only need to write data between startportion and end portion.
            else if(currentblk == endblk){
                memset(write_temp,0,BLOCK_SZ*2);
                read_blocks(table[inodeindex].data_ptrs[currentblk-1],1,write_temp);
                memcpy(write_temp+offset,buf,length);
                write = write_blocks(table[inodeindex].data_ptrs[currentblk-1],1,write_temp);
                if(write >=1){
                    num_bytes_to_be_written +=length;
                    fdt[fileID].wptr +=length;
                }
                if(length+pointer > table[inodeindex].size){
                    table[inodeindex].size = length+pointer;
                    updatetable();
                }
                //printf("4num write minus pointer change = %d\n", num_bytes_to_be_written+pointer-fdt[fileID].wptr);
                return num_bytes_to_be_written;
            }
            //if the first block has the index greater then 13 then we only need to get the indirect array.
        }
        else if(currentblk > 12){
            if(currentblk < endblk){//write data on first block
                memset(write_temp,0,BLOCK_SZ*2);
                unsigned int indirectptrarray2[BLOCK_SZ/sizeof(unsigned int)];
                read_blocks(table[inodeindex].indirectptr,1,indirectptrarray2);
                read_blocks(indirectptrarray2[currentblk-13],1,write_temp);
                memcpy(write_temp+offset,buf,BLOCK_SZ-offset);
                write = write_blocks(indirectptrarray2[currentblk-13],1,write_temp);
                if(write >=1){
                    num_bytes_to_be_written +=(BLOCK_SZ-offset);
                    fdt[fileID].wptr += (BLOCK_SZ -offset);
                }
                int j;
                for(j=currentblk;j<endblk-1;j++){//write data on middle blocks
                    if(indirectptrarray2[j-12] == 0){
                        indirectptrarray2[j-12] =get_index();
                        write_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
                    }
                    memset(write_temp,0,BLOCK_SZ*2);
                    memcpy(write_temp,(buf+BLOCK_SZ-offset+(j-currentblk)*BLOCK_SZ),BLOCK_SZ);
                    write=write_blocks(indirectptrarray2[j-12],1,write_temp);
                    if(write >=1){
                        num_bytes_to_be_written += BLOCK_SZ;
                        fdt[fileID].wptr += BLOCK_SZ;
                    }
                }
                //write data on last block
                memset(write_temp,0,BLOCK_SZ*2);
                memcpy(write_temp,(buf+(endblk-currentblk)*BLOCK_SZ-offset),(offset+length)%BLOCK_SZ);
                if(indirectptrarray2[endblk-13] == 0){
                    indirectptrarray2[endblk-13] =get_index();
                    write_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
                }
                write = write_blocks(indirectptrarray2[endblk-13],1,write_temp);
                if(write >=1){
                    num_bytes_to_be_written +=(offset+length)%BLOCK_SZ;
                    fdt[fileID].wptr +=(offset+length)%BLOCK_SZ;
                }
                if(length+pointer > table[inodeindex].size){
                    table[inodeindex].size = length+pointer;
                    updatetable();
                }
                //update the freebit map and inote table.
                write_blocks(table[inodeindex].indirectptr,1,indirectptrarray2);
                write_blocks(1, sb.inode_table_len, table);
                write_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
                //printf("5num write minus pointer change = %d\n", num_bytes_to_be_written+pointer-fdt[fileID].wptr);
                return num_bytes_to_be_written;
            }
            else if(endblk == currentblk){//write data on current block
                memset(write_temp,0,BLOCK_SZ*2);
                unsigned int indirectptrarray2[BLOCK_SZ/sizeof(unsigned int)];
                read_blocks(table[inodeindex].indirectptr,1,indirectptrarray2);
                read_blocks(indirectptrarray2[currentblk-13],1,write_temp);
                memcpy(write_temp+offset,buf,length);
                write = write_blocks(indirectptrarray2[currentblk-13],1,write_temp);
                if(write >=1){
                    num_bytes_to_be_written +=length;
                    fdt[fileID].wptr += length;
                }
                if(length+pointer > table[inodeindex].size){
                    table[inodeindex].size = length+pointer;
                    updatetable();
                }
                //update the freebit map and inote table.
                write_blocks(table[inodeindex].indirectptr,1,indirectptrarray2);
                write_blocks(1, sb.inode_table_len, table);
                write_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
                //printf("6num write minus pointer change = %d\n", num_bytes_to_be_written+pointer-fdt[fileID].wptr);
                return num_bytes_to_be_written;
            }
        }
        else if(currentblk<=12 && endblk>12){
        //if the first block's index is less than 13 and last one's is above 12 
        //then both direct pointers and indirect pointers need to be used.
                unsigned int indirectptrarray3[BLOCK_SZ/sizeof(unsigned int)];
                memset(write_temp,0,BLOCK_SZ*2);
                read_blocks(table[inodeindex].data_ptrs[currentblk-1],1,write_temp);
                memcpy(write_temp+offset,buf,BLOCK_SZ-offset);
                //write data to first block
                write = write_blocks(table[inodeindex].data_ptrs[currentblk-1],1,write_temp);
                if(write >=1){
                    num_bytes_to_be_written +=(BLOCK_SZ - offset);
                    fdt[fileID].wptr +=(BLOCK_SZ-offset);
                }
                int j;
                //write data to middle blocks by direct pointer
                for(j=currentblk;j<12;j++){
                    memset(write_temp,0,BLOCK_SZ*2);
                    memcpy(write_temp,(buf+BLOCK_SZ-offset+(j-currentblk)*BLOCK_SZ),BLOCK_SZ);
                    if(table[inodeindex].data_ptrs[j] == 0){
                        table[inodeindex].data_ptrs[j] = get_index();
                        updatetable();
                        write_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
                    }
                    write = write_blocks(table[inodeindex].data_ptrs[j],1,write_temp);
                    if(write>=1){
                        num_bytes_to_be_written+=BLOCK_SZ;
                        fdt[fileID].wptr +=BLOCK_SZ;
                    }
                }
                //write data to middle blocks by indirect pointer
                for(j=12;j<endblk-1;j++){
                    if(table[inodeindex].indirectptr == 0){
                        table[inodeindex].indirectptr = get_index();
                        updatetable();
                        write_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
                        read_blocks(table[inodeindex].indirectptr,1,indirectptrarray3);
                    }

                    if(indirectptrarray3[j-12] == 0){
                        indirectptrarray3[j-12] =get_index();
                        write_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
                    }
                    memset(write_temp,0,BLOCK_SZ*2);
                    memcpy(write_temp,(buf+BLOCK_SZ-offset+(j-currentblk)*BLOCK_SZ),BLOCK_SZ);
                    write=write_blocks(indirectptrarray3[j-12],1,write_temp);
                    if(write >=1){
                        num_bytes_to_be_written += BLOCK_SZ;
                        fdt[fileID].wptr += BLOCK_SZ;
                    }
                }
                //write data to the last block.
                memset(write_temp,0,BLOCK_SZ*2);
                memcpy(write_temp,(buf+(endblk-currentblk)*BLOCK_SZ-offset),(offset+length)%BLOCK_SZ);
                if(indirectptrarray3[endblk-13] == 0){
                    indirectptrarray3[endblk-13] =get_index();
                    write_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
                }
                write = write_blocks(indirectptrarray3[endblk-13],1,write_temp);
                if(write >=1){
                    num_bytes_to_be_written +=((offset+length)%BLOCK_SZ);
                    fdt[fileID].wptr +=((offset+length)%BLOCK_SZ);
                }
                //update the file size
                if(length+pointer > table[inodeindex].size){
                    table[inodeindex].size = length+pointer;
                    updatetable();
                }
                //update the inode table and freebitmap.
                write_blocks(table[inodeindex].indirectptr,1,indirectptrarray3);
                write_blocks(1, sb.inode_table_len, table);
                write_blocks(1+NUM_INODE_BLOCKS,1,free_bit_map);
                //printf("7num write minus pointer change = %d\n", num_bytes_to_be_written+pointer-fdt[fileID].wptr);
                return num_bytes_to_be_written;
        }
    }


}

int sfs_fseek(int fileID, int loc){
    int index = fdt[fileID].inode;
    int size = table[index].size;
    if(size<loc){
        loc = size;
    }
    //set both pointers to location.
    fdt[fileID].wptr = loc;
    fdt[fileID].rwptr = loc;
	return 0;
}

int sfs_remove(char *file) {
    int i,j;
    j = NUM_INODES+1;
    for(i=0;i<NUM_INODES;i++){
        if(strcmp(file,root[i].filename)==0){
            j = i;
        }
    }
    if(j == NUM_INODES+1){
        return -1;
    }
    else{
        //delete infor in root directory
        root[j].filename = "\0";
        int remain;
        char emptyblock[BLOCK_SZ/sizeof(char)] = "\0";
        unsigned int indirectptrtemp2[10000] ;
        int index = root[j].inoteindex;
        //delete information in fdt
        for(i = 0;i<NUM_INODES;i++){
            if(fdt[i].inode == index){
                fdt[i].inode = 0;
                fdt[i].rwptr = 0;
                fdt[i].wptr = 0;
                fdt[i].open = 0;
            }
        }
        //delete infor in inode and delete all stored data in disk .
        for(i = 0; i < 12; i++){
            if(table[index].data_ptrs[i] != 0){
                write_blocks(table[index].data_ptrs[i],1,emptyblock);
                rm_index(table[index].data_ptrs[i]);
                updatefreebitmap();
                table[index].data_ptrs[i] = 0;
            }
        }
        if(table[index].indirectptr != NUM_BLOCKS +1){
            read_blocks(table[index].indirectptr,1,indirectptrtemp2);
            if(table[index].size %BLOCK_SZ ==0){
                remain = table[index].size/BLOCK_SZ;
            }
            else{
                remain = table[index].size/BLOCK_SZ +1;
            }
            for(i=0;i<remain;i++){
                write_blocks(indirectptrtemp2[i],1,emptyblock);
                rm_index(indirectptrtemp2[i]);
                updatefreebitmap();
            }
            write_blocks(table[index].indirectptr,1,emptyblock);
            rm_index(table[index].indirectptr);
            updatefreebitmap();
            table[index].indirectptr = NUM_BLOCKS +1;
        }

    }
	
	return 0;
}
