#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include "hashset.h"

#include "url.h"
#include "bool.h"
#include "urlconnection.h"
#include "streamtokenizer.h"
#include "html-utils.h"

static void Welcome(const char *welcomeTextFileName);
static void BuildIndices(const char *feedsFileName);
static void ProcessFeed(const char *remoteDocumentName);
static void PullAllNewsItems(urlconnection *urlconn);
static bool GetNextItemTag(streamtokenizer *st);
static void ProcessSingleNewsItem(streamtokenizer *st);
static void ExtractElement(streamtokenizer *st, const char *htmlTag, char dataBuffer[], int bufferLength);
static void ParseArticle(const char *articleTitle, const char *articleDescription, const char *articleURL);
static void ScanArticle(streamtokenizer *st, const char *articleTitle, const char *unused, const char *articleURL);
static void QueryIndices();
static void ProcessResponse(const char *word);
static bool WordIsWellFormed(const char *word);


static const signed long kHashMultiplier = -1664117991L;
vector articleList;
static int StringHash(const void *s, int numBuckets)
{
  int i;
  unsigned long hashcode = 0;

  for (i = 0; i < strlen(s); i++)
    hashcode = hashcode * kHashMultiplier + tolower(((char*)s)[i]);
  //printf("Generate hashValue:%d\n",hashcode % numBuckets);
  return hashcode % numBuckets;
}


//the end node connect title and count
typedef struct
{
  char title[512];
  int count;
} EA;

//the word node connect word and end end node
typedef struct
{
  hashset eas;
  char word[40];
} EK;

//Finally We mannly use hashset to made a EK set
hashset eks;

//Function for ea
static int EaHash(const void *ea,int numBuckets)
{
  return StringHash(((EA*)ea)->title, numBuckets);
}

static int EaCompare(const void*ek1,const void *ek2)
{
  return strcasecmp(((EA*)ek1)->title, ((EA*)ek2)->title);
}

static void EaFree(void *ek)
{
  free(ek);
}

//Function for ek
static int EkHash(const void *ek,int numBuckets)
{
  return StringHash(((EK*)ek)->word, numBuckets);
}

static int EkCompare(const void*ek1,const void *ek2)
{
  return strcasecmp(((EK*)ek1)->word, ((EK*)ek2)->word);
}

static void EkFree(void *ek)
{
  HashSetDispose(&(((EK*)ek)->eas));
}

void awMapper(void *ea,void *word)
{
  printf("We find %s in %s appears %d times\n",word,((EA*)ea)->title,((EA*)ea)->count);
}

void listEK(void *ek,void *p)
{
  printf("show word:%s\n",((EK*)ek)->word);
}
hashset stopWord;

/**
 * Function: main
 * --------------
 * Serves as the entry point of the full application.
 * You'll want to update main to declare several hashsets--
 * one for stop words, another for previously seen urls, etc--
 * and pass them (by address) to BuildIndices and QueryIndices.
 * In fact, you'll need to extend many of the prototypes of the
 * supplied helpers functions to take one or more hashset *s.
 *
 * Think very carefully about how you're going to keep track of
 * all of the stop words, how you're going to keep track of
 * all the previously seen articles, and how you're going to 
 * map words to the collection of news articles where that
 * word appears.
 */

static const char *const kWelcomeTextFile = "./welcome.txt";
static const char *const kDefaultFeedsFile = "./chrss.txt";

int main(int argc, char **argv)
{
  HashSetNew(&eks, sizeof(EK), 200000, EkHash,EkCompare , EkFree);

  HashSetNew(&stopWord, sizeof(char)*40, 1400, StringHash, (HashSetCompareFunction)strcasecmp, free);
  FILE* stopTxt;
  stopTxt = fopen("./stop-words.txt", "r");
  char cache = getc(stopTxt);
  void* initPos;
  while(cache != EOF)
    {
      char* line = malloc(40*sizeof(char));
      initPos = line;

      while(cache != '\n' && cache != EOF)
        {
          *(line++) = cache;
          cache = getc(stopTxt);
        }
      *line = '\0';
      HashSetEnter(&stopWord, initPos);
      if(HashSetLookup(&stopWord, initPos) == NULL)
        return -1;
      free(initPos);
      cache = getc(stopTxt);
    }
  fclose(stopTxt);

  
  Welcome(kWelcomeTextFile);

  //We assumne the article at most 1024 chars,and at most 3000 articles

  VectorNew(&articleList, sizeof(char)*512, free, 3000);

  //Do StopWord Initializes

  BuildIndices((argc == 1) ? kDefaultFeedsFile : argv[1]);
  QueryIndices();
  return 0;
}

