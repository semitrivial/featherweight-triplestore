#include "feather.h"
#include "srv.h"

char *html;
char *js;

int main( int argc, const char* argv[] )
{
  FILE *fp;

  if ( argc != 3 )
  {
    printf( "Syntax: feather <partsfile> <tripsfile>\nWhere <partsfile> is a parts file and <tripsfile> is a triples file\n\n" );
    return 0;
  }

  fp = fopen( argv[1], "r" );

  if ( !fp )
  {
    fprintf( stderr, "Could not open file %s for reading\n\n", argv[1] );
    return 0;
  }

  init_feather();
  init_feather_http_server();

  parse_feather_parts_file(fp);

  fclose(fp);

  fp = fopen( argv[2], "r" );

  if ( !fp )
  {
    fprintf( stderr, "Could not open file %s for reading\n\n", argv[2] );
    return 0;
  }

  parse_feather_trips_file(fp);

  fclose(fp);

  printf( "Ready.\n" );

  while(1)
  {
    /*
     * To do: make this commandline-configurable
     */
    usleep(10000);
    main_loop();
  }
}

void init_feather_http_server( void )
{
  int status, yes=1;
  struct addrinfo hints, *servinfo;
  char portstr[128];

  html = load_file( "gui.html" );
  js = load_file( "gui.js" );

  memset( &hints, 0, sizeof(hints) );
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  sprintf( portstr, "%d", 5053 );

  if ( (status=getaddrinfo(NULL,portstr,&hints,&servinfo)) != 0 )
  {
    fprintf( stderr, "Fatal: Couldn't open server port (getaddrinfo failed)\n" );
    abort();
  }

  srvsock = socket( servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol );

  if ( srvsock == -1 )
  {
    fprintf( stderr, "Fatal: Couldn't open server port (socket failed)\n" );
    abort();
  }

  if ( setsockopt( srvsock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int) ) == -1 )
  {
    fprintf( stderr, "Fatal: Couldn't open server port (setsockopt failed)\n" );
    abort();
  }

  if ( bind( srvsock, servinfo->ai_addr, servinfo->ai_addrlen ) == -1 )
  {
    fprintf( stderr, "Fatal: Couldn't open server port (bind failed)\n" );
    abort();
  }

  if ( listen( srvsock, HTTP_LISTEN_BACKLOG ) == -1 )
  {
    fprintf( stderr, "Fatal: Couldn't open server port (listen failed)\n" );
    abort();
  }

  return;
}

void main_loop( void )
{
  http_request *req;
  int count=0;

  while ( count < 10 )
  {
    req = http_recv();

    if ( req )
    {
      char *reqptr, *reqtype, *request, repl[MAX_STRING_LEN];

      count++;

      if ( !strcmp( req->query, "gui" )
      ||   !strcmp( req->query, "/gui" )
      ||   !strcmp( req->query, "gui/" )
      ||   !strcmp( req->query, "/gui/" ) )
      {
        send_gui( req );
        continue;
      }

      if ( !strcmp( req->query, "js/" )
      ||   !strcmp( req->query, "/js/" ) )
      {
        send_js( req );
        continue;
      }

      for ( reqptr = (*req->query == '/') ? req->query + 1 : req->query; *reqptr; reqptr++ )
        if ( *reqptr == '/' )
          break;

      if ( !*reqptr )
      {
        send_400_response( req, "Syntax Error" );
        continue;
      }

      *reqptr = '\0';
      reqtype = (*req->query == '/') ? req->query + 1 : req->query;
      request = url_decode(&reqptr[1]);

      if ( !strcmp( reqtype, "shortpath" ) )
      {
        handle_shortpath( request, req );
        free( request );
        return;
      }

      if ( !strcmp( reqtype, "subgraph" ) )
      {
        handle_subgraph_request( req, request );
        free( request );
        return;
      }

      if ( !strcmp( reqtype, "count" ) )
        count = get_count_by_iri( request );
      else if ( !strcmp( reqtype, "count-recursive" ) )
        count = get_recursive_count_by_iri( request );
      else
      {
        *reqptr = '/';
        free( request );
        send_400_response( req, "Syntax Error" );
        continue;
      }

      sprintf( repl, "{\"Results\": [%d]}", count );

      send_200_response( req, repl );
      *reqptr = '/';
      free( request );
    }
    else
      break;
  }
}

