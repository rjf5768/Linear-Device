/* Author:    
   Date:
    */
    
    
    
/***
 *      ______ .___  ___. .______     _______.  ______              ____    __   __  
 *     /      ||   \/   | |   _  \   /       | /      |            |___ \  /_ | /_ | 
 *    |  ,----'|  \  /  | |  |_)  | |   (----`|  ,----'              __) |  | |  | | 
 *    |  |     |  |\/|  | |   ___/   \   \    |  |                  |__ <   | |  | | 
 *    |  `----.|  |  |  | |  |   .----)   |   |  `----.             ___) |  | |  | | 
 *     \______||__|  |__| | _|   |_______/     \______|            |____/   |_|  |_| 
 *                                                                                   
 */


#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "cache.h"
#include "mdadm.h"
#include "util.h"
#include "jbod.h"
#include "net.h"

//using this helper can reduce code length in read function and write function
uint32_t locate_helper(jbod_cmd_t cmd, uint32_t block_id, uint32_t disk_id) {
    uint32_t op = 0;
    uint32_t temp_cmd = (cmd) << 14;//locate at 14 bits
    uint32_t temp_disk_id = (disk_id) << 28;//locate at 28 bits
    uint32_t temp_block_id = (block_id) << 20;//locate at 20 bits
    op = temp_cmd | temp_disk_id | temp_block_id;
    return op;
}
int is_mounted = 0;

int mdadm_mount(void) {
    
    if (is_mounted == 1) {//already mounted, cant mount again
        return -1;
    }else{
        is_mounted = jbod_client_operation(locate_helper(JBOD_MOUNT,0,0), NULL);
        if (is_mounted == 1){
            return -1;//fail
        }else{
            is_mounted = 1;
            return 1;//success
        }
    }  
}

int mdadm_unmount(void) {
    if (is_mounted == 0) {//not mounted already cant unmount
        return -1;
    }else{
        is_mounted = jbod_client_operation(locate_helper(JBOD_UNMOUNT,0,0),NULL);
        if (is_mounted == 0){
            return 1;//success
        }else{
            return -1;//fail
        }
    }
}




