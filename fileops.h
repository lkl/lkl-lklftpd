#ifndef LKLFTPD__FILEOPS__H__
#define LKLFTPD__FILEOPS__H__

#ifndef LKL_FILE_APIS

#include <apr_file_io.h>

#define lkl_file_t   apr_file_t
#define lkl_dir_t    apr_dir_t

#define lkl_file_read		apr_file_read
#define lkl_file_writev		apr_file_writev
#define lkl_file_writev_full	apr_file_writev_full
#define lkl_file_open		apr_file_open
#define lkl_file_close		apr_file_close
#define lkl_file_write		apr_file_write
#define lkl_file_remove		apr_file_remove
#define lkl_file_rename		apr_file_rename
#define lkl_file_copy		apr_file_copy
#define lkl_file_append		apr_file_append
#define lkl_file_eof		apr_file_eof
#define lkl_file_readfull	apr_file_readfull
#define lkl_file_writefull	apr_file_writefull
#define lkl_file_putc		apr_file_putc
#define lkl_file_getc		apr_file_getc
#define lkl_file_ungetc		apr_file_ungetc
#define lkl_file_puts		apr_file_puts
#define lkl_file_gets		apr_file_gets
#define lkl_file_flush		apr_file_flush
#define lkl_file_seek		apr_file_seek
#define lkl_file_lock		apr_file_lock
#define lkl_file_unlock		apr_file_unlock
#define lkl_file_write_full	apr_file_write_full



#define lkl_file_perms_set	apr_file_perms_set
#define lkl_file_attrs_set	apr_file_attrs_set
#define lkl_file_mtime_set	apr_file_mtime_set
#define lkl_file_info_get	apr_file_info_get
#define lkl_file_trunc		apr_file_trunc
#define lkl_file_flags_get	apr_file_flags_get
#define lkl_file_mktemp		apr_file_mktemp

#define lkl_stat		apr_stat

#define lkl_dir_make		apr_dir_make
#define lkl_dir_remove		apr_dir_remove
#define lkl_dir_open		apr_dir_open
#define lkl_dir_close		apr_dir_close
#define lkl_dir_read		apr_dir_read

#else//LKL_FILE_APIS

#include <apr.h>
#include <apr_pools.h>
#include <apr_time.h>
#include <apr_errno.h>
#include <apr_file_info.h>
#include <apr_inherit.h>
#include <apr_file_io.h>
#include <apr_poll.h>
#include <apr_strings.h>

#include <asm/lkl.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
struct lkl_file_t
{
	apr_pool_t *pool;
	int filedes;
	char *fname;
	apr_int32_t flags;
	int eof_hit;
	int is_pipe;
	apr_interval_time_t timeout;
	int buffered;
	enum {BLK_UNKNOWN, BLK_OFF, BLK_ON } blocking;
	int ungetchar;    /* Last char provided by an unget op. (-1 = no char)*/

	/* Stuff for buffered mode */
	char *buffer;
	int bufpos;               /* Read/Write position in buffer */
	unsigned long dataRead;   /* amount of valid data read into buffer */
	int direction;            /* buffer being used for 0 = read, 1 = write */
	apr_off_t filePtr;        /* position in file of handle */
	struct apr_thread_mutex_t *thlock;
};

typedef struct lkl_file_t lkl_file_t;
struct linux_dirent;
struct lkl_dir_t
{
	apr_pool_t *pool;
	char * dirname; // dir name
	int fd; // file descriptor
	struct __kernel_dirent * data; // dir block
	int offset; // current offset
	int size; // total valid data
	struct __kernel_dirent *entry; // current entry
};
typedef struct lkl_dir_t lkl_dir_t;

#define file_lock(f)   do { \
                           if ((f)->thlock) \
                               apr_thread_mutex_lock((f)->thlock); \
                       } while (0)
#define file_unlock(f) do { \
                           if ((f)->thlock) \
                               apr_thread_mutex_unlock((f)->thlock); \
                       } while (0)

