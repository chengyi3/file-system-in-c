/*
 * file:        fs5600.c
 */

#define FUSE_USE_VERSION 27
#define _FILE_OFFSET_BITS 64

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fuse.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include "fs5600.h"

/* if you don't understand why you can't use these system calls here,
 * you need to read the assignment description another time
 */
#define stat(a,b) error do not use stat()
#define open(a,b) error do not use open()
#define read(a,b,c) error do not use read()
#define write(a,b,c) error do not use write()


// === block manipulation functions ===

/* disk access.
 * All access is in terms of 4KB blocks; read and
 * write functions return 0 (success) or -EIO.
 *
 * read/write "nblks" blocks of data
 *   starting from block id "lba"
 *   to/from memory "buf".
 *     (see implementations in misc.c)
 */
extern int block_read(void *buf, int lba, int nblks);
extern int block_write(void *buf, int lba, int nblks);
char*  bitmap;
int total_blocks_size;
int total_blocks;
int parent_dir_inum;
int parent_len;
/* bitmap functions
 */
void bit_set(unsigned char *map, int i)
{
    map[i/8] |= (1 << (i%8));
}
void bit_clear(unsigned char *map, int i)
{
    map[i/8] &= ~(1 << (i%8));
}
int bit_test(unsigned char *map, int i)
{
    return map[i/8] & (1 << (i%8));
}


/*
 * Allocate a free block from the disk.
 *
 * success - return free block number
 * no free block - return -ENOSPC
 */
int alloc_blk() {
    unsigned char * map = (unsigned char *) bitmap;
    int i;
    int num;
    int b = 0;
    for (i=0; i<total_blocks; i++) {
        if (bit_test(map, i) == 0) {
            bit_set(map, i);
            b = 1;
            num = i;
            break;
        }
    }
    block_write(bitmap, 1, 1);
    if (b == 0) {
        return -ENOSPC;
    }
    return num;
    /* Your code here */
}

/*
 * Return a block to disk, which can be used later.
 */
void free_blk(int i) {
    bit_clear((unsigned char *) bitmap, i);
    block_write(bitmap, 1, 1);
    /* your code here*/
}


// === FS helper functions ===


/* Two notes on path translation:
 *
 * (1) translation errors:
 *
 *   In addition to the method-specific errors listed below, almost
 *   every method can return one of the following errors if it fails to
 *   locate a file or directory corresponding to a specified path.
 *
 *   ENOENT - a component of the path doesn't exist.
 *   ENOTDIR - an intermediate component of the path (e.g. 'b' in
 *             /a/b/c) is not a directory
 *
 * (2) note on splitting the 'path' variable:
 *
 *   the value passed in by the FUSE framework is declared as 'const',
 *   which means you can't modify it. The standard mechanisms for
 *   splitting strings in C (strtok, strsep) modify the string in place,
 *   so you have to copy the string and then free the copy when you're
 *   done. One way of doing this:
 *
 *      char *_path = strdup(path);
 *      int inum = ... // translate _path to inode number
 *      free(_path);
 */


/*
 * convert path into inode number.
 *
 *
 *  - first split the path into directory and file names
 *  - then, start from the root inode (which inode/block number is that?)
 *  - then, walk the dirs to find the final file or dir.
 *    when walking:
 *      -- how do I know if an inode is a dir or file? (hint: mode)
 *      -- what should I do if something goes wrong? (hint: read the above note about errors)
 *      -- how many dir entries in one inode? (hint: read Lab4 instructions about directory inode)
 */

int path2inum(const char *path) {
    parent_len = 1;
    errno = 0;
    char *_path = strdup(path);
    char * names[11];
    char *t;
    int h = 0;
    t = strtok(_path, "/");
    while (t != NULL) {
//    printf("%s\n", t);
        names[h++] = t;
        t = strtok(NULL, "/");
    }
    char buf[FS_BLOCK_SIZE];
    block_read(&buf, 2, 1);
    inode_t * r = (inode_t *)buf;
    if (!names[0]) {
      //  printf("a");
            return 2;
    }
   // printf("%d\n",h);
   // int v;
     //for (v = 0; v < h; v++)
       // printf("%s\n", names[v]);
    int inum = 2;
    int j = 0;
    while (j < h) {
//      printf("%d\n", j);
        if (S_ISDIR(r->mode)) {
        parent_dir_inum = inum;
            int bid = r->ptrs[0];
            char b[FS_BLOCK_SIZE];
            block_read(&b, bid, 1);
            dirent_t * dir = (dirent_t *)b;
            int d;
            int exist = 0;
            for (d = 0; d < FS_BLOCK_SIZE/sizeof(dirent_t); d++) {
                if (dir[d].valid == 1 && !strcmp(dir[d].name, names[j])) {
                    exist = 1;
            parent_len += 1;
                    inum = dir[d].inode;
    //        printf("%d\n",inum);
                    char c[FS_BLOCK_SIZE];
                    block_read(&c, inum, 1);
                    r = (inode_t *)c;
                    j+=1;
                    break;
                }
            }
            if (exist == 0) {
        // printf("directory doesnot exist\n");
         errno = ENOENT;
         return errno;
            }
            
        }
        else if (S_ISREG(r->mode)) {
    //    printf("%s\n",names[j]);
            if (j+1 <= h) {
    //    printf("file appear in directory");
        errno = ENOTDIR;
        return errno;
            }
       // printf("c");
            break;
        }
    }
    free(_path);
    return inum;
    /* your code here */
}