/** 
 * Function: Welcome
 * -----------------
 * Displays the contents of the specified file, which
 * holds the introductory remarks to be printed every time
 * the application launches.  This type of overhead may
 * seem silly, but by placing the text in an external file,
 * we can change the welcome text without forcing a recompilation and
 * build of the application.  It's as if welcomeTextFileName
 * is a configuration file that travels with the application.
 */
 
static const char *const kNewLineDelimiters = "\r\n";
static void Welcome(const char *welcomeTextFileName)
{
  FILE *infile;
  streamtokenizer st;
  char buffer[512];

  infile = fopen(welcomeTextFileName, "r");
  assert(infile != NULL);

  STNew(&st, infile, kNewLineDelimiters, true);
  while (STNextToken(&st, buffer, sizeof(buffer))) {
    printf("%s\n", buffer);
  }

  printf("\n");
  STDispose(&st); // remember that STDispose doesn't close the file, since STNew doesn't open one..
  fclose(infile);
}

/**
 * Function: BuildIndices
 * ----------------------
 * As far as the user is concerned, BuildIndices needs to read each and every
 * one of the feeds listed in the specied feedsFileName, and for each feed parse
 * content of all referenced articles and store the content in the hashset of indices.
 * Each line of the specified feeds file looks like this:
 *
 *   <feed name>: <URL of remore xml document>
 *
 * Each iteration of the supplied while loop parses and discards the feed name (it's
 * in the file for humans to read, but our aggregator doesn't care what the name is)
 * and then extracts the URL.  It then relies on ProcessFeed to pull the remote
 * document and index its content.
 */

static void BuildIndices(const char *feedsFileName)
{
  FILE *infile;
  streamtokenizer st;
  char remoteFileName[512];
  
  infile = fopen(feedsFileName, "r");
  assert(infile != NULL);
  STNew(&st, infile, kNewLineDelimiters, true);
  while (STSkipUntil(&st, ":") != EOF) { // ignore everything up to the first selicolon of the line
    STSkipOver(&st, ": ");		 // now ignore the semicolon and any whitespace directly after it
    STNextToken(&st, remoteFileName, sizeof(remoteFileName));
    printf("%s\n",remoteFileName);
    ProcessFeed(remoteFileName);
  }
  
  STDispose(&st);
  fclose(infile);
  printf("\n");
}


/**
 * Function: ProcessFeed
 * ---------------------
 * ProcessFeed locates the specified RSS document, and if a (possibly redirected) connection to that remote
 * document can be established, then PullAllNewsItems is tapped to actually read the feed.  Check out the
 * documentation of the PullAllNewsItems function for more information, and inspect the documentation
 * for ParseArticle for information about what the different response codes mean.
 */

static void ProcessFeed(const char *remoteDocumentName)
{
  url u;
  urlconnection urlconn;
  printf("try get remoteDocumentName:%s\n",remoteDocumentName);
  URLNewAbsolute(&u, remoteDocumentName);

  URLConnectionNew(&urlconn, &u);

  
  switch (urlconn.responseCode) {
      case 0: printf("Unable to connect to \"%s\".  Ignoring...", u.serverName);
        
              break;
      case 200: PullAllNewsItems(&urlconn);
                break;
      case 301: 
      case 302: ProcessFeed(urlconn.newUrl);
                break;
      default: printf("Connection to \"%s\" was established, but unable to retrieve \"%s\". [response code: %d, response message:\"%s\"]\n",
		      u.serverName, u.fileName, urlconn.responseCode, urlconn.responseMessage);
	       break;
  };
  
  URLConnectionDispose(&urlconn);
  URLDispose(&u);
}