apr_fileperms_t lkl_unix_mode2perms(mode_t mode);
mode_t lkl_unix_perms2mode(apr_fileperms_t perms);
apr_status_t lkl_file_flush_locked(lkl_file_t *thefile);
apr_status_t lkl_file_info_get_locked(apr_finfo_t *finfo, apr_int32_t wanted,
                                      lkl_file_t *thefile);


apr_status_t lkl_file_open(lkl_file_t **newf, const char *fname,
                                        apr_int32_t flag, apr_fileperms_t perm,
                                        apr_pool_t *pool);

/**
 * Close the specified file.
 * @param file The file descriptor to close.
 */
apr_status_t lkl_file_close(lkl_file_t *file);

/**
 * Delete the specified file.
 * @param path The full path to the file (using / on all systems)
 * @param pool The pool to use.
 * @remark If the file is open, it won't be removed until all
 * instances are closed.
 */
apr_status_t lkl_file_remove(const char *path, apr_pool_t *pool);

/**
 * Rename the specified file.
 * @param from_path The full path to the original file (using / on all systems)
 * @param to_path The full path to the new file (using / on all systems)
 * @param pool The pool to use.
 * @warning If a file exists at the new location, then it will be
 * overwritten.  Moving files or directories across devices may not be
 * possible.
 */
apr_status_t lkl_file_rename(const char *from_path,
                                          const char *to_path,
                                          apr_pool_t *pool);

/**
 * Copy the specified file to another file.
 * @param from_path The full path to the original file (using / on all systems)
 * @param to_path The full path to the new file (using / on all systems)
 * @param perms Access permissions for the new file if it is created.
 *     In place of the usual or'd combination of file permissions, the
 *     value APR_FILE_SOURCE_PERMS may be given, in which case the source
 *     file's permissions are copied.
 * @param pool The pool to use.
 * @remark The new file does not need to exist, it will be created if required.
 * @warning If the new file already exists, its contents will be overwritten.
 */
apr_status_t lkl_file_copy(const char *from_path,
                                        const char *to_path,
                                        apr_fileperms_t perms,
                                        apr_pool_t *pool);

/**
 * Append the specified file to another file.
 * @param from_path The full path to the source file (use / on all systems)
 * @param to_path The full path to the destination file (use / on all systems)
 * @param perms Access permissions for the destination file if it is created.
 *     In place of the usual or'd combination of file permissions, the
 *     value APR_FILE_SOURCE_PERMS may be given, in which case the source
 *     file's permissions are copied.
 * @param pool The pool to use.
 * @remark The new file does not need to exist, it will be created if required.
 */
apr_status_t lkl_file_append(const char *from_path,
                                          const char *to_path,
                                          apr_fileperms_t perms,
                                          apr_pool_t *pool);

/**
 * Are we at the end of the file
 * @param fptr The apr file we are testing.
 * @remark Returns APR_EOF if we are at the end of file, APR_SUCCESS otherwise.
 */
apr_status_t lkl_file_eof(lkl_file_t *fptr);

/**
 * Read data from the specified file.
 * @param thefile The file descriptor to read from.
 * @param buf The buffer to store the data to.
 * @param nbytes On entry, the number of bytes to read; on exit, the number
 * of bytes read.
 *
 * @remark apr_file_read will read up to the specified number of
 * bytes, but never more.  If there isn't enough data to fill that
 * number of bytes, all of the available data is read.  The third
 * argument is modified to reflect the number of bytes read.  If a
 * char was put back into the stream via ungetc, it will be the first
 * character returned.
 *
 * @remark It is not possible for both bytes to be read and an APR_EOF
 * or other error to be returned.  APR_EINTR is never returned.
 */
apr_status_t lkl_file_read(lkl_file_t *thefile, void *buf,
                                        apr_size_t *nbytes);