/*
 * Helper function:
 *   copy the information in an inode to struct stat
 *   (see its definition below, and the "full definition" in 'man lstat'.)
 *
 *  struct stat {
 *        ino_t     st_ino;         // Inode number
 *        mode_t    st_mode;        // File type and mode
 *        nlink_t   st_nlink;       // Number of hard links
 *        uid_t     st_uid;         // User ID of owner
 *        gid_t     st_gid;         // Group ID of owner
 *        off_t     st_size;        // Total size, in bytes
 *        blkcnt_t  st_blocks;      // Number of blocks allocated
 *                                  // (note: block size is FS_BLOCK_SIZE;
 *                                  // and this number is an int which should be round up)
 *
 *        struct timespec st_atim;  // Time of last access
 *        struct timespec st_mtim;  // Time of last modification
 *        struct timespec st_ctim;  // Time of last status change
 *    };
 *
 */

void inode2stat(struct stat *sb, struct fs_inode *in, uint32_t inode_num)
{
    memset(sb, 0, sizeof(*sb));
    sb->st_mode = in->mode;
    sb->st_ino = inode_num;
    sb->st_nlink = 1;
    sb->st_uid = in->uid;
    sb->st_gid = in->gid;
    sb->st_size = in->size;
    sb->st_blocks = (FS_BLOCK_SIZE + in->size - 1) / FS_BLOCK_SIZE;
    sb->st_mtim.tv_sec = in->mtime;
    sb->st_atim = sb->st_mtim;
    sb->st_ctim.tv_sec = in->ctime;

    /* your code here */
}




// ====== FUSE APIs ========

/*
 * init - this is called once by the FUSE framework at startup.
 *
 *
 *   - read superblock
 *   - check if the magic number matches FS_MAGIC
 *   - initialize whatever in-memory data structure your fs5600 needs
 *     (you may come back later when requiring new data structures)
 */
void* fs_init(struct fuse_conn_info *conn)
{
    char buf[FS_BLOCK_SIZE];
    int ret = block_read(&buf, 0, 1);
    if (ret < 0) {
        exit(1);
    }
    super_t * super = (super_t *)buf;
    if (super->magic != FS_MAGIC) {
        exit(1);
    }
    total_blocks = super->disk_size;
    bitmap = malloc(FS_BLOCK_SIZE);
    block_read(bitmap, 1, 1);
    total_blocks_size = super->disk_size * FS_BLOCK_SIZE;
    
    return NULL;
    /* your code here */
}


/*
 * statfs - get file system statistics
 * see 'man 2 statfs' for description of 'struct statvfs'.
 * Errors - none. Needs to work.
 */
int fs_statfs(const char *path, struct statvfs *st)
{
    /* needs to return the following fields (ignore others):
     *   [DONE] f_bsize = FS_BLOCK_SIZE
     *   [DONE] f_namemax = <whatever your max namelength is>
     *   [TODO] f_blocks = total image - (superblock + block map)
     *   [TODO] f_bfree = f_blocks - blocks used
     *   [TODO] f_bavail = f_bfree
     *
     * it's okay to calculate this dynamically on the rare occasions
     * when this function is called.
     */
   int free = 0;
   int i;
   for (i =0; i < total_blocks; i++){
       if (bit_test((unsigned char *)bitmap, i) == 0) {
               free += 1;
       }
   }
    st->f_bsize = FS_BLOCK_SIZE;
    st->f_namemax = 27;  // why? see fs5600.h
    st->f_blocks = total_blocks - 2;
    st->f_bfree = free;
    st->f_bavail = st->f_bfree;
    return 0;
    /* your code here */
}