int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
    if (is_mounted == 0) {
        // System is not mounted, cannot read
        return -1;
    }
    // If length is 0, no need to read
    if (len == 0) {
        return 0; // Return success
    }
    // Check for out of bounds and invalid parameters
    if (addr + len >(int)1048576 || len > 1024 || buf == NULL) {
        return -1;
    }


    uint8_t temp_buf [256];
    uint8_t *ptr;
    ptr = &temp_buf[addr % JBOD_DISK_SIZE % JBOD_NUM_BLOCKS_PER_DISK];
        //prevent null temp_buf
        if (temp_buf == NULL) {
            return -1;
        }
    
    //calculating current address, block id , disk id and also the end address by add length and starting add together
    uint32_t end_addr = addr + len - 1 ;
    uint32_t current_add = addr;
    uint32_t current_disk_id = current_add / (JBOD_NUM_BLOCKS_PER_DISK * JBOD_BLOCK_SIZE);
    uint32_t current_block_id = (current_add % (JBOD_NUM_BLOCKS_PER_DISK * JBOD_BLOCK_SIZE)) / JBOD_BLOCK_SIZE;
    
    //seek and read op, using helper function to acheive
    uint32_t seekDisk = locate_helper(JBOD_SEEK_TO_DISK,current_block_id,current_disk_id);
    uint32_t seekBlock = locate_helper(JBOD_SEEK_TO_BLOCK,current_block_id,current_disk_id);
    uint32_t readBlock = locate_helper(JBOD_READ_BLOCK,current_block_id,current_disk_id);
    jbod_client_operation(seekDisk,NULL);
    jbod_client_operation(seekBlock,NULL);

    //trying to read, use cache to check all conditions
    if (cache_enabled()){
        if(cache_lookup(current_disk_id, current_block_id, temp_buf) == -1){
            seekDisk = locate_helper(JBOD_SEEK_TO_DISK,current_block_id,current_disk_id);
            seekBlock = locate_helper(JBOD_SEEK_TO_BLOCK,current_block_id,current_disk_id);
            readBlock = locate_helper(JBOD_READ_BLOCK,current_block_id,current_disk_id);
            jbod_client_operation(seekDisk,NULL);
            jbod_client_operation(seekBlock,NULL);
            jbod_client_operation(readBlock,temp_buf);
            cache_insert(current_disk_id, current_block_id, temp_buf);
        }
    }
    else{
        seekDisk = locate_helper(JBOD_SEEK_TO_DISK,current_block_id,current_disk_id);
        seekBlock = locate_helper(JBOD_SEEK_TO_BLOCK,current_block_id,current_disk_id);
        readBlock = locate_helper(JBOD_READ_BLOCK,current_block_id,current_disk_id);
        jbod_client_operation(seekDisk,NULL);
        jbod_client_operation(seekBlock,NULL);
        jbod_client_operation(readBlock,temp_buf);
        
    }
    


    //iterate through every bytes from the starting address to the end address
    while(current_add <= end_addr){
        *buf = *ptr;
        buf += 1;
        ptr += 1;
        //over a disk, starting from block 0
        if ((current_add % JBOD_DISK_SIZE) == 65535){
            current_disk_id++;
            current_block_id = 0;
            seekDisk = locate_helper(JBOD_SEEK_TO_DISK,current_block_id,current_disk_id);
            seekBlock = locate_helper(JBOD_SEEK_TO_BLOCK,current_block_id,current_disk_id);
            readBlock = locate_helper(JBOD_READ_BLOCK,current_block_id,current_disk_id);
            jbod_client_operation(seekDisk,NULL);
            jbod_client_operation(seekBlock,NULL);
            //trying to read, use cache to check all conditions
            if (cache_enabled()){
                if(cache_lookup(current_disk_id, current_block_id, temp_buf) == -1){
                    seekDisk = locate_helper(JBOD_SEEK_TO_DISK,current_block_id,current_disk_id);
                    seekBlock = locate_helper(JBOD_SEEK_TO_BLOCK,current_block_id,current_disk_id);
                    readBlock = locate_helper(JBOD_READ_BLOCK,current_block_id,current_disk_id);
                    jbod_client_operation(seekDisk,NULL);
                    jbod_client_operation(seekBlock,NULL);
                    jbod_client_operation(readBlock,temp_buf);
                    cache_insert(current_disk_id, current_block_id, temp_buf);
                }
            }
            else{
                seekDisk = locate_helper(JBOD_SEEK_TO_DISK,current_block_id,current_disk_id);
                seekBlock = locate_helper(JBOD_SEEK_TO_BLOCK,current_block_id,current_disk_id);
                readBlock = locate_helper(JBOD_READ_BLOCK,current_block_id,current_disk_id);
                jbod_client_operation(seekDisk,NULL);
                jbod_client_operation(seekBlock,NULL);
                jbod_client_operation(readBlock,temp_buf);
                
            }
            ptr = temp_buf;
            
        }else if((current_add % JBOD_BLOCK_SIZE)==255){//over a block, go to the next block
            current_block_id += 1;
            readBlock = locate_helper(JBOD_READ_BLOCK,current_block_id,current_disk_id);
            //trying to read, use cache to check all conditions
            if (cache_enabled()){
                if(cache_lookup(current_disk_id, current_block_id, temp_buf) == -1){
                    seekDisk = locate_helper(JBOD_SEEK_TO_DISK,current_block_id,current_disk_id);
                    seekBlock = locate_helper(JBOD_SEEK_TO_BLOCK,current_block_id,current_disk_id);
                    readBlock = locate_helper(JBOD_READ_BLOCK,current_block_id,current_disk_id);
                    jbod_client_operation(seekDisk,NULL);
                    jbod_client_operation(seekBlock,NULL);
                    jbod_client_operation(readBlock,temp_buf);
                    cache_insert(current_disk_id, current_block_id, temp_buf);
                }
            }
            else{
                seekDisk = locate_helper(JBOD_SEEK_TO_DISK,current_block_id,current_disk_id);
                seekBlock = locate_helper(JBOD_SEEK_TO_BLOCK,current_block_id,current_disk_id);
                readBlock = locate_helper(JBOD_READ_BLOCK,current_block_id,current_disk_id);
                jbod_client_operation(seekDisk,NULL);
                jbod_client_operation(seekBlock,NULL);
                jbod_client_operation(readBlock,temp_buf);
                
            }
            ptr = temp_buf;
        }
        //address move on, keep iterating
        current_add += 1 ;
    }
    return len;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
    if (is_mounted == 0) {
        // System is not mounted, cannot read
        return -1;
    }
    // If length is 0, no need to read
    if (len == 0) {
        return 0; // Return success
    }
    // Check for out of bounds and invalid parameters
    if (addr + len >(int)1048576 || len > 1024 || buf == NULL) {
        return -1;
    }

    //initializing variables for later use
    int current_add = addr;
    int block_count = 0; 
    int tmp_len = len;
    int copy_of_len = len;
    int bytes_written = 0;

    //seek to the right place, also initialize variables for later use
    uint32_t current_disk_id = current_add / (JBOD_NUM_BLOCKS_PER_DISK * JBOD_BLOCK_SIZE);
    uint32_t current_block_id = (current_add % (JBOD_NUM_BLOCKS_PER_DISK * JBOD_BLOCK_SIZE)) / JBOD_BLOCK_SIZE;
    uint32_t seekDisk = locate_helper(JBOD_SEEK_TO_DISK,current_block_id,current_disk_id);
    uint32_t seekBlock = locate_helper(JBOD_SEEK_TO_BLOCK,current_block_id,current_disk_id);
    uint32_t readBlock = locate_helper(JBOD_READ_BLOCK,current_block_id,current_disk_id);
    jbod_client_operation(seekDisk,NULL);
    jbod_client_operation(seekBlock,NULL);

    while (current_add < addr + len){

        //initialize again in the while loop
        uint32_t current_disk_id = current_add / (JBOD_NUM_BLOCKS_PER_DISK * JBOD_BLOCK_SIZE);
        uint32_t current_block_id = (current_add % (JBOD_NUM_BLOCKS_PER_DISK * JBOD_BLOCK_SIZE)) / JBOD_BLOCK_SIZE;
        uint32_t current_offset = (current_add % JBOD_DISK_SIZE) % JBOD_BLOCK_SIZE;
        seekDisk = locate_helper(JBOD_SEEK_TO_DISK,current_block_id,current_disk_id);
        seekBlock = locate_helper(JBOD_SEEK_TO_BLOCK,current_block_id,current_disk_id);
        jbod_client_operation(seekDisk,NULL);
        jbod_client_operation(seekBlock,NULL);

        uint8_t tmp[JBOD_BLOCK_SIZE];
        

        // for first block
        if (block_count == 0){
            if (len + current_offset <= JBOD_BLOCK_SIZE){ 
                // read block to tmp buf
                //trying to read, use cache to check all conditions
                if (cache_enabled()){
                    if(cache_lookup(current_disk_id, current_block_id, tmp) == -1){
                        seekDisk = locate_helper(JBOD_SEEK_TO_DISK,current_block_id,current_disk_id);
                        seekBlock = locate_helper(JBOD_SEEK_TO_BLOCK,current_block_id,current_disk_id);
                        readBlock = locate_helper(JBOD_READ_BLOCK,current_block_id,current_disk_id);
                        jbod_client_operation(seekDisk,NULL);
                        jbod_client_operation(seekBlock,NULL);
                        jbod_client_operation(readBlock,tmp);
                        cache_insert(current_disk_id, current_block_id, tmp);
                    }
                }
                else{
                    seekDisk = locate_helper(JBOD_SEEK_TO_DISK,current_block_id,current_disk_id);
                    seekBlock = locate_helper(JBOD_SEEK_TO_BLOCK,current_block_id,current_disk_id);
                    readBlock = locate_helper(JBOD_READ_BLOCK,current_block_id,current_disk_id);
                    jbod_client_operation(seekDisk,NULL);
                    jbod_client_operation(seekBlock,NULL);
                    jbod_client_operation(readBlock,tmp);
                    
                }
                seekDisk = locate_helper(JBOD_SEEK_TO_DISK,current_block_id,current_disk_id);
                seekBlock = locate_helper(JBOD_SEEK_TO_BLOCK,current_block_id,current_disk_id);
                jbod_client_operation(seekDisk,NULL);
                jbod_client_operation(seekBlock,NULL);
                memcpy(tmp + current_offset, buf, len);
                jbod_client_operation(locate_helper(JBOD_WRITE_BLOCK, 0, 0), tmp);
                //we want to check if cache is enabled, if so update it after write block
                if (cache_enabled() == true){
			        cache_update(current_disk_id, current_block_id, tmp);
		        }

                //update variables
                bytes_written += len;
                copy_of_len -= len;
                current_add += len;

            }else{
                //trying to read, use cache to check all conditions
                if (cache_enabled()){
                    if(cache_lookup(current_disk_id, current_block_id, tmp) == -1){
                        seekDisk = locate_helper(JBOD_SEEK_TO_DISK,current_block_id,current_disk_id);
                        seekBlock = locate_helper(JBOD_SEEK_TO_BLOCK,current_block_id,current_disk_id);
                        readBlock = locate_helper(JBOD_READ_BLOCK,current_block_id,current_disk_id);
                        jbod_client_operation(seekDisk,NULL);
                        jbod_client_operation(seekBlock,NULL);
                        jbod_client_operation(readBlock,tmp);
                        cache_insert(current_disk_id, current_block_id, tmp);
                    }
                }
                else{
                    seekDisk = locate_helper(JBOD_SEEK_TO_DISK,current_block_id,current_disk_id);
                    seekBlock = locate_helper(JBOD_SEEK_TO_BLOCK,current_block_id,current_disk_id);
                    readBlock = locate_helper(JBOD_READ_BLOCK,current_block_id,current_disk_id);
                    jbod_client_operation(seekDisk,NULL);
                    jbod_client_operation(seekBlock,NULL);
                    jbod_client_operation(readBlock,tmp);
                    
                }
                seekDisk = locate_helper(JBOD_SEEK_TO_DISK,current_block_id,current_disk_id);
                seekBlock = locate_helper(JBOD_SEEK_TO_BLOCK,current_block_id,current_disk_id);
                jbod_client_operation(seekDisk,NULL);
                jbod_client_operation(seekBlock,NULL);  
                tmp_len = JBOD_BLOCK_SIZE - current_offset;             
                memcpy(tmp + current_offset, buf, tmp_len);
                jbod_client_operation(locate_helper(JBOD_WRITE_BLOCK, 0, 0), tmp);
                //we want to check if cache is enabled, if so update it after write block
                if (cache_enabled() == true){
			        cache_update(current_disk_id, current_block_id, tmp);
		        }
                //update variables
                bytes_written += tmp_len;
                copy_of_len -= tmp_len;
                current_add += tmp_len;
            }
        block_count = 1; 
        }else if (copy_of_len >= 256){// for full blocks
            memcpy(tmp, buf + bytes_written, JBOD_BLOCK_SIZE);
            jbod_client_operation(locate_helper(JBOD_WRITE_BLOCK, 0, 0), tmp);
            //we want to check if cache is enabled, if so update it after write block
            if (cache_enabled() == true){
			        cache_update(current_disk_id, current_block_id, tmp);
		        }
            //update variables
            copy_of_len -= 256;
            tmp_len = 256;
            bytes_written += 256;
            current_add += 256;
        }else{// for last block
            //trying to read, use cache to check all conditions
            if (cache_enabled()){
                if(cache_lookup(current_disk_id, current_block_id, tmp) == -1){
                    seekDisk = locate_helper(JBOD_SEEK_TO_DISK,current_block_id,current_disk_id);
                    seekBlock = locate_helper(JBOD_SEEK_TO_BLOCK,current_block_id,current_disk_id);
                    readBlock = locate_helper(JBOD_READ_BLOCK,current_block_id,current_disk_id);
                    jbod_client_operation(seekDisk,NULL);
                    jbod_client_operation(seekBlock,NULL);
                    jbod_client_operation(readBlock,tmp);
                    cache_insert(current_disk_id, current_block_id, tmp);
                }
            }
            else{
                seekDisk = locate_helper(JBOD_SEEK_TO_DISK,current_block_id,current_disk_id);
                seekBlock = locate_helper(JBOD_SEEK_TO_BLOCK,current_block_id,current_disk_id);
                readBlock = locate_helper(JBOD_READ_BLOCK,current_block_id,current_disk_id);
                jbod_client_operation(seekDisk,NULL);
                jbod_client_operation(seekBlock,NULL);
               jbod_client_operation(readBlock,tmp);

                
            }
            seekDisk = locate_helper(JBOD_SEEK_TO_DISK,current_block_id,current_disk_id);
            seekBlock = locate_helper(JBOD_SEEK_TO_BLOCK,current_block_id,current_disk_id);
            jbod_client_operation(seekDisk,NULL);
            jbod_client_operation(seekBlock,NULL);  
            memcpy(tmp + current_offset, buf + bytes_written, copy_of_len);
            jbod_client_operation(locate_helper(JBOD_WRITE_BLOCK, 0, 0), tmp);
            //we want to check if cache is enabled, if so update it after write block
            if (cache_enabled() == true){
			        cache_update(current_disk_id, current_block_id, tmp);
		        }
            //update variables
            current_add += copy_of_len;
        }
    }
    return len;
}