/**
 * Write data to the specified file.
 * @param thefile The file descriptor to write to.
 * @param buf The buffer which contains the data.
 * @param nbytes On entry, the number of bytes to write; on exit, the number
 *               of bytes written.
 *
 * @remark apr_file_write will write up to the specified number of
 * bytes, but never more.  If the OS cannot write that many bytes, it
 * will write as many as it can.  The third argument is modified to
 * reflect the * number of bytes written.
 *
 * @remark It is possible for both bytes to be written and an error to
 * be returned.  APR_EINTR is never returned.
 */
apr_status_t lkl_file_write(lkl_file_t *thefile, const void *buf,
                                         apr_size_t *nbytes);

/**
 * Write data from iovec array to the specified file.
 * @param thefile The file descriptor to write to.
 * @param vec The array from which to get the data to write to the file.
 * @param nvec The number of elements in the struct iovec array. This must
 *             be smaller than APR_MAX_IOVEC_SIZE.  If it isn't, the function
 *             will fail with APR_EINVAL.
 * @param nbytes The number of bytes written.
 *
 * @remark It is possible for both bytes to be written and an error to
 * be returned.  APR_EINTR is never returned.
 *
 * @remark apr_file_writev is available even if the underlying
 * operating system doesn't provide writev().
 */
apr_status_t lkl_file_writev(lkl_file_t *thefile,
                                          const struct iovec *vec,
                                          apr_size_t nvec, apr_size_t *nbytes);

/**
 * Read data from the specified file, ensuring that the buffer is filled
 * before returning.
 * @param thefile The file descriptor to read from.
 * @param buf The buffer to store the data to.
 * @param nbytes The number of bytes to read.
 * @param bytes_read If non-NULL, this will contain the number of bytes read.
 *
 * @remark apr_file_read will read up to the specified number of
 * bytes, but never more.  If there isn't enough data to fill that
 * number of bytes, then the process/thread will block until it is
 * available or EOF is reached.  If a char was put back into the
 * stream via ungetc, it will be the first character returned.
 *
 * @remark It is possible for both bytes to be read and an error to be
 * returned.  And if *bytes_read is less than nbytes, an accompanying
 * error is _always_ returned.
 *
 * @remark APR_EINTR is never returned.
 */
apr_status_t lkl_file_read_full(lkl_file_t *thefile, void *buf,
                                             apr_size_t nbytes,
                                             apr_size_t *bytes_read);

/**
 * Write data to the specified file, ensuring that all of the data is
 * written before returning.
 * @param thefile The file descriptor to write to.
 * @param buf The buffer which contains the data.
 * @param nbytes The number of bytes to write.
 * @param bytes_written If non-NULL, set to the number of bytes written.
 *
 * @remark apr_file_write will write up to the specified number of
 * bytes, but never more.  If the OS cannot write that many bytes, the
 * process/thread will block until they can be written. Exceptional
 * error such as "out of space" or "pipe closed" will terminate with
 * an error.
 *
 * @remark It is possible for both bytes to be written and an error to
 * be returned.  And if *bytes_written is less than nbytes, an
 * accompanying error is _always_ returned.
 *
 * @remark APR_EINTR is never returned.
 */
apr_status_t lkl_file_write_full(lkl_file_t *thefile,
                                              const void *buf,
                                              apr_size_t nbytes,
                                              apr_size_t *bytes_written);


/**
 * Write data from iovec array to the specified file, ensuring that all of the
 * data is written before returning.
 * @param thefile The file descriptor to write to.
 * @param vec The array from which to get the data to write to the file.
 * @param nvec The number of elements in the struct iovec array. This must
 *             be smaller than APR_MAX_IOVEC_SIZE.  If it isn't, the function
 *             will fail with APR_EINVAL.
 * @param nbytes The number of bytes written.
 *
 * @remark apr_file_writev_full is available even if the underlying
 * operating system doesn't provide writev().
 */
apr_status_t lkl_file_writev_full(lkl_file_t *thefile,
                                               const struct iovec *vec,
                                               apr_size_t nvec,
                                               apr_size_t *nbytes);
/**
 * Write a character into the specified file.
 * @param ch The character to write.
 * @param thefile The file descriptor to write to
 */
