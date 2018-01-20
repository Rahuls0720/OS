#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "filesys/free-map.h"

/* Notes: 
      update: filesys_create() to add files to directories other than root
      update: sys calls that call filesys_create() to add file to different directories
      update: filesys_open() and filesys_remove to look in all directories
      update: use search_fs() instead of lookup() in dir_add()
      implement: get_parent_dir() and get_name() once parser is done
*/

// Forward Declarations
uint32_t get_array_length(char **array);
struct dir* search_fs(char *name);

// char **path_to_array(const char *path);
void addWord(char ***array, int currentSize, char* word);

/* A directory. */
struct dir
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file/dir name. */
    bool in_use;                        /* Is file in use or free? */
    bool directory;                     /* Is entry a directory? */
  };

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR. Adds this directory to the CWD.
   Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, size_t entry_cnt, char **path) //dir is the abs/relative path 
{
  if (inode_create (sector, entry_cnt * sizeof (struct dir_entry), true)){
    if(sector != ROOT_DIR_SECTOR){ //add new directory entry to parent directory
      char** abs_path = get_abs_path(&path);
      return dir_add(get_parent_dir(abs_path), get_name(abs_path), sector, true); 
    }
    return true;
  }
  return false;
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode)
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL;
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir)
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir)
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir)
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp)
{
  struct dir_entry e;
  size_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (e.in_use && !strcmp (name, e.name))
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode)
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

/* If path is relative, returns absolute.
   'path' is freed in the process, so be sure
   to update any pointers pointing to 'path'. */
char** get_abs_path(char ***path){
  // Return null if already absolute
  if (strcmp("/", (*path)[0]) == 0 || (*path)[0] == NULL) {
    //return NULL;
	return *path;
  }

  // cwd should always be an absolute path
  char **returnArray = path_to_array(thread_current()->cwd);
  int cwd_words = get_array_length(returnArray);
  
  // Overwrite ending NULL
  if ((*path)[1] == NULL) {
    return returnArray;
  }
  int wordLen = strlen((*path)[1]);
  returnArray[cwd_words] = (char*) malloc(sizeof(char) * (wordLen + 1));
  strlcpy(returnArray[cwd_words], (*path)[1], wordLen+1);
  cwd_words++;


  // Append relative path to cwd
  int i = 2;
  while ((*path)[i] != NULL) {
    addWord(&returnArray, cwd_words,  (*path)[i]);  
    i++;
    cwd_words++;
  }

  // Terminate with NULL
  returnArray = realloc(returnArray, sizeof(char*) * (cwd_words + 1));
  returnArray[cwd_words] = NULL;

  freeArray(path);
  return returnArray;
}

/* Returns the name of the last entry in path */
char* get_name(char **path) {
  return path[get_array_length(path) - 1];
}

/* Returns the length of the array.
   ie. How many words it holds */
uint32_t get_array_length(char **array) {
  int length = 0;
  while (array[length] != NULL) {
  length++; 
  }
  return length;
}

/* Returns the parent directory last entry in path.
   Precondition: path must be an absolute path */
struct dir*
get_parent_dir(char **path) {
  /* Find name of parent
     Travel down from root, according to path,
	 until inode name matches */

	int arrayLen = get_array_length(path);
	struct dir *cwd = dir_open_root();
	struct inode *currentInode = cwd->inode;

	int i = 1;
	while (i < (arrayLen - 1)) {
		dir_lookup(cwd, path[i], &currentInode);
		if (currentInode != NULL && currentInode->data.isdir) {
			cwd = dir_open(inode_open (currentInode->sector));
			i++;
		} else {
			return NULL;
		}
	}

	return cwd;
}

/* Returns a struct dir of the cwd.
   May want to change this to a generic
   find dir function. */
struct dir*
get_cwd(void) {
	struct thread *t = thread_current();
	char **tempArray = path_to_array(t->cwd);
	struct dir *cwd = dir_open_root();
	struct inode *currentInode = cwd->inode;

	int i = 1;
	while (tempArray[i] != NULL) {
		dir_lookup(cwd, tempArray[i], &currentInode);
		if (currentInode != NULL && currentInode->data.isdir) {
			// Do we need to close directories too?
			cwd = dir_open(inode_open (currentInode->sector));
			i++;
		} else {
			return NULL;
		}
	}

	freeArray(&tempArray);
	return dir_open(inode_open(currentInode->sector));
}

/* Searches the entires fs for file/dir named Name */
struct dir*
search_fs(char *name){
  //call lookup
  return NULL;
}

/* Adds a file or dir named NAME to current working DIR, 
   which must not already contain a file/dir by that name.
   The file's inode is in sector INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector, bool directory) 
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.

     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (!e.in_use)
      break;

  /* Write slot. */
  if(directory) { 
    e.directory = true; 
  }else{
    e.directory = false;
  }
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name)
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Get inode to remove, but don't open it. */
  inode = inode_open (e.inode_sector);
  inode->open_cnt--;
  if (inode == NULL)
    goto done;
  if (inode->open_cnt > 0) {
 	inode_close(inode);
	return false;
  }

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e)
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e)
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        }
    }
  return false;
}

/* Cleans 'path' and returns an array of path strings with '/'
   as the delimeter. Calling process must call 'free_path()'
   after use. */
char
**path_to_array(const char *path) {
	char *savePtr;
	char **returnArray = NULL;

	/* Copying input to maintain const */
	char *pathCopy = malloc(sizeof(char) * (strlen(path) + 1));
	strlcpy(pathCopy, path, strlen(path)+1);

	/* Looping and buildling array */
	char *word = strtok_r(pathCopy, "/", &savePtr);
	int wordCount = 0;
	
	// Absolute or Relative
	if (pathCopy[0] == '/') {
		addWord(&returnArray, wordCount, "/");
		wordCount++;
	} else {
		addWord(&returnArray, wordCount, ".");
		wordCount++;
	}

	while (word != NULL) {
		// Skip over '.' b/c it's just the current directory
		if (strcmp(word, ".") != 0) {
			addWord(&returnArray, wordCount, word);
			wordCount++;
		}
		word = strtok_r(NULL, "/", &savePtr);
	}

	// Terminate with NULL
	returnArray = realloc(returnArray, sizeof(char*) * (wordCount + 1));
	returnArray[wordCount] = NULL;
	
	return returnArray;
}

/* Adds 'word' to 'array' */
void
addWord(char ***array, int currentSize, char* word) {
	// Allocate space for new array entry
	*array = realloc(*array, sizeof(char*) * (currentSize + 1));

	// Allocate and copy word into array
	(*array)[currentSize] = (char*) malloc(sizeof(char) * (strlen(word) + 1));
	strlcpy((*array)[currentSize], word, strlen(word)+1);
}

void freeArray(char ***array) {
	int i = 0;
	while((*array)[i] != NULL) {
		free((*array)[i]);
		i++;
	}
	free(*array);
}
