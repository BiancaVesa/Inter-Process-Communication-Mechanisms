#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>

#define PIPE1 "RESP_PIPE_25475"
#define PIPE2 "REQ_PIPE_25475"

int main(){
    int fd1, fd2;
    char* connect_req = (char*)malloc(8 * sizeof(char));
    char* ping_resp = (char*)malloc(5 * sizeof(char));
    char* success_resp = (char*)malloc(8 * sizeof(char));
    char* error_resp = (char*)malloc(5 * sizeof(char));
    
    unsigned char* mem_reg = NULL;
    unsigned int mem_size;
    unsigned char* file_data = NULL;
    off_t file_size;

    unsigned int var = 25475;
    unsigned char *c = (unsigned char*)(&var);
    int shmFd;

    connect_req[0] = 0x07;
    connect_req[1] = 0x43;
    connect_req[2] = 0x4f;
    connect_req[3] = 0x4e;
    connect_req[4] = 0x4e;
    connect_req[5] = 0x45;
    connect_req[6] = 0x43;
    connect_req[7] = 0x54;

    ping_resp[0] = 0x04;
    ping_resp[1] = 0x50;
    ping_resp[2] = 0x4f;
    ping_resp[3] = 0x4e;
    ping_resp[4] = 0x47;

    success_resp[0] = 0x07;
    success_resp[1] = 0x53;
    success_resp[2] = 0x55;
    success_resp[3] = 0x43;
    success_resp[4] = 0x43;
    success_resp[5] = 0x45;
    success_resp[6] = 0x53;
    success_resp[7] = 0x53;

    error_resp[0] = 0x05;
    error_resp[1] = 0x45;
    error_resp[2] = 0x52;
    error_resp[3] = 0x52;
    error_resp[4] = 0x4f;
    error_resp[5] = 0x52;
            


    unsigned char req_size;
    char* req = NULL;

    if (mkfifo(PIPE1, 0600) != 0){
        printf("ERROR\ncannot create the response pipe\n");
        return 1;
    }

    fd2 = open(PIPE2, O_RDONLY);

    if (fd2 == -1){
        free(connect_req);
        free(ping_resp);
        unlink(PIPE2);
        close(fd2);
        printf("ERROR\ncannot open the request pipe\n");
        return 3;
    }

    fd1 = open(PIPE1, O_WRONLY);

    if (fd1 == -1){
        free(connect_req);
        free(ping_resp);
        unlink(PIPE1);
        unlink(PIPE2);
        close(fd1);
        close(fd2);
        printf("Could not open PIPE for writing\n");
        return 2;
    }

    if(write(fd1, connect_req, (*connect_req) + 1) != -1){
        printf("SUCCES\n");
    }
    else{
        printf("Could not write in pipe\n");
        free(connect_req);
        free(ping_resp);
        close(fd1);
        close(fd2);
        unlink(PIPE1);
        unlink(PIPE2);
        return 4;
    }

    for(;;)
    {
        if(read(fd2, &req_size, sizeof(unsigned char)) == 1){
            req = (char*)malloc(req_size * sizeof(char));
            
            read(fd2, req, req_size);

            if(strncmp(req, "EXIT", 4) == 0){
                break;
            }

            if(strncmp(req, "PING", 4) == 0){
                write(fd1, &req_size, 1);
                write(fd1, req, req_size);
                write(fd1, ping_resp, (*ping_resp) + 1);
                write(fd1, c, sizeof(unsigned int));
            }

            if (strncmp(req, "CREATE_SHM", 10) == 0){
                
                read(fd2, &mem_size, sizeof(unsigned int));
                shmFd = shm_open("/8cJYZa", O_CREAT | O_RDWR, 0644);
                int t = ftruncate(shmFd, mem_size);
                
                if(t == -1 || shmFd < 0) {
                    write(fd1, &req_size, 1);
                    write(fd1, req, req_size);
                    write(fd1, error_resp, (*error_resp) + 1);

                    free(connect_req);
                    free(ping_resp);
                    close(fd1);
                    close(fd2);
                    unlink(PIPE1);
                    unlink(PIPE2);
                    return 1;
                }

                mem_reg = (unsigned char*)mmap(NULL, mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
             

                if (mem_reg != MAP_FAILED) {
                    write(fd1, &req_size, 1);
                    write(fd1, req, req_size);
                    write(fd1, success_resp, (*success_resp) + 1);
                }
                else{
                    write(fd1, &req_size, 1);
                    write(fd1, req, req_size);
                    write(fd1, error_resp, (*error_resp) + 1);
                }
            }

            if(strncmp(req, "WRITE_TO_SHM", 12) == 0){
                unsigned int offset, value;
                read(fd2, &offset, sizeof(unsigned int));
                read(fd2, &value, sizeof(unsigned int));
                
                //assigning the new value in the shared memory on the offset location
                *(unsigned int*)(mem_reg + offset) = value;
                
                if(*(unsigned int*)(mem_reg + offset) == value && offset <= mem_size - 4){
                    write(fd1, &req_size, 1);
                    write(fd1, req, req_size);
                    write(fd1, success_resp, (*success_resp) + 1);
                }
                else{
                    write(fd1, &req_size, 1);
                    write(fd1, req, req_size);
                    write(fd1, error_resp, (*error_resp) + 1);
                }
            }

            if(strncmp(req, "MAP_FILE", 8) == 0){
                unsigned char file_name_size;
                read(fd2, &file_name_size, sizeof(unsigned char));
                char* file_name = (char*)malloc(file_name_size * sizeof(char));
                int s = read(fd2, file_name, *(unsigned int *)&file_name_size);
                file_name[s] = '\0';

                int file = open(file_name, O_RDONLY);

                if(file == -1){
                    write(fd1, &req_size, 1);
                    write(fd1, req, req_size);
                    write(fd1, error_resp, (*error_resp) + 1);
                    return 1;
                }

                file_size = lseek(file, 0, SEEK_END);
                lseek(file, 0, SEEK_SET);

                file_data = (unsigned char*)mmap(NULL, file_size, PROT_READ , MAP_SHARED, file, 0);

                unsigned char magic, nr_sections;
                short int version;
                unsigned int sect_type;

                //check the SF format
                magic = file_data[file_size - 1];
                short int header_size, types = 1;
                memcpy(&header_size, file_data + (file_size - 3), 2); 
                memcpy(&version, file_data + (file_size - header_size), 2);
                nr_sections = file_data[file_size - header_size + 2];
                for(unsigned int section = 1; section <= (header_size - 6) / 32; section++){
                    long sect_type_address = file_size - header_size + 3 + (section - 1) * 32 + 20; 
                    memcpy(&sect_type, file_data + sect_type_address, 4); 
                    
                    if (sect_type != 51 && sect_type != 53 && sect_type != 29 && sect_type != 96 && sect_type != 11 && sect_type != 22)
                    {
                        types = 0;
                    }
                }

                if (file_data != MAP_FAILED && magic == 'A' && version >= 54 && version <= 86 && nr_sections >= 5 && nr_sections <= 15 && types == 1){
                    write(fd1, &req_size, 1);
                    write(fd1, req, req_size);
                    write(fd1, success_resp, (*success_resp) + 1);
                }
                else{
                    write(fd1, &req_size, 1);
                    write(fd1, req, req_size);
                    write(fd1, error_resp, (*error_resp) + 1);
                }
            }

            if(strncmp(req, "READ_FROM_FILE_OFFSET", 22) == 0){
                unsigned int nr_bytes, offset;
                read(fd2, &offset, sizeof(unsigned int));
                read(fd2, &nr_bytes, sizeof(unsigned int));

                shmFd = shm_open("/8cJYZa", O_RDWR, 0644);
                if(offset + nr_bytes > file_size || shmFd < 0 || file_data == NULL)
                {
                    write(fd1, &req_size, 1);
                    write(fd1, req, req_size);
                    write(fd1, error_resp, (*error_resp) + 1);
                }
                else{
                    //copy the bytes from the mapped file intro a string
                    char* read_data = (char*)malloc((nr_bytes + 1) * sizeof(char));
                    for(int i = 0; i < nr_bytes; i++){
                        read_data[i] = file_data[offset + i];
                    }
                    read_data[nr_bytes] = '\0';
                    //copy the bytes at the begining of the shared memory
                    strncpy((char*)mem_reg, read_data, nr_bytes);
                    free(read_data);
                    write(fd1, &req_size, 1);
                    write(fd1, req, req_size);
                    write(fd1, success_resp, (*success_resp) + 1);
                }

            }

            if(strncmp(req, "READ_FROM_FILE_SECTION", 22) == 0){
                unsigned int nr_bytes, offset, section;
                read(fd2, &section, sizeof(unsigned int));
                read(fd2, &offset, sizeof(unsigned int));
                read(fd2, &nr_bytes, sizeof(unsigned int));

                shmFd = shm_open("/8cJYZa", O_RDWR, 0644);

                if(offset + nr_bytes > file_size || shmFd < 0 || file_data == NULL)
                {
                    write(fd1, &req_size, 1);
                    write(fd1, req, req_size);
                    write(fd1, error_resp, (*error_resp) + 1);
                }
                else{
                    short int header_size;
                    memcpy(&header_size, file_data + (file_size - 3), 2); //get section size from mapped file
                    unsigned char nr_sections = file_data[file_size - header_size + 2];
                    unsigned long sect_off_address = file_size - header_size + 3 + (section - 1) * 32 + 24; // the address of the bytes that store the section offset

                    if (section > *(unsigned int*)&nr_sections || section < 1 || sect_off_address > file_size){
                        write(fd1, &req_size, 1);
                        write(fd1, req, req_size);
                        write(fd1, error_resp, (*error_resp) + 1);
                    }
                    else {
                        unsigned int sect_offset, sect_size;
                        memcpy(&sect_offset, file_data + sect_off_address, 4); //get section offset from mapped file
                        memcpy(&sect_size, file_data + sect_off_address + 4, 4); //get section size from mapped file

                        //if there are not enough bytes in the section to read from offset, send error
                        if(offset + nr_bytes > sect_size){
                            write(fd1, &req_size, 1);
                            write(fd1, req, req_size);
                            write(fd1, error_resp, (*error_resp) + 1);
                            return 1;
                        }
                        
                        //copy the data from the mapped file into an auxiliary string
                        char* read_data = (char*)malloc((nr_bytes + 1) * sizeof(char));
                        for(int i = 0; i < nr_bytes; i++){
                            read_data[i] = file_data[sect_offset + offset + i];
                        }
                        read_data[nr_bytes] = '\0';

                        strncpy((char*)mem_reg, read_data, nr_bytes);
                        free(read_data);
                        write(fd1, &req_size, 1);
                        write(fd1, req, req_size);
                        write(fd1, success_resp, (*success_resp) + 1);
                    }
                }
            }
            

            if(strncmp(req, "READ_FROM_LOGICAL_SPACE_OFFSET", 30) == 0){
                unsigned int log_offset, nr_bytes;
                read(fd2, &log_offset, sizeof(unsigned int));
                read(fd2, &nr_bytes, sizeof(unsigned int));


                shmFd = shm_open("/8cJYZa", O_RDWR, 0644);

                if(shmFd < 0 || file_data == NULL)
                {
                    write(fd1, &req_size, 1);
                    write(fd1, req, req_size);
                    write(fd1, error_resp, (*error_resp) + 1);
                }
                else{
                    short int header_size;
                    memcpy(&header_size, file_data + (file_size - 3), 2); //get header size from mapped file
                    int sect_offset, sect_size, aux_sect_size, start_offset = 0, end_offset = 0, mul = 0, found = 0;
                    unsigned long sect_off_address;

                    //taking each section from 1 to nr_of_sections
                    for(unsigned int section = 1; section <= (header_size - 6) / 32; section++){
                        sect_off_address = file_size - header_size + 3 + (section - 1) * 32 + 24; //the address of the bytes that store the section offset
                        
                        memcpy(&sect_offset, file_data + sect_off_address, 4); //get section offset from mapped file
                        memcpy(&sect_size, file_data + sect_off_address + 4, 4); //get section size from mapped file
                        aux_sect_size = sect_size;
                        
                        if(sect_off_address > file_size){
                            write(fd1, &req_size, 1);
                            write(fd1, req, req_size);
                            write(fd1, error_resp, (*error_resp) + 1);
                            return 1;
                        }

                        //calculating the corresponding offset
                        start_offset = end_offset;

                        while(sect_size - 5120 > 0){
                            mul ++;
                            sect_size -= 5120;
                        }
                
                        mul++;
                        end_offset = 5120 * mul;

                                           
                        if(end_offset > log_offset){

                            //if there are not enough bytes in the section to read from offset, send error
                            if(log_offset - start_offset + nr_bytes > aux_sect_size)
                            break;

                            char* read_data = (char*)malloc((nr_bytes + 1) * sizeof(char));
                            for(int i = 0; i < nr_bytes; i++){
                                read_data[i] = file_data[sect_offset + (log_offset - start_offset) + i];
                            }
                            read_data[nr_bytes] = '\0';
                            
                            strncpy((char*)mem_reg , read_data, nr_bytes);

                            free(read_data);

                            write(fd1, &req_size, 1);
                            write(fd1, req, req_size);
                            write(fd1, success_resp, (*success_resp) + 1);
                            found = 1;
                            break;
                        }
                    }

                    if (found == 0){
                        write(fd1, &req_size, 1);
                        write(fd1, req, req_size);
                        write(fd1, error_resp, (*error_resp) + 1);
                    }
                }
            }

        }
        else{
            printf("Could not read from pipe\n");
            free(connect_req);
            free(ping_resp);
            free(success_resp);
            free(error_resp);
            close(fd1);
            close(fd2);
            unlink(PIPE1);
            unlink(PIPE2);
            return 4;
        }
    }
    shm_unlink("/8cJYZa");
    munmap(mem_reg, mem_size);
    munmap(file_data, file_size);
    free(connect_req);
    free(ping_resp);
    free(req);
    free(success_resp);
    free(error_resp);
    close(fd1);
    close(fd2);
    unlink(PIPE1);
    unlink(PIPE2);

    return 0;
}