/*
 * getattr - get file or directory attributes. For a description of
 *  the fields in 'struct stat', read 'man 2 stat'.
 *
 *
 *  1. parse the path given by "const char * path",
 *     find the inode of the specified file,
 *       [note: you should implement the helfer function "path2inum"
 *       and use it.]
 *  2. copy inode's information to "struct stat",
 *       [note: you should implement the helper function "inode2stat"
 *       and use it.]
 *  3. and return:
 *     ** success - return 0
 *     ** errors - path translation, ENOENT
 */


int fs_getattr(const char *path, struct stat *sb)
{
    errno = 0;
    int a = path2inum(path);
    if (errno == ENOTDIR || errno == ENOENT) {
        return -errno;
    }
  //  printf("%s[%d]\n", path,errno);
   // printf("[%d]\n",a);
    char buf[FS_BLOCK_SIZE];
    block_read(&buf, a, 1);
    inode_t * n = (inode_t *)buf;
   // printf("%d%d\n", n->uid,n->gid);
    inode2stat(sb,n,a);
    return 0;
    /* your code here */
}

/*
 * readdir - get directory contents.
 *
 * call the 'filler' function for *each valid entry* in the
 * directory, as follows:
 *     filler(ptr, <name>, <statbuf>, 0)
 * where
 *   ** "ptr" is the second argument
 *   ** <name> is the name of the file/dir (the name in the direntry)
 *   ** <statbuf> is a pointer to the struct stat (of the file/dir)
 *
 * success - return 0
 * errors - path resolution, ENOTDIR, ENOENT
 *
 */
int fs_readdir(const char *path, void *ptr, fuse_fill_dir_t filler,
               off_t offset, struct fuse_file_info *fi)
{
    //dir's inode
    errno = 0;
    int a = path2inum(path);
    if (errno == ENOTDIR || errno == ENOENT) {
        return -errno;
    }
   // printf("{%d}\n",a);
    char b[FS_BLOCK_SIZE];
    block_read(&b, a, 1);
    //get the dir's inode
    inode_t * n = (inode_t *)b;
    if (S_ISREG(n->mode)){
        return -ENOTDIR;
    }
    //get the directory array
    int bid = n->ptrs[0];
    char c[FS_BLOCK_SIZE];
    block_read(&c, bid, 1);
    dirent_t * dir = (dirent_t *)c;
    //walk through dir entry, find valid entry call filler
    int d;
    for (d = 0; d < FS_BLOCK_SIZE/sizeof(dirent_t); d++) {
        if (dir[d].valid == 1) {
            int inum = dir[d].inode;
            char c[FS_BLOCK_SIZE];
            block_read(&c, inum, 1);
            inode_t * r = (inode_t *)c;
        struct stat sb;
        memset(&sb, 0, sizeof(stat));
            //copy inode information to sb
            inode2stat(&sb, r, inum);
            filler(ptr, dir[d].name, &sb, 0);
        }
    }
    return 0;
    /* your code here */
}


/*
 * read - read data from an open file.
 * success: should return exactly the number of bytes requested, except:
 *   - if offset >= file len, return 0
 *   - if offset+len > file len, return #bytes from offset to end
 *   - on error, return <0
 * Errors - path resolution, ENOENT, EISDIR
 */
int fs_read(const char *path, char *buf, size_t len, off_t offset,
        struct fuse_file_info *fi)
{
  // printf("read offset: %d\n",offset);
   // errno = 0;
   // char * c = buf;
    int num = len;
    int a = path2inum(path);
    if (errno == ENOTDIR || errno == ENOENT) {
        return -errno;
    }
    char b[FS_BLOCK_SIZE];
    block_read(&b, a, 1);
        //get the file's inode
    inode_t * n = (inode_t *)b;
    if (!S_ISREG(n->mode)){
            return -EISDIR;
    }
    if (offset >= n->size) {
//     buf = c;
        return 0;
    }
    if (offset + len > n->size) {
//    printf("z");
        num = n->size - offset;
        len = n->size - offset;
//    printf("size is%d\n",n->size);
    }
    //start read from data_block
    int data_block = offset / FS_BLOCK_SIZE;
    //start byte in the data block
    int start = offset-FS_BLOCK_SIZE * data_block;
    
    while (len > 0 && n->ptrs[data_block] != 0) {
        char *buffer = malloc(FS_BLOCK_SIZE);
        block_read(buffer, n->ptrs[data_block], 1);
        if (len >= FS_BLOCK_SIZE-start) {
            memcpy(buf, buffer+start, FS_BLOCK_SIZE-start);
            buf += FS_BLOCK_SIZE-start;
       // num += FS_BLOCK_SIZE-start;
            len -= FS_BLOCK_SIZE-start;
          //  data_block += 1;
            //start = 0;
        }
        else {
            memcpy(buf, buffer+start, len);
       // num += len;
            buf += len;
            len -= len;
        }
    data_block += 1;
        start = 0;
        
    }
  // printf("%d\n",len);
  //  buf = c;
  //  printf("%d\n",len);
    /* your code here */
    return num;
}



