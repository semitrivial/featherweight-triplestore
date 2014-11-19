#include "feather.h"

void init_feather(void)
{
  iritrie = blank_trie();

  init_html_codes();

  return;
}

void parse_feather_parts_file(FILE *fp)
{
  char firstword[MAX_STRING_LEN], *fptr = firstword;
  char bigger[MAX_STRING_LEN], *bptr = bigger;
  char smaller[MAX_STRING_LEN], *sptr = smaller;
  int fSpace = 0, fSpace2 = 0;
  int linenum = 1;
  char c;

  /*
   * Variables for QUICK_GETC
   */
  char read_buf[READ_BLOCK_SIZE], *read_end = &read_buf[READ_BLOCK_SIZE], *read_ptr = read_end;
  int fread_len;

  printf( "Parsing partsfile...\n" );

  for (;;)
  {
    QUICK_GETC(c,fp);

    if ( !c )
      break;

    if ( c == '\n' )
    {
      *sptr = '\0';
      add_feather_part(bigger,smaller);
      fptr = firstword;
      bptr = bigger;
      sptr = smaller;
      fSpace = 0;
      fSpace2 = 0;
      linenum++;
      continue;
    }

    if ( !fSpace && c == ' ' )
    {
      if ( fptr == firstword )
      {
        fprintf( stderr, "Error on line %d of feather-file: blank firstword\n\n", linenum );
        abort();
      }

      *fptr = '\0';
      bptr = bigger;
      fSpace = 1;
      continue;
    }

    if ( !fSpace2 && c == ' ' )
    {
      if ( bptr == bigger )
      {
        fprintf( stderr, "Error on line %d of feather-file: blank bigger part\n\n", linenum );
        abort();
      }

      *bptr = '\0';
      sptr = smaller;
      fSpace2 = 1;
      continue;
    }

    if ( !fSpace )
      *fptr++ = c;
    else if ( !fSpace2 )
      *bptr++ = c;
    else
      *sptr++ = c;
  }

  printf( "Finished parsing file\n" );
  return;
}

void parse_feather_trips_file(FILE *fp)
{
  char c;
  char subject[MAX_STRING_LEN], *sptr = subject;
  char predicate[MAX_STRING_LEN], *pptr = predicate;
  char object[MAX_STRING_LEN], *optr = object;
  int fSpace = 0, fSpace2 = 0;
  int linenum = 1;

  /*
   * Variables for QUICK_GETC
   */
  char read_buf[READ_BLOCK_SIZE], *read_end = &read_buf[READ_BLOCK_SIZE], *read_ptr = read_end;
  int fread_len;

  printf( "Parsing tripsfile...\n" );

  for(;;)
  {
    QUICK_GETC(c,fp);

    if ( !c )
      break;

    if ( c == '\n' )
    {
      *optr = '\0';
      add_feather_entry( subject, predicate, object );
      sptr = subject;
      pptr = predicate;
      fSpace = 0;
      fSpace2 = 0;
      linenum++;
      continue;
    }

    if ( !fSpace && c == ' ' )
    {
      if ( sptr == subject )
      {
        fprintf( stderr, "Error on line %d of feather-file: blank subject\n\n", linenum );
        abort();
      }

      *sptr = '\0';
      pptr = predicate;
      fSpace = 1;
      continue;
    }

    if ( !fSpace2 && c == ' ' )
    {
      if ( pptr == predicate )
      {
        fprintf( stderr, "Error on line %d of feather-file: blank predicate\n\n", linenum );
        abort();
      }

      *pptr = '\0';
      optr = object;
      fSpace2 = 1;
      continue;
    }

    if ( !fSpace )
      *sptr++ = c;
    else if ( !fSpace2 )
      *pptr++ = c;
    else
      *optr++ = c;
  }

  printf( "Finished parsing file.\n" );

  return;
}

void add_to_data( trie ***dest, trie *datum )
{
  trie **data;
  int cnt;

  if ( !*dest )
  {
    CREATE( *dest, trie *, 2 );
    (*dest)[0] = datum;
    (*dest)[1] = NULL;
  }
  else
  {
    for ( data = *dest, cnt = 0; *data; data++ )
      cnt++;

    CREATE( data, trie *, cnt + 2 );
    memcpy( data, *dest, cnt * sizeof(trie*) );
    data[cnt] = datum;
    free( *dest );
    *dest = data;
  }
}

void add_feather_entry( char *subject, char *predicate, char *object )
{
  trie *object_t = trie_strdup( object, iritrie );
  trie_strdup( subject, iritrie );
  trie_strdup( predicate, iritrie );

  object_t->count++;

  increment_recursive_count( object_t );
}

void increment_recursive_count( trie *t )
{
  trie **ptr;

  t->recursive_count++;

  if ( t->data )
    for ( ptr = t->data; *ptr; ptr++ )
      increment_recursive_count( *ptr );
}

int get_count_by_iri( char *iri )
{
  trie *iri_t = trie_search( iri, iritrie );

  if ( !iri_t )
    return 0;

  return iri_t->count;
}

int get_recursive_count_by_iri( char *iri )
{
  trie *iri_t = trie_search( iri, iritrie );

  if ( !iri_t )
    return 0;

  return iri_t->recursive_count;
}

void add_feather_part( char *bigger, char *smaller )
{
  trie *bigtrie = trie_strdup( bigger, iritrie );
  trie *smalltrie = trie_strdup( smaller, iritrie );

  add_to_data( &smalltrie->data, bigtrie );

  return;
}