apr_status_t lkl_file_putc(char ch, lkl_file_t *thefile);

/**
 * Read a character from the specified file.
 * @param ch The character to read into
 * @param thefile The file descriptor to read from
 */
apr_status_t lkl_file_getc(char *ch, lkl_file_t *thefile);

/**
 * Put a character back onto a specified stream.
 * @param ch The character to write.
 * @param thefile The file descriptor to write to
 */
apr_status_t lkl_file_ungetc(char ch, lkl_file_t *thefile);

/**
 * Read a string from the specified file.
 * @param str The buffer to store the string in.
 * @param len The length of the string
 * @param thefile The file descriptor to read from
 * @remark The buffer will be NUL-terminated if any characters are stored.
 */
apr_status_t lkl_file_gets(char *str, int len,
                                        lkl_file_t *thefile);

/**
 * Write the string into the specified file.
 * @param str The string to write.
 * @param thefile The file descriptor to write to
 */
apr_status_t lkl_file_puts(const char *str, lkl_file_t *thefile);

/**
 * Flush the file's buffer.
 * @param thefile The file descriptor to flush
 */
apr_status_t lkl_file_flush(lkl_file_t *thefile);


/**
 * Move the read/write file offset to a specified byte within a file.
 * @param thefile The file descriptor
 * @param where How to move the pointer, one of:
 * <PRE>
 *            APR_SET  --  set the offset to offset
 *            APR_CUR  --  add the offset to the current position
 *            APR_END  --  add the offset to the current file size
 * </PRE>
 * @param offset The offset to move the pointer to.
 * @remark The third argument is modified to be the offset the pointer
          was actually moved to.
 */
apr_status_t lkl_file_seek(lkl_file_t *thefile, apr_seek_where_t where, apr_off_t *offset);


/** file (un)locking functions. */

/**
 * Establish a lock on the specified, open file. The lock may be advisory
 * or mandatory, at the discretion of the platform. The lock applies to
 * the file as a whole, rather than a specific range. Locks are established
 * on a per-thread/process basis; a second lock by the same thread will not
 * block.
 * @param thefile The file to lock.
 * @param type The type of lock to establish on the file.
 */
apr_status_t lkl_file_lock(lkl_file_t *thefile, int type);

/**
 * Remove any outstanding locks on the file.
 * @param thefile The file to unlock.
 */
apr_status_t lkl_file_unlock(lkl_file_t *thefile);

/**accessor and general file_io functions. */

/**
 * return the file name of the current file.
 * @param new_path The path of the file.
 * @param thefile The currently open file.
 */
apr_status_t lkl_file_name_get(const char **new_path,
                                            lkl_file_t *thefile);

/**
 * set the specified file's permission bits.
 * @param fname The file (name) to apply the permissions to.
 * @param perms The permission bits to apply to the file.
 *
 * @warning Some platforms may not be able to apply all of the
 * available permission bits; APR_INCOMPLETE will be returned if some
 * permissions are specified which could not be set.
 *
 * @warning Platforms which do not implement this feature will return
 * APR_ENOTIMPL.
 */
apr_status_t lkl_file_perms_set(const char *fname,
                                             apr_fileperms_t perms);

/**
 * Set attributes of the specified file.
 * @param fname The full path to the file (using / on all systems)
 * @param attributes Or'd combination of
 * <PRE>
 *            APR_FILE_ATTR_READONLY   - make the file readonly
 *            APR_FILE_ATTR_EXECUTABLE - make the file executable
 *            APR_FILE_ATTR_HIDDEN     - make the file hidden
 * </PRE>
 * @param attr_mask Mask of valid bits in attributes.
 * @param pool the pool to use.
 * @remark This function should be used in preference to explict manipulation
 *      of the file permissions, because the operations to provide these
 *      attributes are platform specific and may involve more than simply
 *      setting permission bits.
 * @warning Platforms which do not implement this feature will return
 *      APR_ENOTIMPL.
 */
