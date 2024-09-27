#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>

typedef struct
{
  char name[100];
  char mode[8];
  char uid[8];
  char gid[8];
  char file_size[12];
  char last_modification[12];
  char checksum[8];
  unsigned char file_type;
  char name_linked_file[100];
  char magic[6];
  char version[2];
  char uname[32];
  char gname[32];
  char devmajor[8];
  char devminor[8];
  char prefix[155];
  char pad[12];
}tar_header_format;
  
FILE *validate_file_destination(char *file_name)
{
  FILE *buffer = fopen(file_name, "rb");
  
  if(buffer == NULL){
    printf("Error : file %s not found\n", file_name);
    exit(1);
  }

  return buffer;
}

int set_tar_name(char *file_name, char *header_field)
{
  if(strlen(file_name) > 99){
    printf("Error name too long");
    return 0;
  }
  else{
    strncpy(header_field, file_name, 100);
    return 1;
  }
}

struct stat get_file_status(char *file_name)
{
  struct stat file_status;
  if(stat(file_name, &file_status) < 0){
    printf("Fail to archive, try again with other file\n");
    exit(1);
  }

  return file_status;
}

int set_type_flag(unsigned char *type, mode_t mode) {
    if (S_ISREG(mode)) {
        *type = '0';  // Regular file
    } else if (S_ISLNK(mode)) {
        *type = '2';  // Symbolic link
    } else if (S_ISDIR(mode)) {
        *type = '5';  // Directory
    } else if (S_ISCHR(mode)) {
         *type = '3';  // Character device
    } else if (S_ISBLK(mode)) {
        *type = '4';  // Block device
    } else if (S_ISFIFO(mode)) {
        *type = '6';  // FIFO (named pipe)
    } else {
        *type = ' ';  // Unknown or unsupported type
	return 0;
    }
    return 1;
}

void set_username(char *uname, uid_t uid)
{
  struct passwd *user_data;
  user_data = getpwuid(uid);
  memset(uname, 0, 32 * sizeof(char));
  strcpy(uname, user_data->pw_name);
}

void set_groupname(char *gname, gid_t gid)
{
  struct group *grp;
  
  grp = getgrgid(gid);
  snprintf(gname, 32 * sizeof(char), "%s", grp->gr_name);
}

unsigned int calculate_check_sum(tar_header_format *header)
{
  unsigned char* header_data = (unsigned char*)header;
  unsigned int sum = 0;
  for(ssize_t i = 0;i < sizeof(tar_header_format);i++){
    sum += (unsigned char)header_data[i];
  }

  return sum;
}

void set_file_status(tar_header_format *tar_file, struct stat file_status)
{
  snprintf(tar_file->mode, sizeof(tar_file->mode), "%07o", file_status.st_mode & 07777);//the file permissions
  snprintf(tar_file->file_size, sizeof(tar_file->file_size), "%011o", (unsigned int)file_status.st_size);//the file size
  snprintf(tar_file->last_modification, sizeof(tar_file->last_modification), "%011o", (unsigned int)file_status.st_mtime);//last modification  time
  snprintf(tar_file->uid, sizeof(tar_file->uid), "%07o", (unsigned int)file_status.st_uid);//owner numeric id
  snprintf(tar_file->gid, sizeof(tar_file->gid), "%07o", (unsigned int)file_status.st_gid);//group numeric id
  memset(tar_file->checksum, ' ', sizeof(tar_file->checksum));//checksum initialization
  if(set_type_flag(&tar_file->file_type, file_status.st_mode) == 0){//file mode
    printf("Error no recognised file type\n");
  }
  memset(tar_file->name_linked_file, 0, sizeof(tar_file->name_linked_file));//name of the linked file, no permitted
  strncpy(tar_file->magic, "ustar", 5);//type of tar(using Ustar)
  tar_file->magic[5] = ' ';
  strcpy(tar_file->version, " ");
  set_username(tar_file->uname, file_status.st_uid);
  set_groupname(tar_file->gname, file_status.st_gid);
  memset(tar_file->devmajor, 0, sizeof(tar_file->devmajor));
  memset(tar_file->devminor, 0, sizeof(tar_file->devminor));
  memset(tar_file->prefix, 0, sizeof(tar_file->prefix));
  memset(tar_file->pad, 0, sizeof(tar_file->pad));
  snprintf(tar_file->checksum, sizeof(tar_file->checksum), "%06o", calculate_check_sum(tar_file));
  tar_file->checksum[7] = ' ';
  //printf("%s\n%s\n%s\n%s\n%s\n%c\n%s\n%s\n%s\n%s\n%s\n", tar_file->mode, tar_file->uid, tar_file->gid, tar_file->file_size, tar_file->last_modification, tar_file->file_type, tar_file->magic, tar_file->version, tar_file->uname, tar_file->gname, tar_file->name);
}

void write_header_to_file(tar_header_format header, FILE *tar_file)
{
  fwrite(&header, sizeof(header), sizeof(char), tar_file);
}

void write_data_to_file(FILE *tar_file, FILE *buffer, int end)
{
  char data[512];
  ssize_t elems_read = 0;
  while((elems_read = fread(data, sizeof(char), 512, buffer)) == 512){
    fwrite(data, sizeof(char), 512, tar_file);
  }
  if(elems_read != 0){
    fwrite(data, sizeof(char), elems_read, tar_file);
    memset(data, 0, 512 - elems_read);
    fwrite(data, sizeof(char), 512 - elems_read, tar_file);
  }
  if(end == 1){
    memset(data, 0, 512);
    fwrite(data, sizeof(char), 512, tar_file);
  
    fwrite(data, sizeof(char), 512, tar_file);
  }
}

int main(int argc, char **argv)
{
  int end = 0;
  if(argc == 1){
    printf("Arhivator tar, pentru folosire folositi comanda -a(arhivare) -d(dezarhivare)\nPentru arhivare folositi ./arhivator -a file_name1, fileNmae2, file_name3, etc... arhive_name");
  }
  else if(argc == 2){
    printf("Eroare");
  }
  else{
    if(strcmp(argv[1], "-a") != 0 && strcmp(argv[1], "-d") != 0){
      printf("Wrong command");
    }
    else{
      char comanda = argv[1][1];
      
      FILE *dst = fopen(argv[argc - 1], "wb");
      
      for(ssize_t i = 2;i < argc - 1;i++){
	end = (i == argc - 1);
	FILE *buffer = validate_file_destination(argv[i]);
	tar_header_format tar_file = {0};
	
	if(set_tar_name(argv[i], tar_file.name) == 1){
	  printf("%s\n", argv[i]);
	  set_file_status(&tar_file, get_file_status(argv[2]));
	  write_header_to_file(tar_file, dst);
	  write_data_to_file(dst, buffer, end);
	}
    
	fclose(buffer);
      }
      fclose(dst);
    }
  }
  
  return 0;
}
