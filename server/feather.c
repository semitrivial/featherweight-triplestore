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
  int reln_type;

  printf( "Parsing partsfile...\n" );

  for (;;)
  {
    QUICK_GETC(c,fp);

    if ( !c )
      break;

    if ( c == '\n' )
    {
      *sptr = '\0';
      add_feather_part(bigger,smaller,reln_type);
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

      if ( !strcmp( firstword, "Sub" ) )
        reln_type = RELN_TYPE_SUB;
      else
      if ( !strcmp( firstword, "Part" ) )
        reln_type = RELN_TYPE_PART;
      else
      if ( !strcmp( firstword, "Seed" ) )
        reln_type = RELN_TYPE_SEED;
      else
      {
        fprintf( stderr, "Error on line %d of feather-file: firstword is neither 'Sub' nor 'Part'\n\n", linenum );
        abort();
      }

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

void add_to_data( trie ***dest, trie *datum, int **dest_reln_parts, int reln_part )
{
  trie **data;
  int *parts;
  int cnt;

  if ( !*dest )
  {
    CREATE( *dest, trie *, 2 );
    (*dest)[0] = datum;
    (*dest)[1] = NULL;

    CREATE( *dest_reln_parts, int, 2 );
    (*dest_reln_parts)[0] = reln_part;
    (*dest_reln_parts)[1] = -1;
  }
  else
  {
    for ( data = *dest, cnt = 0; *data; data++ )
      cnt++;

    CREATE( data, trie *, cnt + 2 );
    memcpy( data, *dest, cnt * sizeof(trie*) );
    data[cnt] = datum;
    data[cnt+1] = NULL;
    free( *dest );
    *dest = data;

    CREATE( parts, int, cnt + 2 );
    memcpy( parts, *dest_reln_parts, cnt * sizeof(int) );
    parts[cnt] = reln_part;
    parts[cnt+1] = -1;
    free( *dest_reln_parts );
    *dest_reln_parts = parts;
  }
}

void add_feather_entry( char *subject, char *predicate, char *object )
{
  trie *object_t = trie_strdup( object, iritrie );
  trie_strdup( subject, iritrie );
  trie_strdup( predicate, iritrie );

  object_t->count++;

  increment_recursive_count( object_t );
  cleanup_recursive( object_t );
}

void increment_recursive_count( trie *t )
{
  trie **ptr;

  t->recursive_count++;
  t->seen = 1;

  if ( t->data )
  {
    for ( ptr = t->data; *ptr; ptr++ )
    {
      if ( !(*ptr)->seen )
        increment_recursive_count( *ptr );
    }
  }
}

void cleanup_recursive( trie *t )
{
  trie **ptr;

  t->seen = 0;

  if ( t->data )
  {
    for ( ptr = t->data; *ptr; ptr++ )
    {
      if ( (*ptr)->seen )
        cleanup_recursive( *ptr );
    }
  }
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

void add_feather_part( char *bigger, char *smaller, int reln_type )
{
  trie *bigtrie = trie_strdup( bigger, iritrie );
  trie *smalltrie = trie_strdup( smaller, iritrie );

  add_to_data( &smalltrie->data, bigtrie, &smalltrie->reln_types, reln_type );

  return;
}

void free_trie_path( trie_path *p )
{
  free( p->steps );
  free( p->reln_types );
  free( p );
}

trie_path *calculate_shortest_path( trie *anc, trie *des )
{
  one_step *head=NULL, *tail=NULL, *step, *curr;

  if ( anc == des )
  {
    trie_path *path;

    CREATE( path, trie_path, 1 );
    path->length = 1;
    CREATE( path->steps, trie *, 2 );
    path->steps[0] = anc;
    path->steps[1] = NULL;
    CREATE( path->reln_types, int, 1 );
    path->reln_types[0] = -1;

    return path;
  }

  if ( !des->data || !*des->data )
    return NULL;

  CREATE( step, one_step, 1 );
  step->depth = 0;
  step->backtrace = NULL;
  step->location = des;
  step->reln_type = -1;
  des->seen = 1;

  LINK2( step, head, tail, next, prev );
  curr = step;

  for ( ; ; curr = curr->next )
  {
    trie **parents;

    if ( !curr || curr->depth > MAX_PATH_LEN - 3 )
    {
      free_one_steps( head );
      cleanup_recursive( des );
      return NULL;
    }

    if ( curr->location == anc )
    {
      trie **steps, **sptr;
      trie_path *path;
      int *iptr;

      CREATE( path, trie_path, 1 );
      path->length = curr->depth + 1;

      CREATE( steps, trie *, curr->depth + 2 );
      sptr = steps;

      CREATE( path->reln_types, int, curr->depth + 1 );
      iptr = path->reln_types;

      do
      {
        *sptr++ = curr->location;

        if ( curr->reln_type != -1 )
          *iptr++ = curr->reln_type;

        curr = curr->backtrace;
      }
      while ( curr );

      *sptr = NULL;
      *iptr = -1;

      path->steps = steps;

      free_one_steps( head );
      cleanup_recursive( des );

      return path;
    }

    if ( curr->location->data )
    {
      int *iptr;

      for ( parents = curr->location->data, iptr = curr->location->reln_types; *parents; parents++, iptr++ )
      {
        if ( (*parents)->seen )
          continue;

        CREATE( step, one_step, 1 );
        step->depth = curr->depth + 1;
        step->backtrace = curr;
        step->location = *parents;
        step->reln_type = *iptr;
        LINK2( step, head, tail, next, prev );

        (*parents)->seen = 1;
      }
    }
  }
}

void free_one_steps( one_step *head )
{
  one_step *next;

  for ( ; head; head = next )
  {
    next = head->next;
    free( head );
  }
}

subgraph_reln *compute_subgraph( trie **nodes, int *count )
{
  trie **tptr, **node;
  subgraph_reln *reln_head = NULL, *reln_tail = NULL, *reln;

  for ( tptr = nodes; *tptr; tptr++ )
    SET_BIT( (*tptr)->seen, SUBGRAPH_NODE );

  for ( node = nodes; *node; node++ )
  {
    one_step *step_head = NULL, *step_tail = NULL, *step, *curr;

    if ( IS_SET( (*node)->seen, SUBGRAPH_AVOID_DUPES ) )
      continue;
    SET_BIT( (*node)->seen, SUBGRAPH_AVOID_DUPES );

    CREATE( step, one_step, 1 );
    step->location = *node;
    step->reln_type = -1;
    step->backtrace = NULL;
    LINK2( step, step_head, step_tail, next, prev );
    SET_BIT( (*node)->seen, SUBGRAPH_VISITED );

    for ( curr = step; curr; curr = curr->next )
    {
      if ( IS_SET( curr->location->seen, SUBGRAPH_NODE ) && curr->location != *node )
      {
        CREATE( reln, subgraph_reln, 1 );
        reln->parent = curr->location;
        reln->child = *node;
        reln->reln_type = curr->reln_type;
        LINK2( reln, reln_head, reln_tail, next, prev );
        (*count)++;
      }
      else if ( curr->location->data )
      {
        int *iptr;

        for ( tptr = curr->location->data, iptr = curr->location->reln_types; *tptr; tptr++, iptr++ )
        {
          if ( IS_SET( (*tptr)->seen, SUBGRAPH_VISITED ) )
            continue;

          CREATE( step, one_step, 1 );
          step->location = *tptr;
          step->backtrace = curr;
          step->reln_type = *iptr;
          LINK2( step, step_head, step_tail, next, prev );
          SET_BIT( (*tptr)->seen, SUBGRAPH_VISITED );
        }
      }
    }

    for ( step = step_head; step; step = step->next )
      REMOVE_BIT( step->location->seen, SUBGRAPH_VISITED );

    free_one_steps( step_head );
  }

  for ( tptr = nodes; *tptr; tptr++ )
    (*tptr)->seen = 0;

  return reln_head;
}