/**
 * Function: PullAllNewsItems
 * --------------------------
 * Steps though the data of what is assumed to be an RSS feed identifying the names and
 * URLs of online news articles.  Check out "datafiles/sample-rss-feed.txt" for an idea of what an
 * RSS feed from the www.nytimes.com (or anything other server that syndicates is stories).
 *
 * PullAllNewsItems views a typical RSS feed as a sequence of "items", where each item is detailed
 * using a generalization of HTML called XML.  A typical XML fragment for a single news item will certainly
 * adhere to the format of the following example:
 *
 * <item>
 *   <title>At Installation Mass, New Pope Strikes a Tone of Openness</title>
 *   <link>http://www.nytimes.com/2005/04/24/international/worldspecial2/24cnd-pope.html</link>
 *   <description>The Mass, which drew 350,000 spectators, marked an important moment in the transformation of Benedict XVI.</description>
 *   <author>By IAN FISHER and LAURIE GOODSTEIN</author>
 *   <pubDate>Sun, 24 Apr 2005 00:00:00 EDT</pubDate>
 *   <guid isPermaLink="false">http://www.nytimes.com/2005/04/24/international/worldspecial2/24cnd-pope.html</guid>
 * </item>
 *
 * PullAllNewsItems reads and discards all characters up through the opening <item> tag (discarding the <item> tag
 * as well, because once it's read and indentified, it's been pulled,) and then hands the state of the stream to
 * ProcessSingleNewsItem, which handles the job of pulling and analyzing everything up through and including the </item>
 * tag. PullAllNewsItems processes the entire RSS feed and repeatedly advancing to the next <item> tag and then allowing
 * ProcessSingleNewsItem do process everything up until </item>.
 */

static const char *const kTextDelimiters = " \t\n\r\b!@$%^*()_+={[}]|\\'\":;/?.>,<~`";
static void PullAllNewsItems(urlconnection *urlconn)
{
  //printf("Get url:%s\n",urlconn->fullUrl);
  //exit(0);

  streamtokenizer st;

  STNew(&st, urlconn->dataStream, kTextDelimiters, false);
  
  while (GetNextItemTag(&st)) { // if true is returned, then assume that <item ...> has justn been read and pulled from the data stream

    ProcessSingleNewsItem(&st);
  }
  STDispose(&st);
}

/**
 * Function: GetNextItemTag
 * ------------------------
 * Works more or less like GetNextTag below, but this time
 * we're searching for an <item> tag, since that marks the
 * beginning of a block of HTML that's relevant to us.  
 * 
 * Note that each tag is compared to "<item" and not "<item>".
 * That's because the item tag, though unlikely, could include
 * attributes and perhaps look like any one of these:
 *
 *   <item>
 *   <item rdf:about="Latin America reacts to the Vatican">
 *   <item requiresPassword=true>
 *
 * We're just trying to be as general as possible without
 * going overboard.  (Note that we use strncasecmp so that
 * string comparisons are case-insensitive.  That's the case
 * throughout the entire code base.)
 */

static const char *const kItemTagPrefix = "<item";
static bool GetNextItemTag(streamtokenizer *st)
{
  char htmlTag[512];
  while (GetNextTag(st, htmlTag, sizeof(htmlTag))) {
    if (strncasecmp(htmlTag, kItemTagPrefix, strlen(kItemTagPrefix)) == 0) {
      return true;
    }
  }
  return false;
}