void http_update_connections( void )
{
  static struct timeval zero_time;
  http_conn *c, *c_next;
  int top_desc;

  FD_ZERO( &http_inset );
  FD_ZERO( &http_outset );
  FD_ZERO( &http_excset );
  FD_SET( srvsock, &http_inset );
  top_desc = srvsock;

  for ( c = first_http_conn; c; c = c->next )
  {
    if ( c->sock > top_desc )
      top_desc = c->sock;

    if ( c->state == HTTP_SOCKSTATE_READING_REQUEST )
      FD_SET( c->sock, &http_inset );
    else
      FD_SET( c->sock, &http_outset );

    FD_SET( c->sock, &http_excset );
  }

  /*
   * Poll sockets
   */
  if ( select( top_desc+1, &http_inset, &http_outset, &http_excset, &zero_time ) < 0 )
  {
    fprintf( stderr, "Fatal: select failed to poll the sockets\n" );
    abort();
  }

  if ( !FD_ISSET( srvsock, &http_excset ) && FD_ISSET( srvsock, &http_inset ) )
    http_answer_the_phone( srvsock );

  for ( c = first_http_conn; c; c = c_next )
  {
    c_next = c->next;

    c->idle++;

    if ( FD_ISSET( c->sock, &http_excset )
    ||   c->idle > HTTP_KICK_IDLE_AFTER_X_SECS * HTTP_PULSES_PER_SEC )
    {
      FD_CLR( c->sock, &http_inset );
      FD_CLR( c->sock, &http_outset );

      http_kill_socket( c );
      continue;
    }

    if ( c->state == HTTP_SOCKSTATE_AWAITING_INSTRUCTIONS )
      continue;

    if ( c->state == HTTP_SOCKSTATE_READING_REQUEST
    &&   FD_ISSET( c->sock, &http_inset ) )
    {
      c->idle = 0;
      http_listen_to_request( c );
    }
    else
    if ( c->state == HTTP_SOCKSTATE_WRITING_RESPONSE
    &&   c->outbuflen > 0
    &&   FD_ISSET( c->sock, &http_outset ) )
    {
      c->idle = 0;
      http_flush_response( c );
    }
  }
}

void http_kill_socket( http_conn *c )
{
  UNLINK2( c, first_http_conn, last_http_conn, next, prev );

  free( c->buf );
  free( c->outbuf );
  free_http_request( c->req );

  close( c->sock );

  free( c );
}

void free_http_request( http_request *r )
{
  if ( !r )
    return;

  if ( r->dead )
    *r->dead = 1;

  UNLINK2( r, first_http_req, last_http_req, next, prev );

  if ( r->query )
    free( r->query );

  free( r );
}

void http_answer_the_phone( int srvsock )
{
  struct sockaddr_storage their_addr;
  socklen_t addr_size = sizeof(their_addr);
  int caller;
  http_conn *c;
  http_request *req;

  if ( ( caller = accept( srvsock, (struct sockaddr *) &their_addr, &addr_size ) ) < 0 )
    return;

  if ( ( fcntl( caller, F_SETFL, FNDELAY ) ) == -1 )
  {
    close( caller );
    return;
  }

  CREATE( c, http_conn, 1 );
  c->next = NULL;
  c->sock = caller;
  c->idle = 0;
  c->state = HTTP_SOCKSTATE_READING_REQUEST;
  c->writehead = NULL;

  CREATE( c->buf, char, HTTP_INITIAL_INBUF_SIZE + 1 );
  c->bufsize = HTTP_INITIAL_INBUF_SIZE;
  c->buflen = 0;
  *c->buf = '\0';

  CREATE( c->outbuf, char, HTTP_INITIAL_OUTBUF_SIZE + 1 );
  c->outbufsize = HTTP_INITIAL_OUTBUF_SIZE + 1;
  c->outbuflen = 0;
  *c->outbuf = '\0';

  CREATE( req, http_request, 1 );
  req->next = NULL;
  req->conn = c;
  req->query = NULL;
  c->req = req;

  LINK2( req, first_http_req, last_http_req, next, prev );
  LINK2( c, first_http_conn, last_http_conn, next, prev );
}

