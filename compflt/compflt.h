#include <linux/crypto.h>
#include <linux/list.h>
#include <linux/fs.h>
#include "../redirfs/redirfs.h"

#define CFLT_MAGIC "\x06\x10\x19\x82"
#define CFLT_FH_SIZE 9
#define CFLT_BH_SIZE 13
#define CFLT_BLKSIZE_MIN 512
#define CFLT_BLKSIZE_MAX 32768
#define CFLT_BLKSIZE_MOD 512
#define CFLT_DEFAULT_BLKSIZE 4096
#define CFLT_DEFAULT_METHOD "deflate"
enum { CFLT_BLK_NORM, CFLT_BLK_FREE }; // block types

struct cflt_block {
	struct list_head file;
        int dirty;
        unsigned int type; // u8 (0 == free , 1 == normal)
	unsigned int off_u; // u32
	unsigned int off_c; // not written to file
	unsigned int off_next; // u32
	unsigned int size_u; // u16
	unsigned int size_c; // u16
        struct cflt_file *par; // parent cflt_file
        char *data_u;
        char *data_c;
};

struct cflt_inode {
	struct list_head blks;
	struct list_head all; // used for statistics
        // struct list_head free;
	struct inode *inode;
	unsigned int method; // u8
        unsigned int blksize; // u32
	int compressed;
	int dirty;
        size_t size; // whole uncompressed size
};

// base.c
extern rfs_filter compflt;

// file.c
extern struct list_head cflt_file_list;
struct cflt_file *cflt_file_get_header(struct inode*);
void cflt_file_reset(struct cflt_file*, struct inode*);
void cflt_file_clr_blks(struct cflt_file*);
int cflt_file_cache_init(void);
void cflt_file_cache_deinit(void);
struct cflt_file *cflt_file_find(struct inode*);
int cflt_file_read(struct file*, struct cflt_file*);
void cflt_file_write(struct file*, struct cflt_file*);
int cflt_file_blksize_set(unsigned int);
void cflt_file_add_blk(struct cflt_file*, struct cflt_block*);
void cflt_file_del_blk(struct cflt_file*, struct cflt_block*);
int cflt_file_proc_blksize(char*, int);

// block.c
int cflt_block_read_headers(struct file*, struct cflt_file*);
void cflt_block_write_headers(struct file*, struct cflt_file*);
int cflt_block_cache_init(void);
void cflt_block_cache_deinit(void);
struct cflt_block* cflt_block_init(void);
void cflt_block_deinit(struct cflt_block*);
int cflt_block_read_header(struct file*, struct cflt_block*, loff_t*);
int cflt_block_write_header(struct file*, struct cflt_block*);
int cflt_block_read(struct file*, struct cflt_block*, struct crypto_comp*);
int cflt_block_write(struct file*, struct cflt_block*, struct crypto_comp*);

// read_write.c
ssize_t cflt_orig_read(struct file*, char __user*, size_t, loff_t*);
ssize_t cflt_orig_write(struct file*, const char __user*, size_t, loff_t*);
int cflt_read(struct file*, struct cflt_file*, loff_t, size_t*, char*);
int cflt_write(struct file*, struct cflt_file*, loff_t, size_t*, char*);

// compress.c
extern unsigned int cflt_cmethod;
struct crypto_comp *cflt_comp_init(unsigned int);
void cflt_comp_deinit(struct crypto_comp*);
int cflt_decomp_block(struct crypto_comp*, struct cflt_block*);
int cflt_comp_block(struct crypto_comp*, struct cflt_block*);
int cflt_comp_method_set(char*);
int cflt_comp_proc_method(char*, int);
int cflt_comp_proc_stat(char*, int);

// proc.c
int cflt_proc_init(void);
void cflt_proc_deinit(void);

// debug.c
#ifdef CFLT_DEBUG
        #define cflt_debug_printk printk
        void cflt_debug_block(struct cflt_block*);
        void cflt_debug_file_header(struct cflt_file*);
        void cflt_hexdump(void*, unsigned int);
#else
        #define cflt_debug_printk(format, args...) ;
        #define cflt_debug_block(blk) ;
        #define cflt_debug_file_header(fh) ;
        #define cflt_hexdump(buf, len) ;
#endif