/**
 * Function: ProcessSingleNewsItem
 * -------------------------------
 * Code which parses the contents of a single <item> node within an RSS/XML feed.
 * At the moment this function is called, we're to assume that the <item> tag was just
 * read and that the streamtokenizer is currently pointing to everything else, as with:
 *   
 *      <title>Carrie Underwood takes American Idol Crown</title>
 *      <description>Oklahoma farm girl beats out Alabama rocker Bo Bice and 100,000 other contestants to win competition.</description>
 *      <link>http://www.nytimes.com/frontpagenews/2841028302.html</link>
 *   </item>
 *
 * ProcessSingleNewsItem parses everything up through and including the </item>, storing the title, link, and article
 * description in local buffers long enough so that the online new article identified by the link can itself be parsed
 * and indexed.  We don't rely on <title>, <link>, and <description> coming in any particular order.  We do asssume that
 * the link field exists (although we can certainly proceed if the title and article descrption are missing.)  There
 * are often other tags inside an item, but we ignore them.
 */

static const char *const kItemEndTag = "</item>";
static const char *const kTitleTagPrefix = "<title";
static const char *const kDescriptionTagPrefix = "<description";
static const char *const kLinkTagPrefix = "<link";
static void ProcessSingleNewsItem(streamtokenizer *st)
{
  char htmlTag[512];
  char articleTitle[512];
  char articleDescription[512];
  char articleURL[512];
  
  articleTitle[0] = articleDescription[0] = articleURL[0] = '\0';
  
  while (GetNextTag(st, htmlTag, sizeof(htmlTag)) && (strcasecmp(htmlTag, kItemEndTag) != 0)) {
    //htmlTag is the exactly the html tag like: <artid>
    if (strncasecmp(htmlTag, kTitleTagPrefix, strlen(kTitleTagPrefix)) == 0) {
      
      ExtractElement(st, htmlTag, articleTitle, sizeof(articleTitle));
      //printf("Get article title:%s\n",articleTitle);
    }
    if (strncasecmp(htmlTag, kDescriptionTagPrefix, strlen(kDescriptionTagPrefix)) == 0) ExtractElement(st, htmlTag, articleDescription, sizeof(articleDescription));
    if (strncasecmp(htmlTag, kLinkTagPrefix, strlen(kLinkTagPrefix)) == 0) {
      ExtractElement(st, htmlTag, articleURL, sizeof(articleURL));
      //printf("Get URL:%s\n",articleURL);
    }
  }
  
  if (strncmp(articleURL, "", sizeof(articleURL)) == 0)
    {
      printf("No URL FOUND\n");
      return;     // punt, since it's not going to take us anywhere
    }
  ParseArticle(articleTitle, articleDescription, articleURL);
}

/**
 * Function: ExtractElement
 * ------------------------
 * Potentially pulls text from the stream up through and including the matching end tag.  It assumes that
 * the most recently extracted HTML tag resides in the buffer addressed by htmlTag.  The implementation
 * populates the specified data buffer with all of the text up to but not including the opening '<' of the
 * closing tag, and then skips over all of the closing tag as irrelevant.  Assuming for illustration purposes
 * that htmlTag addresses a buffer containing "<description" followed by other text, these three scanarios are
 * handled:
 *
 *    Normal Situation:     <description>http://some.server.com/someRelativePath.html</description>
 *    Uncommon Situation:   <description></description>
 *    Uncommon Situation:   <description/>
 *
 * In each of the second and third scenarios, the document has omitted the data.  This is not uncommon
 * for the description data to be missing, so we need to cover all three scenarious (I've actually seen all three.)
 * It would be quite unusual for the title and/or link fields to be empty, but this handles those possibilities too.
 */
 
static void ExtractElement(streamtokenizer *st, const char *htmlTag, char dataBuffer[], int bufferLength)
{
  assert(htmlTag[strlen(htmlTag) - 1] == '>');
  if (htmlTag[strlen(htmlTag) - 2] == '/') return;    // e.g. <description/> would state that a description is not being supplied
  
  STSkipUntil(st, "[");
  STSkipOver(st, "[");
  STSkipUntil(st, "[");
  STSkipOver(st, "[\n");
  STNextTokenUsingDifferentDelimiters(st, dataBuffer, bufferLength, "\n");
  RemoveEscapeCharacters(dataBuffer+5);
  if (dataBuffer[0] == '<') strcpy(dataBuffer, "");  // e.g. <description></description> also means there's no description
  
  
}