/*
 * rename - rename a file or directory
 * success - return 0
 * Errors - path resolution, ENOENT, EINVAL, EEXIST
 *
 * ENOENT - source does not exist
 * EEXIST - destination already exists
 * EINVAL - source and destination are not in the same directory
 */
int fs_rename(const char *src_path, const char *dst_path)
{
    errno = 0;
    //src inode
    path2inum(src_path);
    int src_parent = parent_dir_inum;
    if (errno == ENOTDIR || errno == ENOENT) {
        return -ENOENT;
    }
    path2inum(dst_path);
    int dst_parent = parent_dir_inum;
    if (src_parent != dst_parent) {
        return -EINVAL;
    }
    //if no error,des already exist
    if (errno == 0) {
        return -EEXIST;
    }
    char *s_path = strdup(src_path);
    char *d_path = strdup(dst_path);
    // get src parent, get des file name(strtok)
    char * s_names[11];
    char *t;
    int h = 0;
    t = strtok(s_path, "/");
    while (t != NULL) {
            s_names[h++] = t;
            t = strtok(NULL, "/");
    }
    char * t_names[11];
    char *m;
    int p = 0;
    m = strtok(d_path, "/");
    while (m != NULL) {
            t_names[p++] = m;
            m = strtok(NULL, "/");
    }

    int dir_inode = parent_dir_inum;
  //  printf("[%d]\n",dir_inode);
    char buf[FS_BLOCK_SIZE];
    block_read(&buf, dir_inode, 1);
    inode_t * r = (inode_t *)buf;
    //dir array
    int bid = r->ptrs[0];
    char b[FS_BLOCK_SIZE];
    block_read(&b, bid, 1);
    dirent_t * dir = (dirent_t *)b;
    int d;
    for (d = 0; d < FS_BLOCK_SIZE/sizeof(dirent_t); d++) {
        if (!strcmp(dir[d].name, s_names[h-1])) {
            strcpy(dir[d].name, t_names[p-1]);
//        printf("%s\n", dir[d].name);
            break;
        }
    }
    block_write(&b, bid, 1);

    /* your code here */
    return 0;
}

/*
 * chmod - change file permissions
 *
 * success - return 0
 * Errors - path resolution, ENOENT.
 *
 */
int fs_chmod(const char *path, mode_t mode)
{
    errno = 0;
    int a = path2inum(path);
    if (errno == ENOTDIR || errno == ENOENT) {
        return -ENOENT;
    }
    char b[FS_BLOCK_SIZE];
    block_read(&b, a, 1);
    //get the file's inode
    inode_t * n = (inode_t *)b;
    n->mode = (n->mode & S_IFMT)|mode;
    block_write(&b, a, 1);
    /* your code here */
    return 0;
}


/*
 * create - create a new file with specified permissions
 *
 * success - return 0
 * errors - path resolution, EEXIST
 *          in particular, for create("/a/b/c") to succeed,
 *          "/a/b" must exist, and "/a/b/c" must not.
 *
 * If a file or directory of this name already exists, return -EEXIST.
 * If there are already 128 entries in the directory (i.e. it's filled an
 * entire block), you are free to return -ENOSPC instead of expanding it.
 * If the name is too long (longer than 27 letters), return -EINVAL.
 */
int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    uint32_t cur_time = time(NULL);
    struct fuse_context *ctx = fuse_get_context();
    uint16_t uid = ctx->uid;
    uint16_t gid = ctx->gid;

    // get rid of compiling warnings; you should remove later
    (void) uid, (void) gid, (void) cur_time;

    path2inum(path);
    int parent = parent_dir_inum;
    if (errno == 0) {
        return -EEXIST;
    }
    if (errno == ENOTDIR){
        return -ENOTDIR;
    }
    //char * _path2 = strdup(path);
    char *_path = strdup(path);
    char * s_names[11];
    char *t;
    int h = 0;
    t = strtok(_path, "/");
    while (t != NULL) {
            s_names[h++] = t;
            t = strtok(NULL, "/");
    }

    int len = 0;
    if (len + strlen(s_names[h-1]) > 27) {
            return -EINVAL;
    }
    if(parent_len < h){
      //    printf("{%d}\n",parent_len);
          return -ENOENT;
     }
  /*  if (sizeof(s_names) > 27) {
        return -EINVAL;
    }*/

    char buf[FS_BLOCK_SIZE];
    block_read(&buf, parent, 1);
    inode_t * r = (inode_t *)buf;
    //check if parent dir exist
    if (bit_test((unsigned char*)bitmap, parent) == 0) {
        return -ENOENT;
    }
   /* dirent_t * y = (dirent_t *)r;
    if (!S_ISDIR(r->mode)){
                return -ENOTDIR;
        }
    if (h>=2){
       if (strcmp(y->name, s_names[h-2]) != 0){
               return -ENOENT;
       }
    }*/
    /*if (h >= 2) {
        if (!S_ISDIR(r->mode)){
                return -ENOTDIR;
        }
        //if it's a directory, but name is not equal to parent path name
        dirent_t * d = (dirent_t*)r;
        //should I check if d->valid == 0?
        if (d->valid == 0 || strcmp(d->name, s_names[h-2]) != 0) {
            return -ENOENT;
        }
    }*/
    //dir array
    int bid = r->ptrs[0];
    char b[FS_BLOCK_SIZE];
    block_read(&b, bid, 1);
    dirent_t * dir = (dirent_t *)b;
    int full = 1;
    int d;
