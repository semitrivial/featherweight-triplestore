#include "macro.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define MAX_STRING_LEN 64000
#define READ_BLOCK_SIZE 1048576  // 1024 * 1024.  For QUICK_GETC.

/*
 * Typedefs
 */
typedef struct TRIE trie;

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
  int seen;
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
void add_feather_part( char *bigger, char *smaller );
int get_count_by_iri( char *iri );
int get_recursive_count_by_iri( char *iri );
void add_to_data( trie ***dest, trie *datum );
void increment_recursive_count( trie *t );
void cleanup_recursive( trie *t );

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