int resize_buffer( http_conn *c, char **buf )
{
  int max, *size;
  char *tmp;

  if ( *buf == c->buf )
  {
    max = HTTP_MAX_INBUF_SIZE;
    size = &c->bufsize;
  }
  else
  {
    max = HTTP_MAX_OUTBUF_SIZE;
    size = &c->outbufsize;
  }

  *size *= 2;
  if ( *size >= max )
  {
    http_kill_socket(c);
    return 0;
  }

  CREATE( tmp, char, (*size)+1 );

  sprintf( tmp, "%s", *buf );
  free( *buf );
  *buf = tmp;
  return 1;
}

void http_listen_to_request( http_conn *c )
{
  int start = c->buflen, readsize;

  if ( start >= c->bufsize - 5
  &&  !resize_buffer( c, &c->buf ) )
    return;

  readsize = recv( c->sock, c->buf + start, c->bufsize - 5 - start, 0 );

  if ( readsize > 0 )
  {
    c->buflen += readsize;
    http_parse_input( c );
    return;
  }

  if ( readsize == 0 || errno != EWOULDBLOCK )
    http_kill_socket( c );
}

void http_flush_response( http_conn *c )
{
  int sent_amount;

  if ( !c->writehead )
    c->writehead = c->outbuf;

  sent_amount = send( c->sock, c->writehead, c->outbuflen, 0 );

  if ( sent_amount >= c->outbuflen )
  {
    http_kill_socket( c );
    return;
  }

  c->outbuflen -= sent_amount;
  c->writehead = &c->writehead[sent_amount];
}

void http_parse_input( http_conn *c )
{
  char *bptr, *end, query[MAX_STRING_LEN], *qptr;
  int spaces = 0, chars = 0;

  end = &c->buf[c->buflen];

  for ( bptr = c->buf; bptr < end; bptr++ )
  {
    switch( *bptr )
    {
      case ' ':
        spaces++;

        if ( spaces == 2 )
        {
          *qptr = '\0';
          c->req->query = strdup( query );
          c->state = HTTP_SOCKSTATE_AWAITING_INSTRUCTIONS;
          return;
        }
        else
        {
          *query = '\0';
          qptr = query;
        }

        break;

      case '\0':
      case '\n':
      case '\r':
        http_kill_socket( c );
        return;

      default:
        if ( spaces != 0 )
        {
          if ( ++chars >= MAX_STRING_LEN - 10 )
          {
            http_kill_socket( c );
            return;
          }
          *qptr++ = *bptr;
        }
        break;
    }
  }
}

http_request *http_recv( void )
{
  http_request *req, *best = NULL;
  int max_idle = -1;

  http_update_connections();

  for ( req = first_http_req; req; req = req->next )
  {
    if ( req->conn->state == HTTP_SOCKSTATE_AWAITING_INSTRUCTIONS
    &&   req->conn->idle > max_idle )
    {
      max_idle = req->conn->idle;
      best = req;
    }
  }

  if ( best )
  {
    best->conn->state = HTTP_SOCKSTATE_WRITING_RESPONSE;
    return best;
  }

  return NULL;
}

void http_write( http_request *req, char *txt )
{
  http_send( req, txt, strlen(txt) );
}

/*
 * Warning: can crash if txt is huge.  Up to caller
 * to ensure txt is not too huge.
 */