//    int create_block;
   // int exist = 0;
    for (d = 0; d < FS_BLOCK_SIZE/sizeof(dirent_t); d++) {
        //if the directory entry is not used, meaning not full
        if (dir[d].valid == 0) {
            full = 0;
            int create_block = alloc_blk();
            dir[d].valid = 1;
            dir[d].inode = create_block;
            strcpy(dir[d].name, s_names[h-1]);
            char * buffer = malloc(FS_BLOCK_SIZE);
            inode_t *n = (inode_t *)buffer;
            n->uid = uid;
            n->gid = gid;
            n->mode = S_IFREG|mode;
            n->mtime = cur_time;
            n->size = 0;
            n->ctime = cur_time;
            int i;
            for (i = 0; i < NUM_PTRS_INODE; i++) {
                n->ptrs[i] = 0;
            }
            block_write(buffer, create_block, 1);
        break;
        }
    }
    if (full) {
        return -ENOSPC;
    }
    block_write(&b, bid, 1);
     block_write(&buf, parent, 1);
    return 0;
}



/*
 * mkdir - create a directory with the given mode.
 *
 * Note that 'mode' only has the permission bits. You
 * have to OR it with S_IFDIR before setting the inode 'mode' field.
 *
 * success - return 0
 * Errors - path resolution, EEXIST
 * Conditions for EEXIST are the same as for create.
 */
int fs_mkdir(const char *path, mode_t mode)
{
    uint32_t cur_time = time(NULL);
    struct fuse_context *ctx = fuse_get_context();
    uint16_t uid = ctx->uid;
    uint16_t gid = ctx->gid;

    // get rid of compiling warnings; you should remove later
    (void) uid, (void) gid, (void) cur_time;

    //my code start here
    path2inum(path);
    int parent = parent_dir_inum;
   // printf("%d\n",parent);
    //directory already exist
    if (errno == 0) {
//    printf("{%s}\n",path);
        return -EEXIST;
    }


    if (errno == ENOTDIR){
        return -ENOTDIR;
    }

    //to get the new directory name, which is at index h-1,repeat several times in the assignment
    char *_path = strdup(path);
    char * s_names[11];
    char *t;
    int h = 0;
    t = strtok(_path, "/");
    while (t != NULL) {
            s_names[h++] = t;
            t = strtok(NULL, "/");
    }
    int len = 0;
    if (len + strlen(s_names[h-1]) > 27) {
        return -EINVAL;
    }

   // printf("[%d]\n",h);
    //if directory name is longer than 27
   /* if (sizeof(s_names) > 27) {
        return -EINVAL;
    }*/
    //read parent directory where we need to create the new directory
    char buf[FS_BLOCK_SIZE];
    block_read(&buf, parent, 1);
    inode_t * r = (inode_t *)buf;
    //check if parent dir exist
    if (bit_test((unsigned char*)bitmap, parent) == 0) {
        return -ENOENT;
    }
     if (!S_ISDIR(r->mode)){
                return -ENOTDIR;
        }
     if(parent_len < h){
//      printf("{%d}\n",parent_len);
          return -ENOENT;
     }
 /* if (h >= 2){
    int len;
    int q;
    for (q = 0; q < h-1; q++) {
        len += strlen("/");
        len += strlen(s_names[q]);
    }
    char * parent_path = malloc(len + 1);
   // char * sep = "/";
    strcpy(parent_path,"/");
    for (q = 0; q < h-1; q++) {
        strcat(parent_path, s_names[q]);
    if(q!= h-2){
        strcat(parent_path,"/");
    }
    }
    printf("{%s}\n", parent_path);
    printf("%s\n",path);
    char * par[11];
    int hi = 0;
    t = strtok(_path, "/");
    while (t != NULL) {
            par[hi++] = t;
            t = strtok(NULL, "/");
    }
    if (strcmp(par[hi-1], s_names[h-1]) != 0){
        return -ENOENT;
    }
    */
    //dir array
    int bid = r->ptrs[0];
    char b[FS_BLOCK_SIZE];
    block_read(&b, bid, 1);
    dirent_t * dir = (dirent_t *)b;
    int full = 1;
    int d;
    //int create_block;
   // int exist = 0;
    for (d = 0; d < FS_BLOCK_SIZE/sizeof(dirent_t); d++) {
        //if the directory entry is not used, meaning not full
        if (dir[d].valid == 0) {
            full = 0;
            int create_block = alloc_blk();
            dir[d].valid = 1;
            dir[d].inode = create_block;
            strcpy(dir[d].name, s_names[h-1]);
            char * buffer = malloc(FS_BLOCK_SIZE);
            inode_t *n = (inode_t *)buffer;
            n->uid = uid;
            n->gid = gid;
            n->mode = S_IFDIR|mode;
            n->mtime = cur_time;
            n->size = 0;
            n->ctime = cur_time;
            int i;
            for (i = 1; i < NUM_PTRS_INODE; i++) {
                n->ptrs[i] = 0;
            }
            //create directory entry block for ptr[0]
            int dir_block = alloc_blk();
            //I am not sure if I should check failure for alloc_blk
            n->ptrs[0] = dir_block;
            //create a block storing directory array
     /*       char * buff = malloc(FS_BLOCK_SIZE);
            dirent_t *m = (dirent_t  *)buff;
            int j;
            for (j = 0; j < 128; j++) {
               // dirent_t *d = (dirent_t *)m[j];
           m[j].valid = 0;
            }
            //write directory entry block
           block_write(buff, dir_block, 1);
       */
            block_write(buffer, create_block, 1);
        break;
        }
    }
    if (full) {
        return -ENOSPC;
    }
  block_write(&b, bid, 1);
  block_write(&buf, parent, 1);
    return 0;
}


