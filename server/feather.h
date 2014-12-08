#include "macro.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define MAX_STRING_LEN 64000
#define READ_BLOCK_SIZE 1048576  // 1024 * 1024.  For QUICK_GETC.

#define MAX_LABEL_LEN 256
#define MAX_PATH_LEN 256

#define RELN_TYPE_SUB 1
#define RELN_TYPE_PART 2
#define RELN_TYPE_SEED 3

/*
 * Typedefs
 */
typedef struct TRIE trie;
typedef struct TRIE_PATH trie_path;
typedef struct ONE_STEP one_step;

/*
 * Structures
 */
struct TRIE
{
  trie *parent;
  char *label;
  trie **children;
  int count;
  int recursive_count;
  trie **data;
  int *reln_types;
  int seen;
};

struct TRIE_PATH
{
  int length;
  trie **steps;
  int *reln_types;
};

struct ONE_STEP
{
  one_step *next;
  one_step *prev;
  int depth;
  one_step *backtrace;
  trie *location;
  int reln_type;
};

/*
 * Global variables
 */
extern trie *iritrie;

/*
 * Function prototypes
 */

/*
 * feather.c
 */
void init_feather(void);
void parse_feather_trips_file(FILE *fp);
void parse_feather_parts_file(FILE *fp);
void add_feather_entry( char *subject, char *predicate, char *object );
void add_feather_part( char *bigger, char *smaller, int reln_type );
int get_count_by_iri( char *iri );
int get_recursive_count_by_iri( char *iri );
void add_to_data( trie ***dest, trie *datum, int **dest_reln_parts, int reln_part );
void increment_recursive_count( trie *t );
void cleanup_recursive( trie *t );
void free_trie_path( trie_path *p );
trie_path *calculate_shortest_path( trie *anc, trie *des );
void free_one_steps( one_step *head );

/*
 * srv.c
 */
void main_loop(void);

/*
 * trie.c
 */
trie *blank_trie(void);
trie *trie_strdup( char *buf, trie *base );
trie *trie_search( char *buf, trie *base );
char *trie_to_static( trie *t );

/*
 * util.c
 */
void log_string( char *txt );
char *html_encode( char *str );
void init_html_codes( void );
char *lowercaserize( char *x );
char *get_url_shortform( char *iri );
char *url_decode(char *str);
char *reln_type_to_string( int reln_type );