apr_status_t lkl_file_attrs_set(const char *fname,
                                             apr_fileattrs_t attributes,
                                             apr_fileattrs_t attr_mask,
                                             apr_pool_t *pool);

/**
 * Set the mtime of the specified file.
 * @param fname The full path to the file (using / on all systems)
 * @param mtime The mtime to apply to the file.
 * @param pool The pool to use.
 * @warning Platforms which do not implement this feature will return
 *      APR_ENOTIMPL.
 */
apr_status_t lkl_file_mtime_set(const char *fname,
                                             apr_time_t mtime,
                                             apr_pool_t *pool);
/**
 * get the specified file's stats.
 * @param finfo Where to store the information about the file.
 * @param wanted The desired apr_finfo_t fields, as a bit flag of APR_FINFO_ values
 * @param thefile The file to get information about.
 */
apr_status_t lkl_file_info_get(apr_finfo_t *finfo,
                                            apr_int32_t wanted,
                                            lkl_file_t *thefile);


/**
 * Truncate the file's length to the specified offset
 * @param fp The file to truncate
 * @param offset The offset to truncate to.
 */
apr_status_t lkl_file_trunc(lkl_file_t *fp, apr_off_t offset);

/**
 * Retrieve the flags that were passed into apr_file_open()
 * when the file was opened.
 * @return apr_int32_t the flags
 */
apr_int32_t lkl_file_flags_get(lkl_file_t *f);


/**
 * Open a temporary file
 * @param fp The apr file to use as a temporary file.
 * @param templ The template to use when creating a temp file.
 * @param flags The flags to open the file with. If this is zero,
 *              the file is opened with
 *              APR_CREATE | APR_READ | APR_WRITE | APR_EXCL | APR_DELONCLOSE
 * @param p The pool to allocate the file out of.
 * @remark
 * This function  generates  a unique temporary file name from template.
 * The last six characters of template must be XXXXXX and these are replaced
 * with a string that makes the filename unique. Since it will  be  modified,
 * template must not be a string constant, but should be declared as a character
 * array.
 *
 */
apr_status_t lkl_file_mktemp(lkl_file_t **fp, char *templ,
                                          apr_int32_t flags, apr_pool_t *p);



apr_status_t lkl_stat(apr_finfo_t *finfo, const char* fname, apr_int32_t wanted, apr_pool_t *pool);

/**
* Open the specified directory.
* @param new_dir The opened directory descriptor.
* @param dirname The full path to the directory (use / on all systems)
* @param cont The pool to use.
*
*/
apr_status_t lkl_dir_open (lkl_dir_t ** new,const char * dirname,apr_pool_t *  pool);

/**
* Read the next entry from the specified directory.
* @param finfo 	the file info structure and filled in by apr_dir_read
* @param wanted The desired apr_finfo_t fields, as a bit flag of APR_FINFO_ values
* @param thedir the directory descriptor returned from apr_dir_open
* @emark
* No ordering is guaranteed for the entries read.
*
*/
apr_status_t lkl_dir_read(apr_finfo_t * finfo, apr_int32_t wanted, lkl_dir_t * thedir);

/**
* Close the specified directory.
* @param thedir the directory descriptor to close.
*
*/
apr_status_t lkl_dir_close(lkl_dir_t * thedir);

/**
* Create a new directory on the file system.
* @param path 	the path for the directory to be created. (use / on all systems)
* @param perm 	Permissions for the new direcoty.
* @param pool 	the pool to use.
*/
apr_status_t lkl_dir_make(const char *path, apr_fileperms_t perm,
                          apr_pool_t *pool);

/**
* Remove directory from the file system.
* @param path 	the path for the directory to be removed. (use / on all systems)
* @param pool 	the pool to use.
*/
apr_status_t lkl_dir_remove(const char *path, apr_pool_t *pool);


#ifdef __cplusplus
}
#endif




#endif//LKL_FILE_APIS

#endif//LKLFTPD__FILEOPS__H__