/*
 * unlink - delete a file
 *  success - return 0
 *  errors - path resolution, ENOENT, EISDIR
 *
 */
int fs_unlink(const char *path)
{
    uint32_t cur_time = time(NULL);
    int a = path2inum(path);
    //check if path exist
    if (errno != 0) {
        return -errno;
    }
   // if (errno == ENOTDIR){
     //   return -ENOTDIR;
   // }

    //read path inode
    char bu[FS_BLOCK_SIZE];
    block_read(&bu, a, 1);
    inode_t * pa = (inode_t *)bu;
    //check if path/file is a directory
    if (S_ISDIR(pa->mode)){
            return -EISDIR;
    }

    int parent = parent_dir_inum;
    char buf[FS_BLOCK_SIZE];
    block_read(&buf, parent, 1);
    inode_t * r = (inode_t *)buf;
    //check if parent exist
    if (bit_test((unsigned char *)bitmap, parent) == 0) {
        return -ENOENT;
    }
    //check if parent is a directory
    if (!S_ISDIR(r->mode)){
            return -ENOTDIR;
    }
    //get the file name, which is at index h-1
    char *_path = strdup(path);
    char * s_names[11];
    char *t;
    int h = 0;
    t = strtok(_path, "/");
    while (t != NULL) {
            s_names[h++] = t;
            t = strtok(NULL, "/");
    }

    if(parent_len < h){
  //        printf("{%d}\n",parent_len);
          return -ENOTDIR;
     }
    //find the file in the parent directory
    int bid = r->ptrs[0];
    char b[FS_BLOCK_SIZE];
    block_read(&b, bid, 1);
    dirent_t * dir = (dirent_t *)b;
    int d;
    for (d = 0; d < FS_BLOCK_SIZE/sizeof(dirent_t); d++) {
        if (dir[d].valid == 1 && !strcmp(dir[d].name, s_names[h-1])) {
            //unlink file from parent directory
            //free all the data block in the unlinked file
            int f = dir[d].inode;
            char bufe[FS_BLOCK_SIZE];
            block_read(&bufe, f, 1);
            inode_t * p = (inode_t *)bufe;
        p->mtime = cur_time;
            int i;
            for (i = 0; i < NUM_PTRS_INODE; i++) {
                if (p->ptrs[i] != 0) {
                    free_blk(p->ptrs[i]);
                }
            }
       // p->mtime = cur_time;
        dir[d].valid = 0;
            free_blk(dir[d].inode);
        p->mtime = cur_time;
        //inode_t * l = (inode_t *)b;
      //  l->mtime = time(NULL);
        block_write(&b, bid, 1);
           // block_write(&bufe, f, 1);
            break;
        }
    }
    r->mtime = time(NULL);
    block_write(&buf, parent, 1);
    //write parent directory array to disk
   // block_write(&b, bid, 1);

    return 0;
}