void http_send( http_request *req, char *txt, int len )
{
  http_conn *c = req->conn;

  if ( len >= c->outbufsize - 5 )
  {
    char *newbuf;

    CREATE( newbuf, char, len+10 );
    memcpy( newbuf, txt, len );
    free( c->outbuf );
    c->outbuf = newbuf;
    c->outbuflen = len;
    c->outbufsize = len+10;
    return;
  }

  memcpy( c->outbuf, txt, len );
  c->outbuflen = len;
}

void send_400_response( http_request *req, char *err )
{
  char buf[MAX_STRING_LEN];

  sprintf( buf, "HTTP/1.1 400 Bad Request\r\n"
                "Date: %s\r\n"
                "Content-Type: text/plain; charset=utf-8\r\n"
                "%s"
                "Content-Length: %zd\r\n"
                "\r\n"
                "%s",
                current_date(),
                nocache_headers(),
                strlen( err ),
                err );

  http_write( req, buf );
}

void send_200_response( http_request *req, char *txt )
{
  send_200_with_type( req, txt, "application/json" );
}

void send_200_with_type( http_request *req, char *txt, char *type )
{
  char buf[2*MAX_STRING_LEN];

  sprintf( buf, "HTTP/1.1 200 OK\r\n"
                "Date: %s\r\n"
                "Content-Type: %s; charset=utf-8\r\n"
                "%s"
                "Content-Length: %zd\r\n"
                "\r\n"
                "%s",
                current_date(),
                type,
                nocache_headers(),
                strlen(txt),
                txt );

  http_write( req, buf );
}

char *nocache_headers(void)
{
  return "Cache-Control: no-cache, no-store, must-revalidate\r\n"
         "Pragma: no-cache\r\n"
         "Expires: 0\r\n";
}

char *current_date(void)
{
  time_t rawtime;
  struct tm *timeinfo;
  static char buf[2048];
  char *bptr;

  time ( &rawtime );
  timeinfo = localtime( &rawtime );
  sprintf( buf, "%s", asctime ( timeinfo ) );

  for ( bptr = &buf[strlen(buf)-1]; *bptr == '\n' || *bptr == '\r'; bptr-- )
    ;

  bptr[1] = '\0';

  return buf;
}

void send_gui( http_request *req )
{
  send_200_with_type( req, html, "text/html" );
}

void send_js( http_request *req )
{
  send_200_with_type( req, js, "application/javascript" );
}

char *load_file( char *filename )
{
  FILE *fp;
  char *buf, *bptr;
  int size;

  /*
   * Variables for QUICK_GETC
   */
  char read_buf[READ_BLOCK_SIZE], *read_end = &read_buf[READ_BLOCK_SIZE], *read_ptr = read_end;
  int fread_len;

  fp = fopen( filename, "r" );

  if ( !fp )
  {
    fprintf( stderr, "Fatal: Couldn't open %s for reading\n", filename );
    abort();
  }

  fseek(fp, 0L, SEEK_END);
  size = ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  CREATE( buf, char, size+1 );

  for ( bptr = buf; ; bptr++ )
  {
    QUICK_GETC(*bptr,fp);

    if ( !*bptr )
      return buf;
  }
}

