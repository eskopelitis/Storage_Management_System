#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache.h"
#include "jbod.h"
#include "mdadm.h"
#include "net.h"
int mount = 0;
int permission = 0;

uint32_t translate(int disk_number, int command, int block_number) {
    uint32_t op = 0;
    op |= (command << 26);
    op |= (disk_number << 22);
    op |= block_number;
    return op;
}

int mdadm_mount(void) {
    //returns an error if it is already mounted
    if (mount == 1) {
        return -1;
    }
    uint32_t op = translate(0, JBOD_MOUNT, 0);
    jbod_client_operation(op, NULL);
    //returns success for mounting
    mount = 1;
    return 1;
}

int mdadm_unmount(void) {
    //returns failure if it's already unmounted
    if (mount == 0) {
        return -1;
    }
    uint32_t op = translate(0, JBOD_UNMOUNT, 0);
    jbod_client_operation(op, NULL);
    //returns sucess if it was mounted
    mount = 0;
    return 1;
}

int mdadm_write_permission(void) {
    // returns failure if already can write
    if (permission == 1) {
        return -1;
    }
    uint32_t op = translate(0, JBOD_WRITE_PERMISSION, 0);
    jbod_client_operation(op, NULL);
    permission = 1;
    return 0;
}

int mdadm_revoke_write_permission(void) {
    // returns failure if already revoked
    if (permission == 0) {
        return -1;
    }
    uint32_t op = translate(0, JBOD_REVOKE_WRITE_PERMISSION, 0);
    jbod_client_operation(op, NULL);
    permission = 0;
    return 0;
}

int mdadm_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf) {
    uint32_t curr = start_addr;
    uint32_t last_addr = (start_addr + read_len);
    
    //if not mounted, can't do any operations
    //if read_buf == NULL, it can't perform function because there is no where to store data
    //if read_len == 0, it can't perform function because there is no data to read
    if (mount == 0 || read_len > 1024 || (read_buf == NULL && read_len != 0) || read_len < 0) {
        return -1;
    }
    
    // if addr and len > total amount of memory, then it would be out of bounds
    if ((start_addr + read_len) > (JBOD_DISK_SIZE * JBOD_NUM_DISKS)) {
        return -1;
    }
    
    int remainder = 0;
    
    while (curr < last_addr) {
        uint32_t block_id = (curr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
        uint32_t disk_id = (curr / JBOD_DISK_SIZE);
        uint8_t buf_temp[256]; // temp buffer
        

        if (cache_lookup(disk_id, block_id, buf_temp) != 1) {
            // read from disk if not in cache
            uint32_t disk_seek = translate(disk_id, JBOD_SEEK_TO_DISK, 0);
            uint32_t block_seek = translate(disk_id, JBOD_SEEK_TO_BLOCK, block_id);
            uint32_t read = translate(disk_id, JBOD_READ_BLOCK, block_id);
            
            // Seek and read operations
            if (jbod_client_operation(disk_seek, NULL) == -1 ||
                jbod_client_operation(block_seek, NULL) == -1 ||
                jbod_client_operation(read, buf_temp) == -1) {
            }
        }
        
        uint32_t copy_bytes = JBOD_BLOCK_SIZE - (curr % JBOD_BLOCK_SIZE);
        if (copy_bytes > (read_len - remainder)) {
            copy_bytes = read_len - remainder;
        }
        
        memcpy(read_buf + remainder, buf_temp + (curr % JBOD_BLOCK_SIZE), copy_bytes);

        curr += copy_bytes;
        remainder += copy_bytes;
    }
    
    return remainder;
}

int mdadm_write(uint32_t start_addr, uint32_t write_len, const uint8_t *write_buf) {
    // returns 0 if nothing to write
    if (write_len == 0) {
        return 0;
    }
    // returns failure if not able to write
    if (!permission) {
        return -1;
    }
    if (mount == 0 || write_len > 1024 || (write_buf == NULL && write_len != 0) || write_len < 0) {
        return -1;
    }
    // check bounds
    if ((start_addr + write_len) > (JBOD_DISK_SIZE * JBOD_NUM_DISKS)) {
        return -1;
    }
    
    uint32_t curr = start_addr;
    uint32_t last_addr = start_addr + write_len;
    int bytes_written = 0;
    
    while (curr < last_addr) {
        uint32_t block_id = (curr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
        uint32_t disk_id = (curr / JBOD_DISK_SIZE);
        uint32_t disk_seek = translate(disk_id, JBOD_SEEK_TO_DISK, block_id);
        uint32_t block_seek = translate(disk_id, JBOD_SEEK_TO_BLOCK, block_id);

        uint8_t buf_temp[JBOD_BLOCK_SIZE];
        uint8_t remainder_addr = (curr % JBOD_DISK_SIZE) % JBOD_BLOCK_SIZE;
        uint32_t copy_bytes = JBOD_BLOCK_SIZE - remainder_addr;
        
        if (cache_lookup(disk_id, block_id, buf_temp) != 1) {
            jbod_client_operation(disk_seek, NULL);
            jbod_client_operation(block_seek, NULL);
            jbod_client_operation(translate(disk_id, JBOD_READ_BLOCK, block_id), buf_temp);
        }
        
        if (copy_bytes > (write_len - bytes_written)) {
            copy_bytes = write_len - bytes_written;
        }
        
        // Update temp buf
        memcpy(buf_temp + remainder_addr, write_buf + bytes_written, copy_bytes);
        jbod_client_operation(disk_seek, NULL);
        jbod_client_operation(block_seek, NULL);
        jbod_client_operation(translate(disk_id, JBOD_WRITE_BLOCK, block_id), buf_temp);
        
        // if enabled, update cache
        if (cache_enabled()) {
            cache_update(disk_id, block_id, buf_temp);
        }
        
        curr += copy_bytes;
        bytes_written += copy_bytes;
    }
    
    return bytes_written;
}