/*
 * rmdir - remove a directory
 *  success - return 0
 *  Errors - path resolution, ENOENT, ENOTDIR, ENOTEMPTY
 */
int fs_rmdir(const char *path)
{
    int a = path2inum(path);
    //check if path exist
    if (errno != 0) {
        return -errno;
    }
    //read path inode
    char bu[FS_BLOCK_SIZE];
    block_read(&bu, a, 1);
    inode_t * pa = (inode_t *)bu;
    
    //check if path/file is a directory
    if (S_ISREG(pa->mode)){
            return -ENOTDIR;
    }
    int parent = parent_dir_inum;
    //get parent inode
    char buf[FS_BLOCK_SIZE];
    block_read(&buf, parent, 1);
    inode_t * r = (inode_t *)buf;
    //check if parent exist
    if (bit_test((unsigned char *)bitmap, parent) == 0) {
        return -ENOENT;
    }
    //check if parent is a directory
    if (!S_ISDIR(r->mode)){
            return -ENOTDIR;
    }
    
    //get the directory name, which is at index h-1
    char *_path = strdup(path);
    char * s_names[11];
    char *t;
    int h = 0;
    t = strtok(_path, "/");
    while (t != NULL) {
            s_names[h++] = t;
            t = strtok(NULL, "/");
    }
    //find the directory in the parent directory
    int bid = r->ptrs[0];
    char b[FS_BLOCK_SIZE];
    block_read(&b, bid, 1);
    dirent_t * dir = (dirent_t *)b;
    int d;
    for (d = 0; d < FS_BLOCK_SIZE/sizeof(dirent_t); d++) {
        if (dir[d].valid == 1 && !strcmp(dir[d].name, s_names[h-1])) {
            //find the directory
            //get  the directory 's inode and check if the directory array is empty
            int g = dir[d].inode;
            char c[FS_BLOCK_SIZE];
            block_read(&c, g, 1);
            inode_t * k = (inode_t *)c;
            
            char bufr[FS_BLOCK_SIZE];
            //when I make a directory, I allocate a block for ptrs[0], so ptrs[0] is not zero.
            int p = k->ptrs[0];
            block_read(&bufr, p, 1);
            dirent_t * f = (dirent_t *)bufr;
            //check if the directory array is empty
            int j;
            for (j = 0; j < 128; j++) {
                if (f[j].valid == 1) {
                    //have file/directory in use
                    return -ENOTEMPTY;
                }
            }
            //free ptrs[0],directory array
            free_blk(p);
            k->mtime = time(NULL);
            block_write(&c, g, 1);
            free_blk(g);
            //safely delete
            dir[d].valid = 0;
        break;
        }
    }
    block_write(&b, bid, 1);
    r->mtime = time(NULL);
    block_write(&buf, parent, 1);
    return 0;
}

/*
 * write - write data to a file
 * success - return number of bytes written. (this will be the same as
 *           the number requested, or else it's an error)
 *
 * Errors - path resolution, ENOENT, EISDIR, ENOSPC
 *  return EINVAL if 'offset' is greater than current file length.
 *  (POSIX semantics support the creation of files with "holes" in them,
 *   but we don't)
 *  return ENOSPC when the data exceed the maximum size of a file.
 */