/** 
 * Function: ParseArticle
 * ----------------------
 * Attempts to establish a network connect to the news article identified by the three
 * parameters.  The network connection is either established of not.  The implementation
 * is prepared to handle a subset of possible (but by far the most common) scenarios,
 * and those scenarios are categorized by response code:
 *
 *    0 means that the server in the URL doesn't even exist or couldn't be contacted.
 *    200 means that the document exists and that a connection to that very document has
 *        been established.
 *    301 means that the document has moved to a new location
 *    302 also means that the document has moved to a new location
 *    4xx and 5xx (which are covered by the default case) means that either
 *        we didn't have access to the document (403), the document didn't exist (404),
 *        or that the server failed in some undocumented way (5xx).
 *
 * The are other response codes, but for the time being we're punting on them, since
 * no others appears all that often, and it'd be tedious to be fully exhaustive in our
 * enumeration of all possibilities.
 */

static void ParseArticle(const char *articleTitle, const char *articleDescription, const char *articleURL)
{
  
  url u;
  urlconnection urlconn;
  streamtokenizer st;

  VectorSort(&articleList, (VectorCompareFunction)strcasecmp);
  if(VectorSearch(&articleList, articleTitle, (VectorCompareFunction)strcasecmp, 0, true) != -1)
    return;
  VectorAppend(&articleList, articleTitle);
  
  printf("In ParseArticle:%s\n",articleTitle);
  /* printf("In ParseArticle:%s",articleURL); */
  URLNewAbsolute(&u, articleURL);
  URLConnectionNew(&urlconn, &u);

  switch (urlconn.responseCode) {
      case 0: printf("Unable to connect to \"%s\".  Domain name or IP address is nonexistent.\n", articleURL);
	      break;
      case 200: printf("Scanning \"%s\" from \"http://%s\"\n", articleTitle, u.serverName);
	  STNew(&st, urlconn.dataStream, kTextDelimiters, false);
		ScanArticle(&st, articleTitle, articleDescription, articleURL);
		STDispose(&st);
		break;
      case 301:
      case 302: // just pretend we have the redirected URL all along, though index using the new URL and not the old one...
                ParseArticle(articleTitle, articleDescription, urlconn.newUrl);
		break;
      default: printf("Unable to pull \"%s\" from \"%s\". [Response code: %d] Punting...\n", articleTitle, u.serverName, urlconn.responseCode);
	       break;
  }
  
  URLConnectionDispose(&urlconn);
  URLDispose(&u);
}

/**
 * Function: ScanArticle
 * ---------------------
 * Parses the specified article, skipping over all HTML tags, and counts the numbers
 * of well-formed words that could potentially serve as keys in the set of indices.
 * Once the full article has been scanned, the number of well-formed words is
 * printed, and the longest well-formed word we encountered along the way
 * is printed as well.
 *
 * This is really a placeholder implementation for what will ultimately be
 * code that indexes the specified content.
 */