void handle_shortpath( char *request, http_request *req )
{
  char *ptr;
  trie *x, *y;
  trie_path *p;

  for ( ptr = request; *ptr; ptr++ )
    if ( *ptr == ',' )
      break;

  if ( !*ptr )
  {
    send_400_response( req, "Syntax Error: expected comma" );
    return;
  }

  *ptr = '\0';

  if ( !(x = trie_search( request, iritrie ))
  ||   !(y = trie_search( &ptr[1], iritrie )) )
  {
    send_400_response( req, "Unrecognized class" );
    return;
  }

  p = calculate_shortest_path( x, y );

  if ( !p )
    send_200_response( req, "{\"Results\": []}" );
  else
  {
    char *buf = malloc( sizeof(char) * p->length * (MAX_LABEL_LEN*2) + 256);
    char *bptr;
    trie **tptr;
    int fFirst = 0, *iptr;

    sprintf( buf, "{\"Results\": [" );
    bptr = &buf[strlen(buf)];

    for ( tptr = p->steps; *tptr; tptr++ )
    {
      if ( fFirst )
        *bptr++ = ',';
      else
        fFirst = 1;

      sprintf( bptr, "%s", get_url_shortform(trie_to_static( *tptr )) );
      bptr = &bptr[strlen(bptr)];
    }

    sprintf( bptr, "], \"Relations\": [" );
    bptr = &bptr[strlen(bptr)];

    for ( fFirst = 0, iptr = p->reln_types; *iptr != -1; iptr++ )
    {
      if ( fFirst )
        *bptr++ = ',';
      else
        fFirst = 1;

      sprintf( bptr, "%s", reln_type_to_string( *iptr ) );
      bptr = &bptr[strlen(bptr)];
    }

    sprintf( bptr, "]}" );

    send_200_response( req, buf );
    free( buf );
    free_trie_path( p );
  }
}

void handle_subgraph_request( http_request *req, char *request )
{
  int commas = 0, fEnd = 0, count=0, fFirst=0;
  trie **nodes, **nptr, *t;
  char *ptr, *left, *buf, *bptr, *prefix;
  subgraph_reln *head, *reln, *reln_next;

  for ( ptr = request; *ptr; ptr++ )
    if ( *ptr == ',' )
      commas++;

  if ( commas < 1 )
  {
    send_200_response( req, "{\"Results\": []}" );
    return;
  }

  if ( commas > MAX_SUBGRAPH_NODES )
  {
    send_200_response( req, "Too many nodes" );
    return;
  }

  /*
   * First argument is prefix, hence commas + 1 instead of commas + 2 in the next line
   */
  CREATE( nodes, trie *, commas + 1 );
  nptr = nodes;

  left = ptr = request;

  for ( ; ; )
  {
    if ( *ptr == ',' || !*ptr )
    {
      if ( *ptr )
        *ptr = '\0';
      else
        fEnd = 1;

      if ( left == ptr && fFirst )
      {
        send_400_response( req, "Blank node name" );
        free( nodes );
        return;
      }

      if ( !fFirst )
      {
        fFirst = 1;
        prefix = left;
        left = &ptr[1];
        ptr++;
        continue;
      }

      t = trie_search_with_prefix( prefix, left, iritrie );
      if ( !t )
      {
        if ( strlen(left) < 100 )
        {
          char *buf = malloc( strlen(left) + 1024 );
          sprintf( buf, "Node %s%s is not in the database", prefix, left );
          send_400_response( req, buf );
          free(buf);
        }
        else
          send_400_response( req, "One of the specified nodes is not in the database" );

        free( nodes );
        return;
      }

      *nptr++ = t;
      left = &ptr[1];
    }
    if ( fEnd )
      break;

    ptr++;
  }

  *nptr = NULL;

  head = compute_subgraph( nodes, &count );
  free( nodes );

  CREATE( buf, char, 256 + 2*count*(MAX_LABEL_LEN+64) );

  sprintf( buf, "{\n \"Results\":\n [\n" );

  bptr = &buf[strlen(buf)];

  for ( reln = head, fFirst = 0; reln; reln = reln_next )
  {
    reln_next = reln->next;

    if ( fFirst )
    {
      *bptr++ = ',';
      *bptr++ = '\n';
    }
    else
      fFirst = 1;

    sprintf( bptr, "  {\n   \"parent\": %s\n", trie_to_static( reln->parent ) );
    bptr = &bptr[strlen(bptr)];
    sprintf( bptr, "   \"child\": %s\n   \"type\": %s\n  }", trie_to_static( reln->child ), reln_type_to_string( reln->reln_type ) );
    bptr = &bptr[strlen(bptr)];

    free( reln );
  }

  sprintf( bptr, "\n ]\n}" );

  send_200_response( req, buf );
  free( buf );
}