int fs_write(const char *path, const char *buf, size_t len,
         off_t offset, struct fuse_file_info *fi)
{
   // errno = 0;
    int num = 0;
    int o = len;
   // char* _buf = strdup(buf);
    int a = path2inum(path);
    if (errno == ENOTDIR || errno == ENOENT) {
        return -errno;
    }
   /* char b[FS_BLOCK_SIZE];
    block_read(&b, a, 1);
    //get the file's inode
    inode_t * n = (inode_t *)b;
    */
  /* if (offset > n->size + 1) {
     printf("a");
     printf("%d[%d]\n",offset, n->size);
        return -EINVAL;
    }*/
    if (offset + len > FS_BLOCK_SIZE*NUM_PTRS_INODE) {
//      printf("b");
        return -ENOSPC;
    }
    char b[FS_BLOCK_SIZE];
    block_read(&b, a, 1);
    //get the file's inode
    inode_t * n = (inode_t *)b;
 //   printf("write n.size:%d\n", n->size);
    if (offset > n->size) {
  //       printf("a");
       //  printf("%d[%d]\n",offset, n->size);
        return -EINVAL;
    }
   // int overwrite = n->size - offset;
   // int w = 1;
    if (offset + len > n->size){
        //no need to change the size
      n->size = offset + len;
    }
    int data_block = offset / FS_BLOCK_SIZE;
   // printf("[%d][%d]\n", offset, data_block);
    //start byte in the data block
    int start = offset-FS_BLOCK_SIZE * data_block;
   // printf("start byte[%d], start block[%d], offset[%d]\n",start, data_block,offset);
    while (len > 0 && data_block < NUM_PTRS_INODE) {
        
        char write[FS_BLOCK_SIZE];
        //memcpy from original block to start
    if (n->ptrs[data_block] == 0) {
//        printf("Y");
            int create_block = alloc_blk();
        n->ptrs[data_block] = create_block;
        block_write(&b, a, 1);
            if (create_block < 0) {
        //    printf("x");
                    n->mtime = time(NULL);
                    block_write(&b, a, 1);
                    return n->size;
            }
    }

        if (start != 0) {
            char buffer[FS_BLOCK_SIZE];
            block_read(&buffer, n->ptrs[data_block], 1);
            memcpy(&write[0], buffer, start);
        }
        
        if (len >= FS_BLOCK_SIZE - start) {
            memcpy(&write[start], buf, FS_BLOCK_SIZE - start);
            buf += FS_BLOCK_SIZE - start;
//        n->size += FS_BLOCK_SIZE - start;
            len -= FS_BLOCK_SIZE - start;
        num += FS_BLOCK_SIZE - start;
        } else{
        char buffer[FS_BLOCK_SIZE];
            block_read(&buffer, n->ptrs[data_block], 1);
           // memcpy(&write[0], buffer, start);
            memcpy(&write[start], buf, len);
        memcpy(&write[start + len], &buffer[start+len], FS_BLOCK_SIZE-len);
            buf += len;
//        n->size += len;
            len = 0;
        num += len;
        }

        block_write(&write, n->ptrs[data_block], 1);
        
       // block_write(write, n->ptrs[data_block], 1);
        data_block += 1;
        start = 0;
}
   // printf("%c\n", *buf);
    n->mtime = time(NULL);
  //  n->size = n->size - overwrite;
    block_write(&b, a, 1);
   // char u[FS_BLOCK_SIZE];
     //block_read(&u, a, 1);

    /* your code here */
    return o;
}

/*
 * truncate - truncate file to exactly 'len' bytes
 * note that CS5600 fs only allows len=0, meaning discard all data in this file.
 *
 * success - return 0
 * Errors - path resolution, ENOENT, EISDIR, EINVAL
 *    return EINVAL if len > 0.
 */
int fs_truncate(const char *path, off_t len)
{
   if (len > 0) {
        return -EINVAL;
    }
    errno = 0;
    int a = path2inum(path);
    if (errno == ENOTDIR || errno == ENOENT) {
        return -errno;
    }
    char b[FS_BLOCK_SIZE];
    block_read(&b, a, 1);
    //get the file's inode
    inode_t * n = (inode_t *)b;
    int i = 0;
    while (n->ptrs[i] != 0) {
        free_blk(n->ptrs[i]);
        n->ptrs[i] = 0;
        i++;
    }
    n->size = 0;
    block_write(&b, a, 1);
    return 0;
   
}

/* 
 * Change file's last modification time.
 *
 * notes:
 *  - read "man 2 utime" to know more.
 *  - when "ut" is NULL, update the time to now (i.e., time(NULL))
 *  - you only need to use the "modtime" in "struct utimbuf" (ignore "actime")
 *    and you only have to update "mtime" in inode.
 *
 * success - return 0
 * Errors - path resolution, ENOENT
 */
int fs_utime(const char *path, struct utimbuf *ut)
{
    errno = 0;
    int a = path2inum(path);
    if (errno == ENOTDIR || errno == ENOENT) {
        return -ENOENT;
    }
    char b[FS_BLOCK_SIZE];
    block_read(&b, a, 1);
    //get the file's inode
    inode_t * n = (inode_t *)b;
    if (ut == NULL) {
        n->mtime = time(NULL);
    }
    else {
        n->mtime = ut->modtime;
    }
   block_write(&b, a, 1);
    /* your code here */
    return 0;

}



/* operations vector. Please don't rename it, or else you'll break things
 */
struct fuse_operations fs_ops = {
    .init = fs_init,            /* read-mostly operations */
    .statfs = fs_statfs,
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .read = fs_read,
    .rename = fs_rename,
    .chmod = fs_chmod,

    .create = fs_create,        /* write operations */
    .mkdir = fs_mkdir,
    .unlink = fs_unlink,
    .rmdir = fs_rmdir,
    .write = fs_write,
    .truncate = fs_truncate,
    .utime = fs_utime,
};