static void ScanArticle(streamtokenizer *st, const char *articleTitle, const char *unused, const char *articleURL)
{
  int numWords = 0;
  char word[512];
  char longestWord[512] = {'\0'};

  void *ek;
  EA *ea;

  EK* cacheek = malloc(sizeof(EK));
  EA* cacheea = malloc(sizeof(EA));
  

  while (STNextToken(st, word, sizeof(word))) {
    if (strcasecmp(word, "<") == 0) {
      SkipIrrelevantContent(st); // in html-utls.h
    } else {
      RemoveEscapeCharacters(word);
      if (WordIsWellFormed(word)) {
        numWords++;
        memcpy(cacheek->word, word, 40*sizeof(char));
        ek = HashSetLookup(&eks, cacheek);
        //printf("Well word:%s\n",word);
        if(ek==NULL)
          {
            //printf("Insert %s ",word);
            ek = malloc(sizeof(EK));
            //((EK*)ek)->eas = malloc(sizeof(EA));
            HashSetNew(&((EK*)ek)->eas, sizeof(EA), 200, EaHash, EaCompare, EaFree);
            memcpy(((EK*)ek)->word,word,40*sizeof(char));
            HashSetEnter(&eks, ek);

            //memcpy(cacheek->word, word, 40*sizeof(char));
            if (HashSetLookup(&eks, cacheek) == NULL)
            {
              printf("Exit because failed to insert %s",word);
              exit(0);
            }

            ea = malloc(sizeof(EA));
            ea->count = 1;
            memcpy(ea->title, articleTitle, sizeof(char)*512);
            HashSetEnter(&((EK*)ek)->eas, ea);
            free(ea);
            free(ek);
            ea = NULL;
            ek = NULL;
          }
        else
          {
            memcpy(cacheea->title,articleTitle,sizeof(char)*512);
            ea = HashSetLookup(&((EK*)ek)->eas,cacheea);
            if(ea == NULL)
              {
                ea = malloc(sizeof(EA));
                ea->count = 1;
                memcpy(ea->title, articleTitle, sizeof(char)*512);
                HashSetEnter(&((EK*)ek)->eas, ea);
                free(ea);
                ea = NULL;
              }
            else
              {
                ea->count++;
              }
          }
	if (strlen(word) > strlen(longestWord))
	  strcpy(longestWord, word);
      }
    }
  }

  printf("\tWe counted %d well-formed words [including duplicates].\n", numWords);
  printf("\tThe longest word scanned was \"%s\".", longestWord);
  if (strlen(longestWord) >= 15 && (strchr(longestWord, '-') == NULL)) 
    printf(" [Ooooo... long word!]");
  printf("\n");
}

/** 
 * Function: QueryIndices
 * ----------------------
 * Standard query loop that allows the user to specify a single search term, and
 * then proceeds (via ProcessResponse) to list up to 10 articles (sorted by relevance)
 * that contain that word.
 */



static void QueryIndices()
{
  //Do StopWord Initializes
  
  char response[512];
  while (true) {
    printf("Please enter a single query term that might be in our set of indices [enter to quit]: ");
    fgets(response, sizeof(response), stdin);
    response[strlen(response) - 1] = '\0';
    if (strcasecmp(response, "") == 0) break;
    ProcessResponse(response);
  }
}

/** 
 * Function: ProcessResponse
 * -------------------------
 * Placeholder implementation for what will become the search of a set of indices
 * for a list of web documents containing the specified word.
 */

static void ProcessResponse(const char *word)
{
  void *ek = NULL;
  //HashSetMap(&eks, listEK, NULL);
  //printf("show word:%s\n",word);
  EK* cacheek = malloc(sizeof(EK));

  if (WordIsWellFormed(word)) {
    memcpy(cacheek->word , word,40*sizeof(char));
    ek = HashSetLookup(&eks, cacheek);
    if(ek == NULL)
      printf("Sorry we don't find %s\n",word);
    else
      {
        printf("\tour hashset of indices, we'd list all of the articles containing \"%s\".\n", word);
        HashSetMap(&((EK*)ek)->eas, awMapper, (void*)word);
      }
  } else {
    printf("\tWe won't be allowing words like \"%s\" into our set of indices.\n", word);
  }
}

/**
 * Predicate Function: WordIsWellFormed
 * ------------------------------------
 * Before we allow a word to be inserted into our map
 * of indices, we'd like to confirm that it's a good search term.
 * One could generalize this function to allow different criteria, but
 * this version hard codes the requirement that a word begin with 
 * a letter of the alphabet and that all letters are either letters, numbers,
 * or the '-' character.  
 */

static bool WordIsWellFormed(const char *word)
{
  int i;

  if (strlen(word) == 0) return true;
  if (!isalpha((int) word[0])) return false;
  if(strlen(word) > 39) return false; //It sure a failure word

  for (i = 1; i < strlen(word); i++)
    if (!isalnum((int) word[i]) && (word[i] != '-')) return false;
  if(HashSetLookup(&stopWord,word) != NULL) return false;

  return true;
}
